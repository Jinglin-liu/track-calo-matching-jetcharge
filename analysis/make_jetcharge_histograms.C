// Read JetChargeInputTree from a single ROOT file, wildcard, or line-delimited
// text manifest of ROOT paths. Reads jet kinematics and accepted-jet tower constituents.
// Typical call from analysis/:
// root -l -b -q 'make_jetcharge_histograms.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_*.root","plots_run79516/jetcharge_histograms.root",5.0,0.7)'
//
// Returns 0 on success, nonzero on error for ROOT batch-mode exit status.
// Each call writes 30 histograms; no event-level TTree is copied.
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
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

int make_jetcharge_histograms(const char* inputSpec,
                               const char* outputFile = "jetcharge_histograms.root",
                               double minChargeJetPt = 0.0,
                               double maxAbsEta = 0.7)
{
  if (!inputSpec || !*inputSpec || !outputFile || !*outputFile ||
      !std::isfinite(minChargeJetPt) || minChargeJetPt < 0.0 ||
      !std::isfinite(maxAbsEta) || maxAbsEta <= 0.0)
  {
    std::cerr << "ERROR: invalid histogram-making arguments.\n";
    return 1;
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
      return 1;
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
        return 1;
      }
      if (gSystem->AccessPathName(path.c_str()))
      {
        std::cerr << "ERROR: missing ROOT input: " << path << '\n';
        return 1;
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
    return 1;
  }
  const char* requiredBranches[] = {
      "jet_pt", "jet_charge", "jet_eta", "jet_phi", "jet_mass",
      "track_pt", "track_charge", "n_tracks",
      "jet_track_offset", "jet_track_delta_r",
      "jet_n_matched_tracks", "n_jets", "jet_charge_kappa",
      "jet_component_offset", "jet_component_source", "jet_component_energy",
      "jet_component_eta", "jet_component_phi"};
  for (const char* branch : requiredBranches)
  {
    if (!chain.GetBranch(branch))
    {
      std::cerr << "ERROR: required branch '" << branch << "' not found.\n";
      return 1;
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
      return 1;
    }
  }

  // Query total entries before attaching TTreeReader to avoid changing
  // the chain's current tree behind the reader (SetEntryBase warning).
  const Long64_t expectedEntries = chain.GetEntries();
  TH1::AddDirectory(kFALSE);
  TTreeReader reader(&chain);
  TTreeReaderValue<std::vector<float>> trackPt(reader, "track_pt");
  TTreeReaderValue<std::vector<int>> trackCharge(reader, "track_charge");
  TTreeReaderValue<Int_t> nTracks(reader, "n_tracks");
  TTreeReaderValue<std::vector<unsigned int>> trackMatchOffset(reader, "jet_track_offset");
  TTreeReaderValue<std::vector<float>> trackMatchDeltaR(reader, "jet_track_delta_r");
  TTreeReaderValue<std::vector<float>> jetPt(reader, "jet_pt");
  TTreeReaderValue<std::vector<float>> jetCharge(reader, "jet_charge");
  TTreeReaderValue<std::vector<float>> jetEta(reader, "jet_eta");
  TTreeReaderValue<std::vector<float>> jetPhi(reader, "jet_phi");
  TTreeReaderValue<std::vector<float>> jetMass(reader, "jet_mass");
  TTreeReaderValue<std::vector<int>> nMatched(reader, "jet_n_matched_tracks");
  TTreeReaderValue<Int_t> nJets(reader, "n_jets");
  TTreeReaderValue<Float_t> chargeKappa(reader, "jet_charge_kappa");
  TTreeReaderValue<std::vector<unsigned int>> componentOffset(reader, "jet_component_offset");
  TTreeReaderValue<std::vector<int>> componentSource(reader, "jet_component_source");
  TTreeReaderValue<std::vector<float>> componentEnergy(reader, "jet_component_energy");
  TTreeReaderValue<std::vector<float>> componentEta(reader, "jet_component_eta");
  TTreeReaderValue<std::vector<float>> componentPhi(reader, "jet_component_phi");

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
  auto hCharge10to15 = std::make_unique<TH1D>(
      "hCharge10to15", "10 #leq jet p_{T} < 15 GeV/c;Q^{#kappa};Jets", 160, -4, 4);
  auto hCharge15plus = std::make_unique<TH1D>(
      "hCharge15plus", "jet p_{T} #geq 15 GeV/c;Q^{#kappa};Jets", 160, -4, 4);
  auto hNMatched = std::make_unique<TH1D>(
      "hNMatched", "Track-to-jet matching;Matched tracks per jet;Jets", 21, -0.5, 20.5);
  auto hChargeVsPt = std::make_unique<TH2D>(
      "hChargeVsPt", "Jet charge vs p_{T} (matched jets);Jet p_{T} [GeV/c];Q^{#kappa}",
      100, 0, 30, 120, -4, 4);

  // Event-level selected-track QA: track pT uses logarithmic bin edges to
  // retain sensitivity to rare anomalously high-pT ("monster") tracks.
  constexpr int nTrackPtBins = 160;
  std::array<double, nTrackPtBins + 1> trackPtEdges{};
  for (int i = 0; i <= nTrackPtBins; ++i)
    trackPtEdges[i] = 0.5 * std::pow(20000.0, static_cast<double>(i) / nTrackPtBins);
  auto hTrackPt = std::make_unique<TH1D>(
      "hTrackPt", "Selected track p_{T};Track p_{T} [GeV/c];Tracks",
      nTrackPtBins, trackPtEdges.data());
  auto hTrackCharge = std::make_unique<TH1D>(
      "hTrackCharge", "Selected track charge;Track charge [e];Tracks",
      7, -3.5, 3.5);
  auto hMatchDeltaR = std::make_unique<TH1D>(
      "hMatchDeltaR", "Matched tracks in selected jets;#DeltaR(track, jet);Matched tracks",
      100, 0.0, 0.5);

  // Tower maps include only constituents belonging to jets passing the same
  // pT/eta selection as hEta/hPhi. One tower may appear in different jets/events.
  // Bin ranges cover the calorimeter acceptance, not just the jet-axis cut.
  constexpr double pi = 3.14159265358979323846;
  auto hJetEtaPhi = std::make_unique<TH2D>(
      "hJetEtaPhi", "Selected jet axes;Jet #eta;Jet #phi [rad]",
      96, -1.2, 1.2, 72, -pi, pi);
  const std::array<std::string, 3> caloSuffix = {{"Cemc", "Ihcal", "Ohcal"}};
  const std::array<std::string, 3> caloTitle = {{"Retowered EMCal", "IHCal", "OHCal"}};
  std::array<std::unique_ptr<TH1D>, 3> hTowerEta;
  std::array<std::unique_ptr<TH1D>, 3> hTowerPhi;
  std::array<std::unique_ptr<TH2D>, 3> hTowerEtaPhi;
  std::array<std::unique_ptr<TH2D>, 3> hTowerEtaPhiEnergy;
  for (std::size_t i = 0; i < caloSuffix.size(); ++i)
  {
    hTowerEta[i] = std::make_unique<TH1D>(
        ("hTowerEta" + caloSuffix[i]).c_str(),
        (caloTitle[i] + " jet-constituent tower #eta;Tower #eta;Constituents").c_str(),
        96, -1.2, 1.2);
    hTowerPhi[i] = std::make_unique<TH1D>(
        ("hTowerPhi" + caloSuffix[i]).c_str(),
        (caloTitle[i] + " jet-constituent tower #phi;Tower #phi [rad];Constituents").c_str(),
        72, -pi, pi);
    hTowerEtaPhi[i] = std::make_unique<TH2D>(
        ("hTowerEtaPhi" + caloSuffix[i]).c_str(),
        (caloTitle[i] + " jet-constituent occupancy;Tower #eta;Tower #phi [rad]").c_str(),
        96, -1.2, 1.2, 72, -pi, pi);
    hTowerEtaPhiEnergy[i] = std::make_unique<TH2D>(
        ("hTowerEtaPhiEnergy" + caloSuffix[i]).c_str(),
        (caloTitle[i] + " constituent energy sum [GeV];Tower #eta;Tower #phi [rad]").c_str(),
        96, -1.2, 1.2, 72, -pi, pi);
  }

  Long64_t eventsRead = 0;
  Long64_t jetsStored = 0;
  Long64_t jetsForCharge = 0;
  Long64_t jetsMatched = 0;
  Long64_t jetsNonfinitePt = 0;
  Long64_t jetsNonfiniteCharge = 0;
  Long64_t mismatchedVectorEvents = 0;
  Long64_t towerConstituentsSelected[3] = {0, 0, 0};
  Long64_t towerConstituentsInvalidGeometry = 0;
  Long64_t towerConstituentsUnknownSource = 0;
  Long64_t nonfiniteTrackPt = 0;
  double highestTrackPt = 0.;
  Float_t observedKappa = -1.0f;


  std::cout << "Input: " << spec << '\n'
            << "Input files: " << nFiles << '\n'
            << "Tree entries: " << expectedEntries << '\n'
            << "Cuts: jet pT >= " << minChargeJetPt
            << ", |eta| < " << maxAbsEta << '\n';

  while (reader.Next())
  {
    ++eventsRead;

    if (eventsRead % 50000 == 0)
    {
    std::cout
        << "Processed " << eventsRead
        << " / " << expectedEntries
        << " events ("
        << 100.0 * eventsRead / expectedEntries
        << "%), stored jets: " << jetsStored
        << std::endl;
    }

    hJetsPerEvent->Fill(*nJets);
    if (eventsRead == 1) observedKappa = *chargeKappa;

    if (trackPt->size() != trackCharge->size() ||
        trackPt->size() != static_cast<std::size_t>(*nTracks))
    {
      std::cerr << "ERROR: inconsistent selected track arrays at entry "
                << (eventsRead - 1) << '\n';
      return 1;
    }
    for (std::size_t t = 0; t < trackPt->size(); ++t)
    {
      const double pt = trackPt->at(t);
      if (!std::isfinite(pt) || pt < 0.)
      {
        ++nonfiniteTrackPt;
        continue;
      }
      highestTrackPt = std::max(highestTrackPt, pt);
      hTrackPt->Fill(pt);
      hTrackCharge->Fill(trackCharge->at(t));
    }

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

    // The flattened arrays must align with the jet-by-jet offsets.
    const std::size_t nComponents = componentSource->size();
    const bool aligned =
        componentOffset->size() == n + 1 &&
        componentEnergy->size() == nComponents &&
        componentEta->size() == nComponents &&
        componentPhi->size() == nComponents;
    const bool alignedMatches =
        trackMatchOffset->size() == n + 1 &&
        !trackMatchOffset->empty() &&
        trackMatchOffset->front() == 0 &&
        trackMatchOffset->back() == trackMatchDeltaR->size() &&
        std::is_sorted(trackMatchOffset->begin(), trackMatchOffset->end());
    if (!alignedMatches)
    {
      std::cerr << "ERROR: malformed matched-track offsets at entry "
                << (eventsRead - 1) << '\n';
      return 1;
    }
    if (!aligned || componentOffset->front() != 0 ||
        componentOffset->back() != nComponents ||
        !std::is_sorted(componentOffset->begin(), componentOffset->end()))
    {
      std::cerr << "ERROR: malformed jet constituent arrays at tree entry "
                << (eventsRead - 1) << "; aborting to avoid incomplete tower QA.\n";
      return 1;
    }

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
      if (std::isfinite(jetPhi->at(j)))
      {
        hPhi->Fill(jetPhi->at(j));
        hJetEtaPhi->Fill(eta, jetPhi->at(j));
      }
      if (std::isfinite(jetMass->at(j))) hMass->Fill(jetMass->at(j));
      hNMatched->Fill(matched);

      const auto matchBegin = trackMatchOffset->at(j);
      const auto matchEnd = trackMatchOffset->at(j + 1);
      if (matchEnd - matchBegin != static_cast<std::size_t>(matched))
      {
        std::cerr << "ERROR: matched-track count/offset mismatch at entry "
                  << (eventsRead - 1) << " jet " << j << '\n';
        return 1;
      }
      for (std::size_t m = matchBegin; m < matchEnd; ++m)
      {
        const double deltaR = trackMatchDeltaR->at(m);
        if (!std::isfinite(deltaR) || deltaR < 0.)
        {
          std::cerr << "ERROR: invalid matched-track delta R at entry "
                    << (eventsRead - 1) << '\n';
          return 1;
        }
        hMatchDeltaR->Fill(deltaR);
      }

      for (std::size_t c = componentOffset->at(j);
           c < componentOffset->at(j + 1); ++c)
      {
        // Jet::SRC numeric values in sPHENIX jetbase/Jet.h.
        // TowerInfo inputs: unsubtracted 28/26/27, Sub1 29/30/31.
        int layer = -1;
        switch (componentSource->at(c))
        {
          case 28: case 29: layer = 0; break;  // Retowered CEMC
          case 26: case 30: layer = 1; break;  // IHCal
          case 27: case 31: layer = 2; break;  // OHCal
          default: break;
        }
        if (layer < 0)
        {
          if (++towerConstituentsUnknownSource <= 5)
            std::cerr << "WARNING: unexpected constituent source "
                      << componentSource->at(c) << " at event "
                      << (eventsRead - 1) << '\n';
          continue;
        }

        const double towerEta = componentEta->at(c);
        const double towerPhi = componentPhi->at(c);
        if (!std::isfinite(towerEta) || !std::isfinite(towerPhi))
        {
          ++towerConstituentsInvalidGeometry;
          continue;
        }
        ++towerConstituentsSelected[layer];
        hTowerEta[layer]->Fill(towerEta);
        hTowerPhi[layer]->Fill(towerPhi);
        hTowerEtaPhi[layer]->Fill(towerEta, towerPhi);

        const double energy = componentEnergy->at(c);
        if (std::isfinite(energy))
          hTowerEtaPhiEnergy[layer]->Fill(towerEta, towerPhi, energy);
      }

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
      else if (pt >= 10 && pt < 15) hCharge10to15->Fill(charge);
      else if (pt >= 15) hCharge15plus->Fill(charge);
    }
  }


  if (eventsRead != expectedEntries)
  {
    std::cerr << "ERROR: read " << eventsRead << " of "
              << expectedEntries << " expected entries; histogram not saved.\n";
    return 1;
  }

  TFile histFile(outputFile, "RECREATE");
  if (histFile.IsZombie())
  {
    std::cerr << "ERROR: cannot create histogram file: " << outputFile << '\n';
    return 1;
  }

  hPtAll->Write(); hPtFid->Write(); hJetsPerEvent->Write();
  hEta->Write(); hPhi->Write(); hMass->Write();
  hChargeAll->Write(); hChargeMatched->Write();
  hCharge2to5->Write(); hCharge5to10->Write();
  hCharge10to15->Write(); hCharge15plus->Write();
  hTrackPt->Write(); hTrackCharge->Write(); hMatchDeltaR->Write();
  hNMatched->Write(); hChargeVsPt->Write();
  hJetEtaPhi->Write();
  for (std::size_t layer = 0; layer < caloSuffix.size(); ++layer)
  {
    hTowerEta[layer]->Write();
    hTowerPhi[layer]->Write();
    hTowerEtaPhi[layer]->Write();
    hTowerEtaPhiEnergy[layer]->Write();
  }

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
            << "Selected CEMC/IHCal/OHCal constituents: "
            << towerConstituentsSelected[0] << " / "
            << towerConstituentsSelected[1] << " / "
            << towerConstituentsSelected[2] << '\n'
            << "Constituents with invalid geometry: "
            << towerConstituentsInvalidGeometry << '\n'
            << "Selected tracks with bad pT:     " << nonfiniteTrackPt << '\n'
            << "Maximum selected track pT:       " << highestTrackPt << " GeV/c\n"
            << "Track pT histogram overflow:     " << hTrackPt->GetBinContent(nTrackPtBins + 1) << '\n'
            << "Constituents with unexpected source: "
            << towerConstituentsUnknownSource << '\n'
            << "Histograms:                  " << outputFile << '\n'
            << "NOTE: tower QA uses constituents of pT/eta-selected jets only.\n"
            << "===========================================\n";
  return 0;
}
