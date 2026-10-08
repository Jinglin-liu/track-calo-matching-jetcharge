#include "AnalysisUtils.h"

#include <TChain.h>
#include <TGaxis.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TStyle.h>

#include <iostream>
#include <map>
#include <tuple>

/**
 * Basic offline analysis of the direct TrackToCalo module output.
 *
 * Run from analysis/ with, for example:
 *   root -l -b -q 'plot_track_calo.C("../root/track_calo_pairs_all.root")'
 *
 * One TrackCaloTree entry is one accepted track-cluster pair.  Multiple
 * entries with the same event and track ID therefore expose matching
 * ambiguity without rerunning reconstruction.
 */
void plot_track_calo(const char *inputPattern = "../root/track_calo_pairs_all.root",
                     const char *outputDirectory = "figure")
{
  gStyle->SetOptStat(0);
  TGaxis::SetMaxDigits(3);
  AnalysisUtils::prepareOutputDirectory(outputDirectory);

  TChain chain("TrackCaloTree");
  if (!AnalysisUtils::addInput(chain, inputPattern))
  {
    return;
  }

  TH1F deltaPhi("h_track_calo_delta_phi", ";#Delta#phi(track state, calo cluster);Entries", 100, -0.1, 0.1);
  AnalysisUtils::plot1D(&deltaPhi, chain, "delta_phi",
                        AnalysisUtils::outputPath(outputDirectory, "track_calo_delta_phi.pdf"));

  TH1F deltaZ("h_track_calo_delta_z", ";#Deltaz(track state, calo cluster) [cm];Entries", 100, -10.0, 10.0);
  AnalysisUtils::plot1D(&deltaZ, chain, "delta_z",
                        AnalysisUtils::outputPath(outputDirectory, "track_calo_delta_z.pdf"));

  TH2F residuals("h_track_calo_residuals", ";#Delta#phi;#Deltaz [cm];Entries", 100, -0.1, 0.1, 100, -10.0, 10.0);
  AnalysisUtils::plot2D(&residuals, chain, "delta_z:delta_phi",
                        AnalysisUtils::outputPath(outputDirectory, "track_calo_residuals.pdf"));

  TH1F eOverP("h_track_calo_e_over_p", ";E_{cluster}/p_{track};Entries", 100, 0.0, 2.0);
  AnalysisUtils::plot1D(&eOverP, chain, "e_over_p",
                        AnalysisUtils::outputPath(outputDirectory, "track_calo_e_over_p.pdf"));

  TH2F deltaPhiVsCharge("h_track_calo_delta_phi_vs_charge",
                        ";track charge;#Delta#phi(track state, calo cluster);Entries", 3, -1.5, 1.5, 100, -0.1, 0.1);
  AnalysisUtils::plot2D(&deltaPhiVsCharge, chain, "delta_phi:track_charge",
                        AnalysisUtils::outputPath(outputDirectory, "track_calo_delta_phi_vs_charge.pdf"));

  // Count accepted clusters per track.  The TChain tree number keeps entries
  // from different input files distinct even when event_index restarts.
  int run = -1;
  unsigned int eventIndex = 0;
  unsigned int trackId = 0;
  chain.SetBranchAddress("run", &run);
  chain.SetBranchAddress("event_index", &eventIndex);
  chain.SetBranchAddress("track_id", &trackId);

  using TrackKey = std::tuple<int, int, unsigned int, unsigned int>;
  std::map<TrackKey, unsigned int> candidatesPerTrack;
  const Long64_t entries = chain.GetEntries();
  for (Long64_t entry = 0; entry < entries; ++entry)
  {
    chain.LoadTree(entry);
    chain.GetEntry(entry);
    const TrackKey key{chain.GetTreeNumber(), run, eventIndex, trackId};
    ++candidatesPerTrack[key];
  }

  TH1F multiplicity("h_track_calo_match_multiplicity", ";accepted EMCal clusters per track;Tracks", 10, 0.5, 10.5);
  unsigned long long ambiguousTracks = 0;
  for (const auto &[key, candidates] : candidatesPerTrack)
  {
    (void)key;
    multiplicity.Fill(candidates);
    if (candidates > 1)
    {
      ++ambiguousTracks;
    }
  }

  TCanvas multiplicityCanvas("canvas_track_calo_match_multiplicity", "", 800, 600);
  multiplicity.SetLineColor(kBlack);
  multiplicity.Draw("E");
  auto sphenixTag = AnalysisUtils::makeSphenixTag();
  sphenixTag->Draw();
  multiplicityCanvas.SaveAs(AnalysisUtils::outputPath(outputDirectory, "track_calo_match_multiplicity.pdf"));

  const double ambiguousFraction =
      candidatesPerTrack.empty() ? 0.0 : static_cast<double>(ambiguousTracks) / candidatesPerTrack.size();
  std::cout << "Entries: " << entries << '\n'
            << "Matched tracks: " << candidatesPerTrack.size() << '\n'
            << "Tracks with multiple accepted clusters: " << ambiguousTracks << " (" << 100.0 * ambiguousFraction
            << "%)" << std::endl;
}
