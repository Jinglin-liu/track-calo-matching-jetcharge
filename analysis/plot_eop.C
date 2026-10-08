#include "AnalysisUtils.h"

#include <TCanvas.h>
#include <TF1.h>
#include <TGaxis.h>
#include <TH1F.h>
#include <TH2D.h>
#include <TH2F.h>
#include <TLegend.h>
#include <TMath.h>
#include <TPaveText.h>
#include <TStyle.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace
{
struct FitResult
{
  double epMean = std::numeric_limits<double>::quiet_NaN();
  double emMean = std::numeric_limits<double>::quiet_NaN();
  bool valid = false;
};

FitResult fitChargePair(TH1 &epHistogram, TH1 &emHistogram, const double epFitMin, const double epFitMax,
                        const double emFitMin, const double emFitMax, const TString &binLabel, const TString &output)
{
  TF1 epFit(Form("fit_%s", epHistogram.GetName()), "gaus", epFitMin, epFitMax);
  TF1 emFit(Form("fit_%s", emHistogram.GetName()), "gaus", emFitMin, emFitMax);

  const bool enoughEntries = epHistogram.GetEntries() >= 10 && emHistogram.GetEntries() >= 10;
  int epStatus = -1;
  int emStatus = -1;
  if (enoughEntries)
  {
    epStatus = epHistogram.Fit(&epFit, "Q0RN", "", epFitMin, epFitMax);
    emStatus = emHistogram.Fit(&emFit, "Q0RN", "", emFitMin, emFitMax);
  }

  FitResult result;
  result.valid = enoughEntries && epStatus == 0 && emStatus == 0;
  if (result.valid)
  {
    result.epMean = epFit.GetParameter(1);
    result.emMean = emFit.GetParameter(1);
  }

  TCanvas canvas(Form("canvas_%s", epHistogram.GetName()), "", 800, 600);
  canvas.SetRightMargin(0.05);
  epHistogram.SetLineColor(kRed);
  emHistogram.SetLineColor(kBlue);
  epHistogram.SetMinimum(0.0);
  epHistogram.SetMaximum(1.15 * std::max({1.0, epHistogram.GetMaximum(), emHistogram.GetMaximum()}));
  epHistogram.Draw("HIST");
  emHistogram.Draw("HIST SAME");
  if (result.valid)
  {
    epFit.SetLineColor(kRed + 2);
    emFit.SetLineColor(kBlue + 2);
    epFit.Draw("SAME");
    emFit.Draw("SAME");
  }

  TLegend legend(0.18, 0.62, 0.34, 0.74);
  legend.AddEntry(&epHistogram, "e^{+}", "l");
  legend.AddEntry(&emHistogram, "e^{-}", "l");
  legend.Draw();

  TPaveText information(0.48, 0.62, 0.90, 0.82, "NDC");
  information.SetFillStyle(0);
  information.SetBorderSize(0);
  information.SetTextSize(0.028);
  information.AddText(binLabel);
  if (result.valid)
  {
    information.AddText(Form("e^{+}: #mu = %.3f #pm %.3f", result.epMean, epFit.GetParError(1)));
    information.AddText(Form("e^{-}: #mu = %.3f #pm %.3f", result.emMean, emFit.GetParError(1)));
  }
  else
  {
    information.AddText("Fit skipped or failed");
  }
  information.Draw();
  auto sphenixTag = AnalysisUtils::makeSphenixTag(0.16, 0.82, 0.49, 0.94);
  sphenixTag->Draw();
  canvas.SaveAs(output);
  return result;
}

void saveMap(TH2 &histogram, const TString &output, const double minimum = std::numeric_limits<double>::quiet_NaN(),
             const double maximum = std::numeric_limits<double>::quiet_NaN())
{
  TCanvas canvas(Form("canvas_%s", histogram.GetName()), "", 800, 600);
  canvas.SetLeftMargin(0.11);
  canvas.SetRightMargin(0.15);
  histogram.GetYaxis()->SetTitleOffset(0.75);
  if (std::isfinite(minimum))
  {
    histogram.SetMinimum(minimum);
  }
  if (std::isfinite(maximum))
  {
    histogram.SetMaximum(maximum);
  }
  histogram.Draw("COLZ");
  auto sphenixTag = AnalysisUtils::makeSphenixTag();
  sphenixTag->Draw();
  canvas.SaveAs(output);
}
void plotEtaPhi(TChain &chain, const TString &outputDirectory, const bool isEop)
{
  constexpr int nPhiBins = 5;
  constexpr int nEtaBins = 5;

  const TString prefix = isEop ? "eop" : "eopCorr";
  const TString xTitle = isEop ? "charge sign #times E_{cluster}/p" : "q/p_{T}-q/(E_{cluster} sin#theta) [GeV^{-1}]";
  const TString epExpression = isEop ? "ep_emcal_e/(ep_pt/sin(ep_theta))" : "1/ep_pt-1/(ep_emcal_e*sin(ep_theta))";
  const TString emExpression = isEop ? "-em_emcal_e/(em_pt/sin(em_theta))" : "-1/em_pt+1/(em_emcal_e*sin(em_theta))";
  const double histogramMin = isEop ? -2.0 : -1.0;
  const double histogramMax = isEop ? 2.0 : 1.0;
  const double epFitMin = isEop ? 0.2 : -0.5;
  const double epFitMax = isEop ? 1.6 : 0.5;
  const double emFitMin = isEop ? -1.6 : -0.5;
  const double emFitMax = isEop ? -0.2 : 0.5;

  constexpr double etaMin = -1.0;
  constexpr double etaMax = 1.0;
  const double phiMin = -TMath::Pi();
  const double phiMax = TMath::Pi();
  const double etaBinWidth = (etaMax - etaMin) / nEtaBins;
  const double phiBinWidth = (phiMax - phiMin) / nPhiBins;

  TH2D epCounts(Form("h2_%s_counts_ep", prefix.Data()), ";#eta;#phi;selected e^{+}", nEtaBins, etaMin, etaMax, nPhiBins,
                phiMin, phiMax);
  TH2D emCounts(Form("h2_%s_counts_em", prefix.Data()), ";#eta;#phi;selected e^{-}", nEtaBins, etaMin, etaMax, nPhiBins,
                phiMin, phiMax);
  TH2D epMean(Form("h2_%s_mean_ep", prefix.Data()), Form(";#eta;#phi;mean %s (e^{+})", xTitle.Data()), nEtaBins, etaMin,
              etaMax, nPhiBins, phiMin, phiMax);
  TH2D emMean(Form("h2_%s_mean_em", prefix.Data()), Form(";#eta;#phi;mean %s (e^{-})", xTitle.Data()), nEtaBins, etaMin,
              etaMax, nPhiBins, phiMin, phiMax);

  std::vector<std::unique_ptr<TH1F>> epHistograms;
  std::vector<std::unique_ptr<TH1F>> emHistograms;
  epHistograms.reserve(nPhiBins * nEtaBins);
  emHistograms.reserve(nPhiBins * nEtaBins);

  for (int phiBin = 0; phiBin < nPhiBins; ++phiBin)
  {
    for (int etaBin = 0; etaBin < nEtaBins; ++etaBin)
    {
      const double lowPhi = phiMin + phiBin * phiBinWidth;
      const double highPhi = lowPhi + phiBinWidth;
      const double lowEta = etaMin + etaBin * etaBinWidth;
      const double highEta = lowEta + etaBinWidth;

      epHistograms.push_back(std::make_unique<TH1F>(Form("h_%s_ep_p%d_e%d", prefix.Data(), phiBin, etaBin),
                                                    Form(";%s;Candidates", xTitle.Data()), 50, histogramMin,
                                                    histogramMax));
      emHistograms.push_back(std::make_unique<TH1F>(Form("h_%s_em_p%d_e%d", prefix.Data(), phiBin, etaBin),
                                                    Form(";%s;Candidates", xTitle.Data()), 50, histogramMin,
                                                    histogramMax));
      TH1F &epHistogram = *epHistograms.back();
      TH1F &emHistogram = *emHistograms.back();

      const TCut epBinCut =
          Form("ep_phi>=%g && ep_phi<%g && ep_eta>=%g && ep_eta<%g", lowPhi, highPhi, lowEta, highEta);
      const TCut emBinCut =
          Form("em_phi>=%g && em_phi<%g && em_eta>=%g && em_eta<%g", lowPhi, highPhi, lowEta, highEta);
      chain.Draw(epExpression + ">>" + epHistogram.GetName(), epBinCut, "goff");
      chain.Draw(emExpression + ">>" + emHistogram.GetName(), emBinCut, "goff");

      const TString binLabel = Form("%.2f #leq #phi < %.2f, %.2f #leq #eta < %.2f", lowPhi, highPhi, lowEta, highEta);
      const TString fitOutput =
          AnalysisUtils::outputPath(outputDirectory, Form("epem_%s_p%d_e%d.pdf", prefix.Data(), phiBin, etaBin));
      const FitResult result =
          fitChargePair(epHistogram, emHistogram, epFitMin, epFitMax, emFitMin, emFitMax, binLabel, fitOutput);

      epCounts.SetBinContent(etaBin + 1, phiBin + 1, epHistogram.GetEntries());
      emCounts.SetBinContent(etaBin + 1, phiBin + 1, emHistogram.GetEntries());
      if (result.valid)
      {
        epMean.SetBinContent(etaBin + 1, phiBin + 1, result.epMean);
        emMean.SetBinContent(etaBin + 1, phiBin + 1, result.emMean);
      }
    }
  }

  auto correction = std::unique_ptr<TH2D>(static_cast<TH2D *>(epMean.Clone(Form("h2_%s_correction", prefix.Data()))));
  correction->SetTitle(Form(";#eta;#phi;charge-average of mean %s", xTitle.Data()));
  correction->Add(&emMean);
  correction->Scale(0.5);

  saveMap(epMean, AnalysisUtils::outputPath(outputDirectory, Form("h2_%s_mean_ep.pdf", prefix.Data())));
  saveMap(emMean, AnalysisUtils::outputPath(outputDirectory, Form("h2_%s_mean_em.pdf", prefix.Data())));
  saveMap(*correction, AnalysisUtils::outputPath(outputDirectory, Form("h2_%s_correction.pdf", prefix.Data())));
  saveMap(epCounts, AnalysisUtils::outputPath(outputDirectory, Form("h2_%s_counts_ep.pdf", prefix.Data())));
  saveMap(emCounts, AnalysisUtils::outputPath(outputDirectory, Form("h2_%s_counts_em.pdf", prefix.Data())));

  if (isEop)
  {
    auto chargeRatio = std::unique_ptr<TH2D>(static_cast<TH2D *>(emMean.Clone("h2_eop_charge_ratio")));
    chargeRatio->SetTitle(";#eta;#phi;-#mu(e^{-})/#mu(e^{+})");
    for (int etaBin = 1; etaBin <= nEtaBins; ++etaBin)
    {
      for (int phiBin = 1; phiBin <= nPhiBins; ++phiBin)
      {
        const double denominator = epMean.GetBinContent(etaBin, phiBin);
        const double numerator = emMean.GetBinContent(etaBin, phiBin);
        chargeRatio->SetBinContent(etaBin, phiBin, denominator != 0.0 ? -numerator / denominator : 0.0);
      }
    }
    saveMap(*chargeRatio, AnalysisUtils::outputPath(outputDirectory, "h2_eop_charge_ratio.pdf"), 0.6, 1.4);
  }
}

void plotOverview(TChain &chain, const TString &outputDirectory)
{
  TH2F epComparison("h_eop2d_ep_comparison", ";1/(E_{cluster} sin#theta) [GeV^{-1}];+1/p_{T} [GeV^{-1}];e^{+}", 40, 0.0,
                    2.0, 40, 0.0, 2.0);
  AnalysisUtils::plot2D(&epComparison, chain, "1/ep_pt:1/(ep_emcal_e*sin(ep_theta))",
                        AnalysisUtils::outputPath(outputDirectory, "ep_eop2D.pdf"));

  TH2F emComparison("h_eop2d_em_comparison", ";1/(E_{cluster} sin#theta) [GeV^{-1}];-1/p_{T} [GeV^{-1}];e^{-}", 40, 0.0,
                    2.0, 40, -2.0, 0.0);
  AnalysisUtils::plot2D(&emComparison, chain, "-1/em_pt:1/(em_emcal_e*sin(em_theta))",
                        AnalysisUtils::outputPath(outputDirectory, "em_eop2D.pdf"));

  TH1F epResidual("h_eop2d_ep_residual", ";+1/p_{T}-1/(E_{cluster} sin#theta) [GeV^{-1}];e^{+}", 50, -1.0, 1.0);
  AnalysisUtils::plot1D(&epResidual, chain, "1/ep_pt-1/(ep_emcal_e*sin(ep_theta))",
                        AnalysisUtils::outputPath(outputDirectory, "ep_eop1D.pdf"));

  TH1F emResidual("h_eop2d_em_residual", ";-1/p_{T}+1/(E_{cluster} sin#theta) [GeV^{-1}];e^{-}", 50, -1.0, 1.0);
  AnalysisUtils::plot1D(&emResidual, chain, "-1/em_pt+1/(em_emcal_e*sin(em_theta))",
                        AnalysisUtils::outputPath(outputDirectory, "em_eop1D.pdf"));

  TH1F epCurvature("h_eop2d_ep_curvature", ";+1/p_{T} [GeV^{-1}];e^{+}", 50, 0.0, 5.0);
  AnalysisUtils::plot1D(&epCurvature, chain, "1/ep_pt", AnalysisUtils::outputPath(outputDirectory, "ep_kappa.pdf"));

  TH1F emCurvature("h_eop2d_em_curvature", ";-1/p_{T} [GeV^{-1}];e^{-}", 50, -5.0, 0.0);
  AnalysisUtils::plot1D(&emCurvature, chain, "-1/em_pt", AnalysisUtils::outputPath(outputDirectory, "em_kappa.pdf"));
}
} // namespace

/**
 * Draw the inclusive E/p comparisons and both eta-phi studies.
 */
void plot_eop(const char *inputFile = "../root/ee_reco_skim.root", const char *outputDirectory = "figure")
{
  gStyle->SetOptStat(0);
  gStyle->SetPalette(kRainBow);
  TGaxis::SetMaxDigits(3);
  AnalysisUtils::prepareOutputDirectory(outputDirectory);

  TChain chain("DecayTree");
  if (!AnalysisUtils::addInput(chain, inputFile))
  {
    return;
  }

  plotOverview(chain, outputDirectory);
  plotEtaPhi(chain, outputDirectory, true);
  plotEtaPhi(chain, outputDirectory, false);
}
