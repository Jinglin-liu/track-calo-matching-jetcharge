// Read JetChargeInputTree from a single ROOT file, wildcard, or line-delimited
// text manifest of ROOT paths. Only plot-level jet branches are read.
// Typical call from analysis/:
// root -l -b -q 'make_jetcharge_histograms.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_*.root","plots_run79516/jetcharge_histograms.root",5.0,0.7)'
//
// Each call writes 13 histograms; no event-level TTree is copied.
// Batch manifests must list one ROOT file path per line (blank/# skipped).
// Use the same cuts and identical binning in every group before hadd.

#include "JetChargePlotHelpers.h"

#include <TChain.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TNamed.h>
#include <TParameter.h>
#include <TSystem.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

bool make_jetcharge_histograms(const char* inputSpec,
                               const char* outputFile = "jetcharge_histograms.root",
                               double minChargeJetPt = 0.0,
                               double maxAbsEta = 0.7)
{
  if (!inputSpec || !*inputSpec || !outputFile || !*outputFile ||
      !std::isfinite(minChargeJetPt) || minChargeJetPt < 0.0 ||
      !std::isfinite(maxAbsEta) || maxAbsEta <= 0.0)
  {
    std::cerr << "ERROR: invalid histogram-making arguments.\n";
    return false;
  }

  TChain chain("JetChargeInputTree");
  const std::string spec(inputSpec);
  const bool manifest = spec.size() >= 4 && spec.substr(spec.size() - 4) == ".txt";
  int nFiles = 0;
  std::string firstInputPath;
  if (manifest)
  {
    std::ifstream list(spec);
    if (!list)
    {
      std::cerr << "ERROR: cannot open manifest: " << spec << '\n';
      return false;
    }
    std::set<std::string> unique;
    std::string line;
    while (std::getline(list, line))
    {
      const auto first = line.find_first_not_of(" \t\r\n");
      if (first == std::string::npos || line[first] == '#') continue;
      const auto last = line.find_last_not_of(" \t\r\n");
      const std::string path = line.substr(first, last - first + 1);
      if (!unique.insert(path).second)
      {
        std::cerr << "ERROR: duplicate path in manifest: " << path << '\n';
        return false;
      }
      if (gSystem->AccessPathName(path.c_str()))
      {
        std::cerr << "ERROR: missing ROOT input: " << path << '\n';
        return false;
      }
      if (firstInputPath.empty()) firstInputPath = path;
      nFiles += chain.Add(path.c_str());
    }
  }
  else
  {
    nFiles = chain.Add(inputSpec);
  }

  if (nFiles <= 0 || chain.LoadTree(0) < 0)
  {
    std::cerr << "ERROR: no readable JetChargeInputTree in input: " << spec << '\n';
    return false;
  }
  const char* requiredBranches[] = {
      "jet_pt", "jet_charge", "jet_eta", "jet_phi", "jet_mass",
      "jet_n_matched_tracks", "n_jets", "jet_charge_kappa"};
  for (const char* branch : requiredBranches)
  {
    if (!chain.GetBranch(branch))
    {
      std::cerr << "ERROR: required branch '" << branch << "' not found.\n";
      return false;
    }
  }

  const std::string out(outputFile);
  const auto slash = out.find_last_of('/');
  if (slash != std::string::npos && slash > 0)
  {
    const auto dir = out.substr(0, slash);
    if (gSystem->mkdir(dir.c_str(), true) != 0 &&
        gSystem->AccessPathName(dir.c_str()))
    {
      std::cerr << "ERROR: cannot create output directory: " << dir << '\n';
      return false;
    }
  }

  TH1::AddDirectory(kFALSE);
  TTreeReader reader(&chain);
  TTreeReaderValue<std::vector<float>> jetPt(reader, "jet_pt");
  TTreeReaderValue<std::vector<float>> jetCharge(reader, "jet_charge");
  TTreeReaderValue<std::vector<float>> jetEta(reader, "jet_eta");
  TTreeReaderValue<std::vector<float>> jetPhi(reader, "jet_phi");
  TTreeReaderValue<std::vector<float>> jetMass(reader, "jet_mass");
  TTreeReaderValue<std::vector<int>> nMatched(reader, "jet_n_matched_tracks");
  TTreeReaderValue<Int_t> nJets(reader, "n_jets");
  TTreeReaderValue<Float_t> chargeKappa(reader, "jet_charge_kappa");

  const JetChargePlots::JetPlotSource source = JetChargePlots::parseJetPlotSource(
      firstInputPath.empty() ? spec : firstInputPath);
  // Use identical binning for both pT distributions.
  auto hPtAll = std::make_unique<TH1D>(
      "hPtAll", "All stored jets;Jet p_{T} [GeV/c];Jets / 0.2 GeV/c", 150, 0, 30);
  auto hPtFid = std::make_unique<TH1D>(
      "hPtFid", "Fiducial jets;Jet p_{T} [GeV/c];Jets / 0.2 GeV/c", 150, 0, 30);
  auto hJetsPerEvent = std::make_unique<TH1D>(
      "hJetsPerEvent", "Stored jet multiplicity;N_{jet} per event;Events", 160, -0.5, 159.5);
  auto hEta = std::make_unique<TH1D>(
      "hEta", "Jet #eta (charge-selected p_{T});Jet #eta;Jets", 80, -1.2, 1.2);
  auto hPhi = std::make_unique<TH1D>(
      "hPhi", "Jet #phi (charge-selected p_{T});Jet #phi [rad];Jets", 72, -3.2, 3.2);
  auto hMass = std::make_unique<TH1D>(
      "hMass", "Jet mass (charge-selected jets);Jet mass [GeV/c^{2}];Jets", 100, 0, 20);

  auto hChargeAll = std::make_unique<TH1D>(
      "hChargeAll", "Jet charge including unmatched jets;Q^{#kappa};Jets", 160, -4, 4);
  auto hChargeMatched = std::make_unique<TH1D>(
      "hChargeMatched", "Jet charge with #geq1 matched track;Q^{#kappa};Jets", 160, -4, 4);
  auto hCharge2to5 = std::make_unique<TH1D>(
      "hCharge2to5", "2 #leq jet p_{T} < 5 GeV/c;Q^{#kappa};Jets", 160, -4, 4);
  auto hCharge5to10 = std::make_unique<TH1D>(
      "hCharge5to10", "5 #leq jet p_{T} < 10 GeV/c;Q^{#kappa};Jets", 160, -4, 4);
  auto hCharge10plus = std::make_unique<TH1D>(
      "hCharge10plus", "jet p_{T} #geq 10 GeV/c;Q^{#kappa};Jets", 160, -4, 4);
  auto hNMatched = std::make_unique<TH1D>(
      "hNMatched", "Track-to-jet matching;Matched tracks per jet;Jets", 21, -0.5, 20.5);
  auto hChargeVsPt = std::make_unique<TH2D>(
      "hChargeVsPt", "Jet charge vs p_{T} (matched jets);Jet p_{T} [GeV/c];Q^{#kappa}",
      100, 0, 30, 120, -4, 4);

  Long64_t eventsRead = 0;
  Long64_t jetsStored = 0;
  Long64_t jetsForCharge = 0;
  Long64_t jetsMatched = 0;
  Long64_t jetsNonfinitePt = 0;
  Long64_t jetsNonfiniteCharge = 0;
  Long64_t mismatchedVectorEvents = 0;
  Float_t observedKappa = -1.0f;


  std::cout << "Input: " << spec << '\n'
            << "Input files: " << nFiles << '\n'
            << "Tree entries: " << chain.GetEntries() << '\n'
            << "Cuts: jet pT >= " << minChargeJetPt
            << ", |eta| < " << maxAbsEta << '\n';

  while (reader.Next())
  {
    ++eventsRead;
    hJetsPerEvent->Fill(*nJets);
    if (eventsRead == 1) observedKappa = *chargeKappa;

    const std::size_t n = jetPt->size();
    jetsStored += static_cast<Long64_t>(n);
    if (jetCharge->size() != n || jetEta->size() != n ||
        jetPhi->size() != n || jetMass->size() != n ||
        nMatched->size() != n || *nJets != static_cast<int>(n))
    {
      ++mismatchedVectorEvents;
      if (mismatchedVectorEvents <= 5)
      {
        std::cerr << "WARNING: inconsistent jet-vector lengths at tree entry "
                  << (eventsRead - 1) << "; processing common elements.\n";
      }
    }

    const std::size_t nCommon = std::min({jetPt->size(), jetCharge->size(),
        jetEta->size(), jetPhi->size(), jetMass->size(), nMatched->size()});

    for (std::size_t j = 0; j < nCommon; ++j)
    {
      const double pt = jetPt->at(j);
      const double eta = jetEta->at(j);
      const double charge = jetCharge->at(j);
      const int matched = nMatched->at(j);

      if (!std::isfinite(pt) || pt < 0)
      {
        ++jetsNonfinitePt;
        continue;
      }

      hPtAll->Fill(pt);
      if (!std::isfinite(eta) || std::abs(eta) >= maxAbsEta) continue;
      hPtFid->Fill(pt);
      if (pt < minChargeJetPt) continue;
      ++jetsForCharge;

      hEta->Fill(eta);
      if (std::isfinite(jetPhi->at(j))) hPhi->Fill(jetPhi->at(j));
      if (std::isfinite(jetMass->at(j))) hMass->Fill(jetMass->at(j));
      hNMatched->Fill(matched);

      if (!std::isfinite(charge))
      {
        ++jetsNonfiniteCharge;
        continue;
      }

      hChargeAll->Fill(charge);
      if (matched <= 0) continue;
      ++jetsMatched;
      hChargeMatched->Fill(charge);
      hChargeVsPt->Fill(pt, charge);
      if (pt >= 2 && pt < 5) hCharge2to5->Fill(charge);
      else if (pt >= 5 && pt < 10) hCharge5to10->Fill(charge);
      else if (pt >= 10) hCharge10plus->Fill(charge);
    }
  }


  if (eventsRead != chain.GetEntries())
  {
    std::cerr << "ERROR: read " << eventsRead << " of "
              << chain.GetEntries() << " expected entries; histogram not saved.\n";
    return false;
  }

  TFile histFile(outputFile, "RECREATE");
  if (histFile.IsZombie())
  {
    std::cerr << "ERROR: cannot create histogram file: " << outputFile << '\n';
    return false;
  }

  hPtAll->Write(); hPtFid->Write(); hJetsPerEvent->Write();
  hEta->Write(); hPhi->Write(); hMass->Write();
  hChargeAll->Write(); hChargeMatched->Write();
  hCharge2to5->Write(); hCharge5to10->Write(); hCharge10plus->Write();
  hNMatched->Write(); hChargeVsPt->Write();

  // Informational annotations; the original production configuration is
  // not inferred from event data. The plotting selections are authoritative.
  TNamed("jetcharge_reco_mode", source.mode.c_str()).Write();
  TNamed("jetcharge_jet_cut_tag", source.jetCut.c_str()).Write();
  TNamed("jetcharge_tower_cut_tag", source.towerCut.c_str()).Write();
  TParameter<double>("jetcharge_plot_min_pt", minChargeJetPt).Write();
  TParameter<double>("jetcharge_plot_max_abs_eta", maxAbsEta).Write();
  TParameter<Long64_t>("jetcharge_events_read", eventsRead).Write();
  histFile.Close();

  std::cout << "\n========== JetChargeInputTree QA ==========\n"
            << "Events read:                  " << eventsRead << '\n'
            << "Jets stored (vector lengths): " << jetsStored << '\n'
            << "Jets with invalid/negative pT: " << jetsNonfinitePt << '\n'
            << "Charge selection (pT >= " << minChargeJetPt
            << ", |eta| < " << maxAbsEta << "): " << jetsForCharge << '\n'
            << "Selected jets with >=1 track: " << jetsMatched << '\n'
            << "Selected jets with bad charge: " << jetsNonfiniteCharge << '\n'
            << "Events with vector mismatch: " << mismatchedVectorEvents << '\n'
            << "Histograms:                  " << outputFile << '\n'
            << "NOTE: only selected jet branches were read.\n"
            << "===========================================\n";
  return true;
}
