#include "AnalysisUtils.h"

#include <TCanvas.h>
#include <TF1.h>
#include <TGaxis.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TLegend.h>
#include <TPaveText.h>
#include <TStyle.h>

#include <algorithm>

namespace
{
void plotChargeOrderedEOverP(TChain &chain, const TCut &baseCut, const TString &output)
{
  TH1F epEOverP("h_conversion_ep_e_over_p", ";E_{cluster}/p;Candidates", 100, 0.0, 2.0);
  TH1F emEOverP("h_conversion_em_e_over_p", ";E_{cluster}/p;Candidates", 100, 0.0, 2.0);

  chain.Draw("track_1_EMCAL_energy_cluster/track_1_p>>h_conversion_ep_e_over_p",
             baseCut + "track_1_charge==1 && track_2_charge==-1", "goff");
  chain.Draw("track_2_EMCAL_energy_cluster/track_2_p>>+h_conversion_ep_e_over_p",
             baseCut + "track_2_charge==1 && track_1_charge==-1", "goff");
  chain.Draw("track_1_EMCAL_energy_cluster/track_1_p>>h_conversion_em_e_over_p",
             baseCut + "track_1_charge==-1 && track_2_charge==1", "goff");
  chain.Draw("track_2_EMCAL_energy_cluster/track_2_p>>+h_conversion_em_e_over_p",
             baseCut + "track_2_charge==-1 && track_1_charge==1", "goff");

  constexpr double fitMin = 0.6;
  constexpr double fitMax = 1.4;
  TF1 epFit("fit_conversion_ep_e_over_p", "gaus", fitMin, fitMax);
  TF1 emFit("fit_conversion_em_e_over_p", "gaus", fitMin, fitMax);
  const bool fitEp = epEOverP.GetEntries() >= 10 && epEOverP.Fit(&epFit, "Q0RN", "", fitMin, fitMax) == 0;
  const bool fitEm = emEOverP.GetEntries() >= 10 && emEOverP.Fit(&emFit, "Q0RN", "", fitMin, fitMax) == 0;

  TCanvas canvas("canvas_conversion_ep_em_e_over_p", "", 800, 600);
  epEOverP.SetLineColor(kRed);
  emEOverP.SetLineColor(kBlue);
  epEOverP.SetLineWidth(2);
  emEOverP.SetLineWidth(2);
  epEOverP.SetMinimum(0.0);
  epEOverP.SetMaximum(1.20 * std::max({1.0, epEOverP.GetMaximum(), emEOverP.GetMaximum()}));
  epEOverP.Draw("HIST");
  emEOverP.Draw("HIST SAME");

  if (fitEp)
  {
    epFit.SetLineColor(kRed + 2);
    epFit.SetLineWidth(2);
    epFit.Draw("SAME");
  }
  if (fitEm)
  {
    emFit.SetLineColor(kBlue + 2);
    emFit.SetLineWidth(2);
    emFit.Draw("SAME");
  }

  auto sphenixTag = AnalysisUtils::makeSphenixTag(0.16, 0.82, 0.49, 0.94);
  sphenixTag->Draw();

  TLegend legend(0.18, 0.64, 0.34, 0.76);
  legend.AddEntry(&epEOverP, "e^{+}", "l");
  legend.AddEntry(&emEOverP, "e^{-}", "l");
  legend.Draw();

  TPaveText fitInformation(0.51, 0.66, 0.88, 0.88, "NDC");
  fitInformation.SetFillStyle(0);
  fitInformation.SetBorderSize(0);
  fitInformation.SetTextAlign(12);
  fitInformation.SetTextSize(0.032);
  if (fitEp)
  {
    fitInformation.AddText(Form("e^{+}: #mu = %.3f #pm %.3f", epFit.GetParameter(1), epFit.GetParError(1)));
    fitInformation.AddText(Form("e^{+}: #sigma = %.3f #pm %.3f", epFit.GetParameter(2), epFit.GetParError(2)));
  }
  else
  {
    fitInformation.AddText("e^{+}: fit skipped or failed");
  }
  if (fitEm)
  {
    fitInformation.AddText(Form("e^{-}: #mu = %.3f #pm %.3f", emFit.GetParameter(1), emFit.GetParError(1)));
    fitInformation.AddText(Form("e^{-}: #sigma = %.3f #pm %.3f", emFit.GetParameter(2), emFit.GetParError(2)));
  }
  else
  {
    fitInformation.AddText("e^{-}: fit skipped or failed");
  }
  fitInformation.Draw();
  canvas.SaveAs(output);
}
} // namespace

/**
 * Basic diagnostics of the KFParticle conversion-photon DecayTree.
 */
void plot_conversion(const char *inputFile = "../root/ee_reco_merged.root", const char *outputDirectory = "figure")
{
  gStyle->SetOptStat(0);
  TGaxis::SetMaxDigits(3);
  AnalysisUtils::prepareOutputDirectory(outputDirectory);

  TChain chain("DecayTree");
  if (!AnalysisUtils::addInput(chain, inputFile))
  {
    return;
  }

  const TString wrappedPairDeltaPhi = "atan2(sin(track_1_phi-track_2_phi),cos(track_1_phi-track_2_phi))";
//  const TCut conversionCut = Form("fabs(track_1_pseudorapidity-track_2_pseudorapidity)<0.02 && "
//                                  "fabs(%s)<0.01",
//                                  wrappedPairDeltaPhi.Data());
  const TCut conversionCut = Form("fabs(track_1_pseudorapidity-track_2_pseudorapidity)<0.05 && "
                                  "fabs(%s)<0.02",
                                  wrappedPairDeltaPhi.Data());
  const TCut displacedConversionCut = conversionCut + "sqrt(gamma_x*gamma_x+gamma_y*gamma_y)>1.5";

  TH1F track1DeltaPhi("h_conversion_track1_delta_phi", ";track 1 #Delta#phi(track-calo);Candidates", 50, -0.2, 0.2);
  AnalysisUtils::plot1D(&track1DeltaPhi, chain, "track_1_EMCAL_DeltaPhi",
                        AnalysisUtils::outputPath(outputDirectory, "track_1_EMCAL_DeltaPhi.pdf"));

  TH1F track2DeltaPhi("h_conversion_track2_delta_phi", ";track 2 #Delta#phi(track-calo);Candidates", 50, -0.2, 0.2);
  AnalysisUtils::plot1D(&track2DeltaPhi, chain, "track_2_EMCAL_DeltaPhi",
                        AnalysisUtils::outputPath(outputDirectory, "track_2_EMCAL_DeltaPhi.pdf"));

  TH1F track1DeltaZ("h_conversion_track1_delta_z", ";track 1 #Deltaz(track-calo) [cm];Candidates", 50, -10.0, 10.0);
  AnalysisUtils::plot1D(&track1DeltaZ, chain, "track_1_EMCAL_DeltaZ",
                        AnalysisUtils::outputPath(outputDirectory, "track_1_EMCAL_DeltaZ.pdf"));

  TH1F track2DeltaZ("h_conversion_track2_delta_z", ";track 2 #Deltaz(track-calo) [cm];Candidates", 50, -10.0, 10.0);
  AnalysisUtils::plot1D(&track2DeltaZ, chain, "track_2_EMCAL_DeltaZ",
                        AnalysisUtils::outputPath(outputDirectory, "track_2_EMCAL_DeltaZ.pdf"));

  TH1F pairDeltaPhi("h_conversion_pair_delta_phi", ";#Delta#phi(track 1, track 2);Candidates", 50, -0.1, 0.1);
  AnalysisUtils::plot1D(&pairDeltaPhi, chain, wrappedPairDeltaPhi,
                        AnalysisUtils::outputPath(outputDirectory, "track12_dphi.pdf"));

  TH1F pairDeltaEta("h_conversion_pair_delta_eta", ";#Delta#eta(track 1, track 2);Candidates", 50, -0.1, 0.1);
  AnalysisUtils::plot1D(&pairDeltaEta, chain, "track_1_pseudorapidity-track_2_pseudorapidity",
                        AnalysisUtils::outputPath(outputDirectory, "track12_deta.pdf"));

  TH1F track1SiliconHits("h_conversion_track1_silicon_hits", ";track 1 MVTX+INTT hits;Candidates", 10, -0.5, 9.5);
  AnalysisUtils::plot1D(&track1SiliconHits, chain, "track_1_MVTX_nHits+track_1_INTT_nHits",
                        AnalysisUtils::outputPath(outputDirectory, "track1_nSi.pdf"));

  TH1F track2SiliconHits("h_conversion_track2_silicon_hits", ";track 2 MVTX+INTT hits;Candidates", 10, -0.5, 9.5);
  AnalysisUtils::plot1D(&track2SiliconHits, chain, "track_2_MVTX_nHits+track_2_INTT_nHits",
                        AnalysisUtils::outputPath(outputDirectory, "track2_nSi.pdf"));

  TH1F mass("h_conversion_mass", ";M_{e^{+}e^{-}} [GeV/c^{2}];Candidates", 100, 0.0, 0.05);
  AnalysisUtils::plot1D(&mass, chain, "gamma_mass", AnalysisUtils::outputPath(outputDirectory, "gamma_mass.pdf"),
                        conversionCut);

  TH1F radius("h_conversion_radius", ";R_{xy}(e^{+}e^{-}) [cm];Candidates", 100, 0.0, 30.0);
  AnalysisUtils::plot1D(&radius, chain, "sqrt(gamma_x*gamma_x+gamma_y*gamma_y)",
                        AnalysisUtils::outputPath(outputDirectory, "gamma_radius.pdf"), conversionCut);

  TH2F vertexXY("h_conversion_vertex_xy", ";x(e^{+}e^{-}) [cm];y(e^{+}e^{-}) [cm];Candidates", 200, -30.0, 30.0, 200,
                -30.0, 30.0);
  AnalysisUtils::plot2D(&vertexXY, chain, "gamma_y:gamma_x",
                        AnalysisUtils::outputPath(outputDirectory, "gamma_x_vs_y.pdf"),
                        conversionCut + "sqrt(gamma_x*gamma_x+gamma_y*gamma_y)>15.0");

  TH2F radiusVsZ("h_conversion_radius_vs_z", ";z(e^{+}e^{-}) [cm];R_{xy}(e^{+}e^{-}) [cm];Candidates", 100, -50.0, 50.0,
                 100, 0.0, 25.0);
  AnalysisUtils::plot2D(&radiusVsZ, chain, "sqrt(gamma_x*gamma_x+gamma_y*gamma_y):gamma_z",
                        AnalysisUtils::outputPath(outputDirectory, "gamma_R_vs_z.pdf"), conversionCut);

  plotChargeOrderedEOverP(chain, displacedConversionCut,
                          AnalysisUtils::outputPath(outputDirectory, "ep_em_Eop_fit.pdf"));
}
