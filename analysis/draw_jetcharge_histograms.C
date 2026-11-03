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

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

bool draw_jetcharge_histograms(const char* histogramFile,
                                const char* outputDir = "jetcharge_plots")
{
  if (!histogramFile || !*histogramFile || !outputDir || !*outputDir)
  {
    std::cerr << "ERROR: invalid input or output path.\n";
    return false;
  }
  TFile input(histogramFile, "READ");
  if (input.IsZombie())
  {
    std::cerr << "ERROR: cannot open histogram file: " << histogramFile << '\n';
    return false;
  }

  TH1D* hPtAll = nullptr;
  input.GetObject("hPtAll", hPtAll);
  if (!hPtAll) { std::cerr << "ERROR: missing or incorrect hPtAll in " << histogramFile << '\n'; return false; }
  TH1D* hPtFid = nullptr;
  input.GetObject("hPtFid", hPtFid);
  if (!hPtFid) { std::cerr << "ERROR: missing or incorrect hPtFid in " << histogramFile << '\n'; return false; }
  TH1D* hJetsPerEvent = nullptr;
  input.GetObject("hJetsPerEvent", hJetsPerEvent);
  if (!hJetsPerEvent) { std::cerr << "ERROR: missing or incorrect hJetsPerEvent in " << histogramFile << '\n'; return false; }
  TH1D* hEta = nullptr;
  input.GetObject("hEta", hEta);
  if (!hEta) { std::cerr << "ERROR: missing or incorrect hEta in " << histogramFile << '\n'; return false; }
  TH1D* hPhi = nullptr;
  input.GetObject("hPhi", hPhi);
  if (!hPhi) { std::cerr << "ERROR: missing or incorrect hPhi in " << histogramFile << '\n'; return false; }
  TH1D* hMass = nullptr;
  input.GetObject("hMass", hMass);
  if (!hMass) { std::cerr << "ERROR: missing or incorrect hMass in " << histogramFile << '\n'; return false; }
  TH1D* hChargeAll = nullptr;
  input.GetObject("hChargeAll", hChargeAll);
  if (!hChargeAll) { std::cerr << "ERROR: missing or incorrect hChargeAll in " << histogramFile << '\n'; return false; }
  TH1D* hChargeMatched = nullptr;
  input.GetObject("hChargeMatched", hChargeMatched);
  if (!hChargeMatched) { std::cerr << "ERROR: missing or incorrect hChargeMatched in " << histogramFile << '\n'; return false; }
  TH1D* hCharge2to5 = nullptr;
  input.GetObject("hCharge2to5", hCharge2to5);
  if (!hCharge2to5) { std::cerr << "ERROR: missing or incorrect hCharge2to5 in " << histogramFile << '\n'; return false; }
  TH1D* hCharge5to10 = nullptr;
  input.GetObject("hCharge5to10", hCharge5to10);
  if (!hCharge5to10) { std::cerr << "ERROR: missing or incorrect hCharge5to10 in " << histogramFile << '\n'; return false; }
  TH1D* hCharge10plus = nullptr;
  input.GetObject("hCharge10plus", hCharge10plus);
  if (!hCharge10plus) { std::cerr << "ERROR: missing or incorrect hCharge10plus in " << histogramFile << '\n'; return false; }
  TH1D* hNMatched = nullptr;
  input.GetObject("hNMatched", hNMatched);
  if (!hNMatched) { std::cerr << "ERROR: missing or incorrect hNMatched in " << histogramFile << '\n'; return false; }
  TH2D* hChargeVsPt = nullptr;
  input.GetObject("hChargeVsPt", hChargeVsPt);
  if (!hChargeVsPt) { std::cerr << "ERROR: missing or incorrect hChargeVsPt in " << histogramFile << '\n'; return false; }
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
    return false;
  }

  if (gSystem->mkdir(outputDir, true) != 0 &&
      gSystem->AccessPathName(outputDir))
  {
    std::cerr << "ERROR: cannot create plot directory: " << outputDir << '\n';
    return false;
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
  plot1d(hCharge2to5, kBlue + 1, true);
  cChargeSlices->cd(2);
  plot1d(hCharge5to10, kOrange + 7, true);
  cChargeSlices->cd(3);
  plot1d(hCharge10plus, kRed + 1, true);
  auto tagChargeSlices = AnalysisUtils::makeSphenixTag(0.51, 0.77, 0.91, 0.92);
  tagChargeSlices->Draw();

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


  std::cout << "Figures saved under " << outputDir << '\n';
  return true;
}
