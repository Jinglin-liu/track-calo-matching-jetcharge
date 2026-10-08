#include "AnalysisUtils.h"

#include <TChain.h>
#include <TFile.h>
#include <TString.h>
#include <TSystem.h>
#include <TTree.h>

#include <cmath>
#include <iostream>

/**
 * Select conversion candidates and write a compact charge-ordered E/p tree.
 *
 * The first argument is the input ROOT file and the second is the output file:
 *   skimer_eop("../root/ee_reco_merged.root", "../root/ee_reco_skim.root")
 */
void skimer_eop(const char *inputFileName = "../root/ee_reco_merged.root",
                const char *outputFileName = "../root/ee_reco_skim.root")
{
  const TString input = inputFileName;
  const TString output = outputFileName;

  TChain chain("DecayTree");
  if (!AnalysisUtils::addInput(chain, input))
  {
    return;
  }

  const Long64_t entries = chain.GetEntries();
  if (entries == 0)
  {
    std::cerr << "Input DecayTree is empty: " << input << std::endl;
    return;
  }

  float gammaX = 0.0;
  float gammaY = 0.0;
  float track1P = 0.0;
  float track1Pt = 0.0;
  float track1EmcalEnergy = 0.0;
  float track1Phi = 0.0;
  float track1Theta = 0.0;
  float track1Eta = 0.0;
  Char_t track1Charge = 0;
  float track2P = 0.0;
  float track2Pt = 0.0;
  float track2EmcalEnergy = 0.0;
  float track2Phi = 0.0;
  float track2Theta = 0.0;
  float track2Eta = 0.0;
  Char_t track2Charge = 0;

  chain.SetBranchAddress("gamma_x", &gammaX);
  chain.SetBranchAddress("gamma_y", &gammaY);
  chain.SetBranchAddress("track_1_p", &track1P);
  chain.SetBranchAddress("track_1_pT", &track1Pt);
  chain.SetBranchAddress("track_1_EMCAL_energy_cluster", &track1EmcalEnergy);
  chain.SetBranchAddress("track_1_phi", &track1Phi);
  chain.SetBranchAddress("track_1_theta", &track1Theta);
  chain.SetBranchAddress("track_1_pseudorapidity", &track1Eta);
  chain.SetBranchAddress("track_1_charge", &track1Charge);
  chain.SetBranchAddress("track_2_p", &track2P);
  chain.SetBranchAddress("track_2_pT", &track2Pt);
  chain.SetBranchAddress("track_2_EMCAL_energy_cluster", &track2EmcalEnergy);
  chain.SetBranchAddress("track_2_phi", &track2Phi);
  chain.SetBranchAddress("track_2_theta", &track2Theta);
  chain.SetBranchAddress("track_2_pseudorapidity", &track2Eta);
  chain.SetBranchAddress("track_2_charge", &track2Charge);

  const TString outputDirectory = gSystem->DirName(output);
  AnalysisUtils::prepareOutputDirectory(outputDirectory);
  TFile outputFile(output, "RECREATE");
  if (outputFile.IsZombie())
  {
    std::cerr << "Cannot create output file: " << output << std::endl;
    return;
  }

  double epP = 0.0;
  double epPt = 0.0;
  double epEmcalEnergy = 0.0;
  double epPhi = 0.0;
  double epTheta = 0.0;
  double epEta = 0.0;
  double emP = 0.0;
  double emPt = 0.0;
  double emEmcalEnergy = 0.0;
  double emPhi = 0.0;
  double emTheta = 0.0;
  double emEta = 0.0;

  TTree outputTree("DecayTree", "Charge-ordered conversion candidates");
  outputTree.Branch("ep_p", &epP, "ep_p/D");
  outputTree.Branch("ep_pt", &epPt, "ep_pt/D");
  outputTree.Branch("ep_emcal_e", &epEmcalEnergy, "ep_emcal_e/D");
  outputTree.Branch("ep_phi", &epPhi, "ep_phi/D");
  outputTree.Branch("ep_theta", &epTheta, "ep_theta/D");
  outputTree.Branch("ep_eta", &epEta, "ep_eta/D");
  outputTree.Branch("em_p", &emP, "em_p/D");
  outputTree.Branch("em_pt", &emPt, "em_pt/D");
  outputTree.Branch("em_emcal_e", &emEmcalEnergy, "em_emcal_e/D");
  outputTree.Branch("em_phi", &emPhi, "em_phi/D");
  outputTree.Branch("em_theta", &emTheta, "em_theta/D");
  outputTree.Branch("em_eta", &emEta, "em_eta/D");

  Long64_t selectedCandidates = 0;
  for (Long64_t entry = 0; entry < entries; ++entry)
  {
    if (entry % 100000 == 0)
    {
      std::cout << "Processed " << entry << " / " << entries << std::endl;
    }
    chain.GetEntry(entry);

    const double pairDeltaPhi = AnalysisUtils::wrapDeltaPhi(track1Phi - track2Phi);
    if (std::abs(pairDeltaPhi) >= 0.02 || std::abs(track1Eta - track2Eta) >= 0.01)
    {
      continue;
    }

    const double conversionRadius = std::hypot(gammaX, gammaY);
    if (conversionRadius <= 1.0 || conversionRadius >= 50.0)
    {
      continue;
    }

    if (track1Charge == 1 && track2Charge == -1)
    {
      epP = track1P;
      epPt = track1Pt;
      epEmcalEnergy = track1EmcalEnergy;
      epPhi = track1Phi;
      epTheta = track1Theta;
      epEta = track1Eta;
      emP = track2P;
      emPt = track2Pt;
      emEmcalEnergy = track2EmcalEnergy;
      emPhi = track2Phi;
      emTheta = track2Theta;
      emEta = track2Eta;
    }
    else if (track1Charge == -1 && track2Charge == 1)
    {
      epP = track2P;
      epPt = track2Pt;
      epEmcalEnergy = track2EmcalEnergy;
      epPhi = track2Phi;
      epTheta = track2Theta;
      epEta = track2Eta;
      emP = track1P;
      emPt = track1Pt;
      emEmcalEnergy = track1EmcalEnergy;
      emPhi = track1Phi;
      emTheta = track1Theta;
      emEta = track1Eta;
    }
    else
    {
      // Do not fill stale values for same-sign or otherwise invalid pairs.
      continue;
    }

    if (epP <= 0.0 || epPt <= 0.0 || epEmcalEnergy <= 0.0 || emP <= 0.0 || emPt <= 0.0 || emEmcalEnergy <= 0.0)
    {
      continue;
    }

    outputTree.Fill();
    ++selectedCandidates;
  }

  outputFile.cd();
  outputTree.Write();
  outputFile.Close();
  std::cout << "Selected " << selectedCandidates << " / " << entries << " candidates\nWrote " << output << std::endl;
}
