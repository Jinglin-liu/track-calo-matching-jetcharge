#ifndef TRACK_CALO_TUTORIAL_ANALYSIS_UTILS_H
#define TRACK_CALO_TUTORIAL_ANALYSIS_UTILS_H

#include <TCanvas.h>
#include <TChain.h>
#include <TCut.h>
#include <TH1.h>
#include <TH2.h>
#include <TLegend.h>
#include <TPad.h>
#include <TPaveText.h>
#include <TString.h>
#include <TSystem.h>
#include <TText.h>
#include <TVector2.h>

#include <iostream>
#include <memory>

namespace AnalysisUtils
{
inline double wrapDeltaPhi(const double deltaPhi)
{
  return TVector2::Phi_mpi_pi(deltaPhi);
}

inline bool addInput(TChain &chain, const TString &inputPattern)
{
  const int filesAdded = chain.Add(inputPattern);
  if (filesAdded == 0)
  {
    std::cerr << "No input files matched: " << inputPattern << std::endl;
    return false;
  }
  std::cout << "Added " << filesAdded << " file(s): " << inputPattern << std::endl;
  return true;
}

inline void prepareOutputDirectory(const TString &outputDirectory)
{
  if (gSystem->mkdir(outputDirectory, true) != 0 && gSystem->AccessPathName(outputDirectory))
  {
    std::cerr << "Cannot create output directory: " << outputDirectory << std::endl;
  }
}

inline TString outputPath(const TString &directory, const TString &fileName)
{
  return directory + "/" + fileName;
}

inline std::unique_ptr<TPaveText> makeSphenixTag(const double x1 = 0.20, const double y1 = 0.60, const double x2 = 0.45, const double y2 = 0.80)
{
  auto tag = std::make_unique<TPaveText>(x1, y1, x2, y2, "NDC");
  tag->SetFillStyle(0);
  tag->SetBorderSize(0);
  tag->SetTextAlign(12);
  tag->SetTextSize(0.035);

  TText *pt_LaTex = tag->AddText("#it{#bf{sPHENIX}} Internal");
  pt_LaTex = tag->AddText("#it{p}+#it{p} #sqrt{s} = 200 GeV");
  // pt_LaTex = tag->AddText("Au+Au #sqrt{s_{NN}} = 200 GeV");
  // pt_LaTex = tag->AddText("O+O #sqrt{s_{NN}} = 200 GeV");
  (void)pt_LaTex;
  return tag;
}

inline void plot1D(TH1 *histogram, TChain &chain, const TString &expression, const TString &output,
                   const TCut &cut = "1", const TString &legendText = "")
{
  chain.Draw(expression + ">>" + histogram->GetName(), cut, "goff");

  TCanvas canvas(Form("canvas_%s", histogram->GetName()), "", 800, 600);
  canvas.SetTopMargin(0.12);
  histogram->SetLineColor(kBlack);
  histogram->Draw("E");
  //auto sphenixTag = makeSphenixTag();
  auto sphenixTag = makeSphenixTag(0.6, 0.6, 0.8, 0.7);
  sphenixTag->Draw();

  if (!legendText.IsNull())
  {
    TLegend legend(0.52, 0.84, 0.88, 0.92);
    legend.AddEntry(histogram, legendText, "lep");
    legend.Draw();
    canvas.SaveAs(output);
    return;
  }
  canvas.SaveAs(output);
}

inline void plot2D(TH2 *histogram, TChain &chain, const TString &expression, const TString &output,
                   const TCut &cut = "1")
{
  chain.Draw(expression + ">>" + histogram->GetName(), cut, "goff");

  TCanvas canvas(Form("canvas_%s", histogram->GetName()), "", 800, 600);
  canvas.SetLeftMargin(0.13);
  canvas.SetRightMargin(0.15);
  histogram->Draw("COLZ");
  auto sphenixTag = makeSphenixTag();
  sphenixTag->Draw();
  canvas.SaveAs(output);
}
} // namespace AnalysisUtils

#endif
