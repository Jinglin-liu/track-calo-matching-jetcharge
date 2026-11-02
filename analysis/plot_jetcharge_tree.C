// plot_jetcharge_tree.C
//
// Plot jet pT, jet charge, and basic QA from JetChargeInputTree.
// Only the small, relevant branches are read. The large jet_component_*
// arrays (which dominate the file size) are not touched.
//
// Examples:
// root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet/jet_charge_inputs_unsubtracted_00079516_00000_00000.root")'
// root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_0GeV_sub1_00000CaloJet/jet_charge_inputs_sub1_00079516_00000_00000.root","plots_sub1",5.0,0.7)'
//
// Arguments:
//   filename       Input ROOT file
//   outputDir      Output folder for PNG, PDF and ROOT histogram file
//   minChargeJetPt Minimum jet pT [GeV/c] for jet-charge and QA plots
//   maxAbsEta      |jet eta| acceptance for jet-charge and QA plots
//
// pT spectra include all finite, nonnegative pT jets; one of them applies
// the |eta| cut. They do NOT apply minChargeJetPt, for an unbiased spectrum.
// Jet-charge plots require pT >= minChargeJetPt and |eta| < maxAbsEta.
// Charge is NaN if jet pT <= 0; unmatched jets have Q=0 when pT>0.
// Reconstruction mode is inferred from the ROOT filename (preferred) or parent
// directory: jet_charge_inputs_sub1_*.root, jet_charge_inputs_unsubtracted_*.root,
// trackcalomatch_*_sub1_*, trackcalomatch_*_unsub_*.
// Optional directory tags such as 5GeV and 0.5tower annotate plots; these are
// labels only and do not override/validate the actual production selections.

#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace
{
  struct JetPlotSource
  {
    std::string mode = "Calorimeter";
    std::string jetCut;
    std::string towerCut;
  };

  JetPlotSource parseJetPlotSource(const std::string& path)
  {
    const auto lower = [](std::string value) {
      std::transform(value.begin(), value.end(), value.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return value;
    };
    const auto slash = path.find_last_of("/\\");
    const std::string basename = lower(path.substr(slash == std::string::npos ? 0 : slash + 1));
    const std::string directory = lower(slash == std::string::npos ? "" : path.substr(0, slash));

    const auto modeOf = [](const std::string& value) {
      if (value.find("sub1") != std::string::npos) return 1;
      if (value.find("unsubtracted") != std::string::npos ||
          value.find("unsub") != std::string::npos) return 2;
      return 0;
    };
    const int fileMode = modeOf(basename);
    const int dirMode = modeOf(directory);
    if (fileMode && dirMode && fileMode != dirMode)
      std::cerr << "WARNING: jet mode in filename conflicts with directory; "
                   "using filename mode.\\n";

    JetPlotSource result;
    const int mode = fileMode ? fileMode : dirMode;
    if (mode == 1) result.mode = "Sub1";
    else if (mode == 2) result.mode = "Unsubtracted";
    else std::cerr << "WARNING: cannot determine jet mode from input path; "
                      "using generic calorimeter label.\\n";

    // Directory names are user-provided metadata, NOT verified TTree cuts.
    // Prefer tags in the parent directory to an occasional tagged file name.
    const std::regex jetRe(R"((?:^|[/_])([0-9]+(?:\\.[0-9]+)?)gev(?:[/_]|$))");
    const std::regex towerRe(R"((?:^|[/_])([0-9]+(?:\\.[0-9]+)?)tower(?:[/_]|$))");
    std::smatch match;
    if (std::regex_search(directory, match, jetRe) ||
        std::regex_search(basename, match, jetRe))
      result.jetCut = match[1].str();
    if (std::regex_search(directory, match, towerRe) ||
        std::regex_search(basename, match, towerRe))
      result.towerCut = match[1].str();
    return result;
  }
}

void plot_jetcharge_tree(const char* filename,
                         const char* outputDir = "jetcharge_plots",
                         double minChargeJetPt = 0.0,
                         double maxAbsEta = 0.7)
{
  if (!filename || !filename[0] || !outputDir || !outputDir[0] ||
      !std::isfinite(minChargeJetPt) || minChargeJetPt < 0.0 ||
      !std::isfinite(maxAbsEta) || maxAbsEta <= 0.0)
  {
    std::cerr << "ERROR: invalid arguments.\n";
    return;
  }

  std::unique_ptr<TFile> input(TFile::Open(filename, "READ"));
  if (!input || input->IsZombie())
  {
    std::cerr << "ERROR: cannot open input file: " << filename << '\n';
    return;
  }

  TTree* tree = nullptr;
  input->GetObject("JetChargeInputTree", tree);
  if (!tree)
  {
    std::cerr << "ERROR: missing TTree 'JetChargeInputTree'.\n";
    return;
  }

  const char* requiredBranches[] = {
      "jet_pt", "jet_charge", "jet_eta", "jet_phi", "jet_mass",
      "jet_n_matched_tracks", "n_jets", "jet_charge_kappa"};
  for (const char* branch : requiredBranches)
  {
    if (!tree->GetBranch(branch))
    {
      std::cerr << "ERROR: required branch '" << branch << "' not found.\n";
      return;
    }
  }

  if (gSystem->mkdir(outputDir, true) != 0 &&
      gSystem->AccessPathName(outputDir))
  {
    std::cerr << "ERROR: cannot create output directory: " << outputDir << '\n';
    return;
  }

  // With TTreeReader only the listed columns are accessed. In particular,
  // jet_component_energy/id/source/ieta/iphi are NOT read.
  TTreeReader reader(tree);
  TTreeReaderValue<std::vector<float>> jetPt(reader, "jet_pt");
  TTreeReaderValue<std::vector<float>> jetCharge(reader, "jet_charge");
  TTreeReaderValue<std::vector<float>> jetEta(reader, "jet_eta");
  TTreeReaderValue<std::vector<float>> jetPhi(reader, "jet_phi");
  TTreeReaderValue<std::vector<float>> jetMass(reader, "jet_mass");
  TTreeReaderValue<std::vector<int>> nMatched(reader, "jet_n_matched_tracks");
  TTreeReaderValue<Int_t> nJets(reader, "n_jets");
  TTreeReaderValue<Float_t> chargeKappa(reader, "jet_charge_kappa");

  const JetPlotSource source = parseJetPlotSource(filename);

  TH1::AddDirectory(kFALSE);
  gStyle->SetOptStat(0);
  gStyle->SetTitleSize(0.044, "XY");
  gStyle->SetLabelSize(0.037, "XY");

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

  std::cout << "Input: " << filename << '\n'
            << "Inferred reconstruction: " << source.mode << '\n'
            << "Directory tags (unverified): jet pT >= "
            << (source.jetCut.empty() ? "unknown" : source.jetCut)
            << ", tower pT >= "
            << (source.towerCut.empty() ? "unknown" : source.towerCut)
            << " GeV/c\\n"
            << "Tree entries: " << tree->GetEntries() << '\n'
            << "Jet-charge cuts: pT >= " << minChargeJetPt
            << " GeV/c, |eta| < " << maxAbsEta << '\n';

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

  auto plot1d = [](TH1D* hist, int color, bool logy = false) {
    hist->SetLineColor(color);
    hist->SetLineWidth(2);
    hist->SetMinimum(logy ? 0.5 : 0.0);
    if (logy) gPad->SetLogy();
    else gPad->SetLogy(false);
    hist->Draw("HIST");
  };

  // 2x2 basic jet QA: pT, eta, phi, jets/event
  auto cJetQA = std::make_unique<TCanvas>("cJetQA", "Jet kinematics and multiplicity", 1300, 980);
  cJetQA->Divide(2, 2);
  cJetQA->cd(1);
  gPad->SetLogy(true);
  hPtAll->SetLineColor(kGray + 2);
  hPtFid->SetLineColor(kBlue + 1);
  hPtAll->SetLineWidth(2);
  hPtFid->SetLineWidth(2);
  hPtAll->SetMinimum(0.5);
  const std::string ptTitle = "Anti-k_{T} R=0.4 " + source.mode +
      " jet p_{T};Jet p_{T} [GeV/c];Jets / 0.2 GeV/c";
  hPtAll->SetTitle(ptTitle.c_str());
  hPtAll->Draw("HIST");
  hPtFid->Draw("HIST SAME");
  const auto drawPathCutTags = [&source]() {
    TLatex label;
    label.SetNDC();
    label.SetTextSize(0.029);
    if (!source.jetCut.empty())
      label.DrawLatex(0.15, 0.86,
          ("Path tag: jet p_{T} #geq " + source.jetCut + " GeV/c").c_str());
    if (!source.towerCut.empty())
      label.DrawLatex(0.15, 0.80,
          ("tower p_{T} #geq " + source.towerCut + " GeV/c").c_str());
  };
  drawPathCutTags();
  TLegend legend(0.48, 0.73, 0.89, 0.88);
  legend.SetBorderSize(0);
  legend.SetFillStyle(0);
  legend.AddEntry(hPtAll.get(), "All stored jets", "l");
  legend.AddEntry(hPtFid.get(), Form("|#eta| < %.2f", maxAbsEta), "l");
  legend.Draw();

  cJetQA->cd(2);
  plot1d(hEta.get(), kBlue + 1);
  cJetQA->cd(3);
  plot1d(hPhi.get(), kBlue + 1);
  cJetQA->cd(4);
  plot1d(hJetsPerEvent.get(), kBlue + 1, true);

  // 2x2 jet-charge QA: inclusive Q, matched-track-only Q, Nmatched, Q vs pT
  auto cChargeQA = std::make_unique<TCanvas>("cChargeQA", "Jet charge QA", 1300, 980);
  cChargeQA->Divide(2, 2);
  cChargeQA->cd(1);
  hChargeAll->SetLineColor(kGray + 2);
  hChargeMatched->SetLineColor(kRed + 1);
  hChargeAll->SetLineWidth(2);
  hChargeMatched->SetLineWidth(2);
  hChargeAll->SetTitle((source.mode + " jet charge: all vs matched;Q^{#kappa};Jets").c_str());
  hChargeAll->SetMinimum(0.5);
  gPad->SetLogy(true);
  hChargeAll->Draw("HIST");
  hChargeMatched->Draw("HIST SAME");
  TLegend chargeLegend(0.51, 0.74, 0.9, 0.89);
  chargeLegend.SetBorderSize(0);
  chargeLegend.SetFillStyle(0);
  chargeLegend.AddEntry(hChargeAll.get(), "All selected jets", "l");
  chargeLegend.AddEntry(hChargeMatched.get(), "#geq 1 matched track", "l");
  chargeLegend.Draw();

  cChargeQA->cd(2);
  plot1d(hChargeMatched.get(), kRed + 1, true);
  cChargeQA->cd(3);
  plot1d(hNMatched.get(), kBlue + 1, true);
  cChargeQA->cd(4);
  gPad->SetRightMargin(0.15);
  gPad->SetLogz(false);
  hChargeVsPt->Draw("COLZ");

  // pT-binned jet-charge distributions, matched tracks only.
  auto cChargeSlices = std::make_unique<TCanvas>(
      "cChargeSlices", "Jet charge by jet pT", 1500, 520);
  cChargeSlices->Divide(3, 1);
  cChargeSlices->cd(1);
  plot1d(hCharge2to5.get(), kBlue + 1, true);
  cChargeSlices->cd(2);
  plot1d(hCharge5to10.get(), kOrange + 7, true);
  cChargeSlices->cd(3);
  plot1d(hCharge10plus.get(), kRed + 1, true);

  // Extra single-panel pT spectrum and jet-mass QA.
  auto cPt = std::make_unique<TCanvas>("cPt", "Jet pT spectrum", 900, 680);
  gPad->SetLogy();
  hPtAll->Draw("HIST");
  hPtFid->Draw("HIST SAME");
  drawPathCutTags();
  legend.Draw();

  auto cMass = std::make_unique<TCanvas>("cMass", "Jet mass", 900, 680);
  hMass->SetTitle((source.mode + " jet mass;Jet mass [GeV/c^{2}];Jets").c_str());
  plot1d(hMass.get(), kBlue + 1, true);

  struct OutputCanvas { TCanvas* canvas; const char* name; };
  const OutputCanvas outputs[] = {
      {cJetQA.get(), "jet_qa"},
      {cChargeQA.get(), "jet_charge_qa"},
      {cChargeSlices.get(), "jet_charge_pt_slices"},
      {cPt.get(), "jet_pt_spectrum"},
      {cMass.get(), "jet_mass"}};

  for (const auto& item : outputs)
  {
    const std::string path = std::string(outputDir) + "/" + item.name;
    item.canvas->SaveAs((path + ".png").c_str());
    item.canvas->SaveAs((path + ".pdf").c_str());
  }

  const std::string histPath = std::string(outputDir) + "/jetcharge_histograms.root";
  TFile histFile(histPath.c_str(), "RECREATE");
  if (!histFile.IsZombie())
  {
    hPtAll->Write(); hPtFid->Write(); hJetsPerEvent->Write();
    hEta->Write(); hPhi->Write(); hMass->Write();
    hChargeAll->Write(); hChargeMatched->Write();
    hCharge2to5->Write(); hCharge5to10->Write(); hCharge10plus->Write();
    hNMatched->Write(); hChargeVsPt->Write();
    histFile.Close();
  }
  else std::cerr << "WARNING: could not create histogram ROOT file.\n";

  std::cout << "\n========== JetChargeInputTree QA ==========\n"
            << "Events read:                  " << eventsRead << '\n'
            << "Jets stored (vector lengths): " << jetsStored << '\n'
            << "Jets with invalid/negative pT: " << jetsNonfinitePt << '\n'
            << "Charge selection (pT >= " << minChargeJetPt
            << ", |eta| < " << maxAbsEta << "): " << jetsForCharge << '\n'
            << "Selected jets with >=1 track: " << jetsMatched << '\n'
            << "Selected jets with bad charge: " << jetsNonfiniteCharge << '\n'
            << "Events with vector mismatch: " << mismatchedVectorEvents << '\n'
            << "Jet-charge kappa (first event): " << observedKappa << '\n'
            << "Outputs saved under:         " << outputDir << '\n'
            << "NOTE: hChargeMatched excludes jets with no matched tracks.\n"
            << "NOTE: Histograms have finite x-ranges; under/overflow is not drawn.\n"
            << "NOTE: Only selected branches were read, not jet_component_* arrays.\n"
            << "===========================================\n";
}
