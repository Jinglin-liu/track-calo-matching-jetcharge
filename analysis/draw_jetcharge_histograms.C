// Draw jet-charge QA from a small, already-filled histogram ROOT file.
// Unlike the maker this macro NEVER reads JetChargeInputTree.
//
// Example (from analysis/):
// root -l -b -q 'draw_jetcharge_histograms.C("plots_run79516/jetcharge_histograms.root","plots_run79516/figures")'

#include "JetChargePlotHelpers.h"
#include "AnalysisUtils.h"  // sPHENIX Internal and pp sqrt(s) label

#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TNamed.h>
#include <TParameter.h>
#include <TStyle.h>
#include <TString.h>
#include <TSystem.h>
#include <TPad.h>

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

int draw_jetcharge_histograms(const char* histogramFile,
                                const char* outputDir = "jetcharge_plots")
{
  if (!histogramFile || !*histogramFile || !outputDir || !*outputDir)
  {
    std::cerr << "ERROR: invalid input or output path.\n";
    return 1;
  }
  TFile input(histogramFile, "READ");
  if (input.IsZombie())
  {
    std::cerr << "ERROR: cannot open histogram file: " << histogramFile << '\n';
    return 1;
  }

  TH1D* hPtAll = nullptr;
  input.GetObject("hPtAll", hPtAll);
  if (!hPtAll) { std::cerr << "ERROR: missing or incorrect hPtAll in " << histogramFile << '\n'; return 1; }
  TH1D* hPtFid = nullptr;
  input.GetObject("hPtFid", hPtFid);
  if (!hPtFid) { std::cerr << "ERROR: missing or incorrect hPtFid in " << histogramFile << '\n'; return 1; }
  TH1D* hJetsPerEvent = nullptr;
  input.GetObject("hJetsPerEvent", hJetsPerEvent);
  if (!hJetsPerEvent) { std::cerr << "ERROR: missing or incorrect hJetsPerEvent in " << histogramFile << '\n'; return 1; }
  TH1D* hEta = nullptr;
  input.GetObject("hEta", hEta);
  if (!hEta) { std::cerr << "ERROR: missing or incorrect hEta in " << histogramFile << '\n'; return 1; }
  TH1D* hPhi = nullptr;
  input.GetObject("hPhi", hPhi);
  if (!hPhi) { std::cerr << "ERROR: missing or incorrect hPhi in " << histogramFile << '\n'; return 1; }
  TH1D* hMass = nullptr;
  input.GetObject("hMass", hMass);
  if (!hMass) { std::cerr << "ERROR: missing or incorrect hMass in " << histogramFile << '\n'; return 1; }
  TH1D* hChargeAll = nullptr;
  input.GetObject("hChargeAll", hChargeAll);
  if (!hChargeAll) { std::cerr << "ERROR: missing or incorrect hChargeAll in " << histogramFile << '\n'; return 1; }
  TH1D* hChargeMatched = nullptr;
  input.GetObject("hChargeMatched", hChargeMatched);
  if (!hChargeMatched) { std::cerr << "ERROR: missing or incorrect hChargeMatched in " << histogramFile << '\n'; return 1; }
  TH1D* hCharge2to5 = nullptr;
  input.GetObject("hCharge2to5", hCharge2to5);
  if (!hCharge2to5) { std::cerr << "ERROR: missing or incorrect hCharge2to5 in " << histogramFile << '\n'; return 1; }
  TH1D* hCharge5to10 = nullptr;
  input.GetObject("hCharge5to10", hCharge5to10);
  if (!hCharge5to10) { std::cerr << "ERROR: missing or incorrect hCharge5to10 in " << histogramFile << '\n'; return 1; }
  TH1D* hCharge10to15 = nullptr;
  input.GetObject("hCharge10to15", hCharge10to15);
  if (!hCharge10to15) { std::cerr << "ERROR: missing hCharge10to15 in " << histogramFile << '\n'; return 1; }
  TH1D* hCharge15plus = nullptr;
  input.GetObject("hCharge15plus", hCharge15plus);
  if (!hCharge15plus) { std::cerr << "ERROR: missing hCharge15plus in " << histogramFile << '\n'; return 1; }
  TH1D* hTrackPt = nullptr;
  input.GetObject("hTrackPt", hTrackPt);
  if (!hTrackPt) { std::cerr << "ERROR: missing hTrackPt in " << histogramFile << '\n'; return 1; }
  TH1D* hTrackCharge = nullptr;
  input.GetObject("hTrackCharge", hTrackCharge);
  if (!hTrackCharge) { std::cerr << "ERROR: missing hTrackCharge in " << histogramFile << '\n'; return 1; }
  TH1D* hMatchDeltaR = nullptr;
  input.GetObject("hMatchDeltaR", hMatchDeltaR);
  if (!hMatchDeltaR) { std::cerr << "ERROR: missing hMatchDeltaR in " << histogramFile << '\n'; return 1; }
  TH1D* hNMatched = nullptr;
  input.GetObject("hNMatched", hNMatched);
  if (!hNMatched) { std::cerr << "ERROR: missing or incorrect hNMatched in " << histogramFile << '\n'; return 1; }
  TH2D* hChargeVsPt = nullptr;
  input.GetObject("hChargeVsPt", hChargeVsPt);
  if (!hChargeVsPt) { std::cerr << "ERROR: missing or incorrect hChargeVsPt in " << histogramFile << '\n'; return 1; }
  // New constituent-geometry histograms (the maker requires geometry branches).
  TH2D* hJetEtaPhi = nullptr;
  input.GetObject("hJetEtaPhi", hJetEtaPhi);
  if (!hJetEtaPhi)
  {
    std::cerr << "ERROR: missing hJetEtaPhi. Rebuild histograms from geometry-enabled TTrees.\n";
    return 1;
  }
  const std::array<std::string, 3> caloSuffix = {{"Cemc", "Ihcal", "Ohcal"}};
  const std::array<std::string, 3> caloTitle = {{"Retowered EMCal", "IHCal", "OHCal"}};
  std::array<TH1D*, 3> hTowerEta{};
  std::array<TH1D*, 3> hTowerPhi{};
  std::array<TH2D*, 3> hTowerEtaPhi{};
  std::array<TH2D*, 3> hTowerEtaPhiEnergy{};
  for (std::size_t i = 0; i < caloSuffix.size(); ++i)
  {
    input.GetObject(("hTowerEta" + caloSuffix[i]).c_str(), hTowerEta[i]);
    input.GetObject(("hTowerPhi" + caloSuffix[i]).c_str(), hTowerPhi[i]);
    input.GetObject(("hTowerEtaPhi" + caloSuffix[i]).c_str(), hTowerEtaPhi[i]);
    input.GetObject(("hTowerEtaPhiEnergy" + caloSuffix[i]).c_str(), hTowerEtaPhiEnergy[i]);
    if (!hTowerEta[i] || !hTowerPhi[i] || !hTowerEtaPhi[i] || !hTowerEtaPhiEnergy[i])
    {
      std::cerr << "ERROR: missing constituent tower QA histograms for "
                << caloTitle[i] << "; regenerate histogram ROOT file.\n";
      return 1;
    }
  }
  JetChargePlots::JetPlotSource source = JetChargePlots::parseJetPlotSource(histogramFile);
  if (auto* mode = dynamic_cast<TNamed*>(input.Get("jetcharge_reco_mode")))
    source.mode = mode->GetTitle();
  if (auto* tag = dynamic_cast<TNamed*>(input.Get("jetcharge_jet_cut_tag")))
    source.jetCut = tag->GetTitle();
  if (auto* tag = dynamic_cast<TNamed*>(input.Get("jetcharge_tower_cut_tag")))
    source.towerCut = tag->GetTitle();

  double maxAbsEta = 0.7;
  if (auto* value = dynamic_cast<TParameter<double>*>(input.Get("jetcharge_plot_max_abs_eta")))
    maxAbsEta = value->GetVal();
  if (!std::isfinite(maxAbsEta) || maxAbsEta <= 0)
  {
    std::cerr << "ERROR: bad maxAbsEta histogram metadata.\n";
    return 1;
  }

  const std::string qaDir = std::string(outputDir) + "/QA";
  const std::string analysisDir = std::string(outputDir) + "/analysis";
  for (const auto& dir : {qaDir, analysisDir})
  {
    if (gSystem->mkdir(dir.c_str(), true) != 0 &&
        gSystem->AccessPathName(dir.c_str()))
    {
      std::cerr << "ERROR: cannot create plot directory: " << dir << '\n';
      return 1;
    }
  }

  gStyle->SetOptStat(0);
  gStyle->SetTitleSize(0.044, "XY");
  gStyle->SetLabelSize(0.037, "XY");

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
  legend.AddEntry(hPtAll, "All stored jets", "l");
  legend.AddEntry(hPtFid, Form("|#eta| < %.2f", maxAbsEta), "l");
  legend.Draw();

  cJetQA->cd(2);
  plot1d(hEta, kBlue + 1);
  auto tagJetQA = AnalysisUtils::makeSphenixTag(0.51, 0.77, 0.91, 0.92);
  tagJetQA->Draw();
  cJetQA->cd(3);
  plot1d(hPhi, kBlue + 1);
  cJetQA->cd(4);
  plot1d(hJetsPerEvent, kBlue + 1, true);

  // 2x2 jet-charge analysis overview: inclusive Q, matched Q, Nmatched, Q vs pT
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
  chargeLegend.AddEntry(hChargeAll, "All selected jets", "l");
  chargeLegend.AddEntry(hChargeMatched, "#geq 1 matched track", "l");
  chargeLegend.Draw();

  cChargeQA->cd(2);
  plot1d(hChargeMatched, kRed + 1, true);
  auto tagChargeQA = AnalysisUtils::makeSphenixTag(0.51, 0.77, 0.91, 0.92);
  tagChargeQA->Draw();
  cChargeQA->cd(3);
  plot1d(hNMatched, kBlue + 1, true);
  cChargeQA->cd(4);
  gPad->SetRightMargin(0.15);
  gPad->SetLogz(false);
  hChargeVsPt->Draw("COLZ");

  // pT-binned jet-charge distributions, matched tracks only.
  auto cChargeSlices = std::make_unique<TCanvas>(
      "cChargeSlices", "Jet charge by jet pT", 1500, 520);
  cChargeSlices->Divide(3, 1);
  cChargeSlices->cd(1);
  plot1d(hCharge5to10, kBlue + 1, true);
  cChargeSlices->cd(2);
  plot1d(hCharge10to15, kOrange + 7, true);
  cChargeSlices->cd(3);
  plot1d(hCharge15plus, kRed + 1, true);
  auto tagChargeSlices = AnalysisUtils::makeSphenixTag(0.51, 0.77, 0.91, 0.92);
  tagChargeSlices->Draw();

  // Dedicated QA figures: selected track charge, monster-track pT and
  // delta R of matched tracks in jets passing the plotting jet cuts.
  auto cTrackQA = std::make_unique<TCanvas>(
      "cTrackQA", "Selected track QA", 1500, 500);
  cTrackQA->Divide(3, 1);
  cTrackQA->cd(1);
  plot1d(hTrackCharge, kBlue + 1, true);
  cTrackQA->cd(2);
  gPad->SetLogx(true);
  plot1d(hTrackPt, kRed + 1, true);
  cTrackQA->cd(3);
  plot1d(hMatchDeltaR, kBlue + 1, false);

  // Dedicated analysis plots for matched-track multiplicity and jet charge vs pT.
  auto cMatchedTracks = std::make_unique<TCanvas>(
      "cMatchedTracks", "Associated tracks per selected jet", 900, 680);
  cMatchedTracks->cd();
  plot1d(hNMatched, kBlue + 1, true);

  auto cChargeVsPt = std::make_unique<TCanvas>(
      "cChargeVsPt", "Jet charge versus jet pT", 1000, 750);
  cChargeVsPt->cd();
  gPad->SetRightMargin(0.16);
  gPad->SetLogz(false);
  hChargeVsPt->Draw("COLZ");

  // Extra single-panel pT spectrum and jet-mass QA.
  auto cPt = std::make_unique<TCanvas>("cPt", "Jet pT spectrum", 900, 680);
  cPt->cd();
  gPad->SetLogy();
  hPtAll->Draw("HIST");
  hPtFid->Draw("HIST SAME");
  drawPathCutTags();
  legend.Draw();
  // Keep the tag clear of the upper-right legend and upper-left cut labels.
  auto tagPt = AnalysisUtils::makeSphenixTag(0.53, 0.55, 0.91, 0.69);
  tagPt->Draw();

  auto cMass = std::make_unique<TCanvas>("cMass", "Jet mass", 900, 680);
  cMass->cd();
  hMass->SetTitle((source.mode + " jet mass;Jet mass [GeV/c^{2}];Jets").c_str());
  plot1d(hMass, kBlue + 1, true);
  auto tagMass = AnalysisUtils::makeSphenixTag(0.52, 0.77, 0.91, 0.92);
  tagMass->Draw();

  // One 2x2 1D eta/phi QA canvas per calorimeter and a 3-panel
  // eta-phi occupancy/energy diagnostic. Axes are physical, vertex-corrected.
  std::array<std::unique_ptr<TCanvas>, 3> cTowerGeometry;
  std::array<std::unique_ptr<TCanvas>, 3> cTowerMaps;
  for (std::size_t i = 0; i < caloSuffix.size(); ++i)
  {
    cTowerGeometry[i] = std::make_unique<TCanvas>(
        ("cTowerGeometry" + caloSuffix[i]).c_str(),
        (caloTitle[i] + " constituent tower geometry").c_str(), 1200, 500);
    cTowerGeometry[i]->Divide(2, 1);
    cTowerGeometry[i]->cd(1);
    plot1d(hTowerEta[i], kBlue + 1);
    cTowerGeometry[i]->cd(2);
    plot1d(hTowerPhi[i], kRed + 1);

    cTowerMaps[i] = std::make_unique<TCanvas>(
        ("cTowerMaps" + caloSuffix[i]).c_str(),
        (caloTitle[i] + " constituent eta-phi maps").c_str(), 1300, 550);
    cTowerMaps[i]->Divide(2, 1);
    cTowerMaps[i]->cd(1);
    gPad->SetRightMargin(0.17);
    gPad->SetLogz(false);
    hTowerEtaPhi[i]->Draw("COLZ");
    cTowerMaps[i]->cd(2);
    gPad->SetRightMargin(0.17);
    gPad->SetLogz(false);
    hTowerEtaPhiEnergy[i]->Draw("COLZ");

    for (const auto& output : std::array<std::pair<TCanvas*, std::string>, 2>{{
             {cTowerGeometry[i].get(), "tower_geometry_1d_" + caloSuffix[i]},
             {cTowerMaps[i].get(), "tower_eta_phi_" + caloSuffix[i]}}})
    {
      const std::string path = qaDir + "/" + output.second;
      output.first->SaveAs((path + ".png").c_str());
      output.first->SaveAs((path + ".pdf").c_str());
    }
  }

  auto cJetEtaPhi = std::make_unique<TCanvas>(
      "cJetEtaPhi", "Selected jet eta-phi occupancy", 950, 720);
  cJetEtaPhi->cd();
  gPad->SetRightMargin(0.16);
  hJetEtaPhi->Draw("COLZ");
  const std::string jetMapPath = qaDir + "/jet_eta_phi";
  cJetEtaPhi->SaveAs((jetMapPath + ".png").c_str());
  cJetEtaPhi->SaveAs((jetMapPath + ".pdf").c_str());

  struct OutputCanvas { TCanvas* canvas; const char* name; const std::string* directory; };
  const OutputCanvas outputs[] = {
      {cJetQA.get(), "jet_qa", &qaDir},
      {cTrackQA.get(), "track_qa", &qaDir},
      {cPt.get(), "jet_pt_spectrum", &qaDir},
      {cMass.get(), "jet_mass", &qaDir},
      {cChargeQA.get(), "jet_charge_overview", &analysisDir},
      {cChargeSlices.get(), "jet_charge_pt_slices", &analysisDir},
      {cMatchedTracks.get(), "associated_tracks_per_jet", &analysisDir},
      {cChargeVsPt.get(), "jet_charge_vs_pt", &analysisDir}};

  for (const auto& item : outputs)
  {
    const std::string path = *item.directory + "/" + item.name;
    item.canvas->SaveAs((path + ".png").c_str());
    item.canvas->SaveAs((path + ".pdf").c_str());
  }


  std::cout << "QA figures: " << qaDir << '\n'
            << "Analysis figures: " << analysisDir << '\n';
  return 0;
}
