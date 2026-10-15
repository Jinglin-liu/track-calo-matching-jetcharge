#ifndef JETCHARGETREEMAKER_H
#define JETCHARGETREEMAKER_H

#include <fun4all/SubsysReco.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

class PHCompositeNode;
class TFile;
class TTree;

/**
 * One TTree entry per Fun4All event, with full selected-track and
 * anti-kT calorimeter-jet arrays. Track-to-jet matching is deliberately
 * deferred to offline analysis: no EMCal match is required for a track.
 *
 * Track and jet vectors in an entry originate from the same Fun4All topNode.
 * This does NOT independently establish synchronization of different input
 * DST streams; verify event synchronization upstream.
 */
class JetChargeTreeMaker : public SubsysReco
{
 public:
  explicit JetChargeTreeMaker(const std::string& name = "JetChargeTreeMaker");
  ~JetChargeTreeMaker() override = default;

  int Init(PHCompositeNode* topNode) override;
  int process_event(PHCompositeNode* topNode) override;
  int End(PHCompositeNode* topNode) override;

  void setOutputFile(const std::string& name) { m_outputName = name; }
  void setTrackNode(const std::string& name) { m_trackNode = name; }
  void setJetNode(const std::string& name) { m_jetNode = name; }
  void setTrackPtMin(float value) { m_trackPtMin = value; }
  void setTrackQualityMax(float value) { m_trackQualityMax = value; }
  void setMinTpcClusters(int value) { m_minTpcClusters = value; }
  void requireCrossingZero(bool value) { m_requireCrossingZero = value; }
  void setJetPtMin(float value) { m_jetPtMin = value; }

 private:
  void resetBranches();

  std::string m_outputName = "jet_charge_inputs.root";
  std::string m_trackNode = "SvtxTrackMap";
  std::string m_jetNode = "AntiKt_unsubtracted_r04";
  float m_trackPtMin = 0.2f;
  float m_trackQualityMax = 300.f;
  int m_minTpcClusters = 22;
  bool m_requireCrossingZero = true;
  float m_jetPtMin = 0.f;
  TFile* m_output = nullptr;
  TTree* m_tree = nullptr;
  std::uint64_t m_eventIndex = 0; // starts at zero; job-local

  int m_run = -1, m_event = -1;
  std::uint64_t m_entryIndex = 0;
  float m_vertexZ = std::numeric_limits<float>::quiet_NaN();
  int m_nTracks = 0, m_nJets = 0;

  std::vector<unsigned int> m_trackId;
  std::vector<int> m_trackCharge, m_trackCrossing, m_trackNdf;
  std::vector<unsigned int> m_trackVertexId;
  std::vector<int> m_trackNMvtx, m_trackNIntt, m_trackNTpc, m_trackNTpot;
  std::vector<float> m_trackPx, m_trackPy, m_trackPz;
  std::vector<float> m_trackPt, m_trackEta, m_trackPhi;
  std::vector<float> m_trackX, m_trackY, m_trackZ;
  std::vector<float> m_trackQuality, m_trackChi2;
  std::vector<float> m_trackDca2d, m_trackDca2dError;
  std::vector<float> m_trackDca3dXY, m_trackDca3dZ;

  std::vector<unsigned int> m_jetId;
  std::vector<float> m_jetPx, m_jetPy, m_jetPz, m_jetE;
  std::vector<float> m_jetPt, m_jetEt, m_jetEta, m_jetPhi, m_jetMass;
  std::vector<float> m_jetEmcalFrac, m_jetIhcalFrac, m_jetOhcalFrac;
  // Flattened constituent array: jet j owns [offset[j], offset[j+1]).
  std::vector<unsigned int> m_jetComponentOffset, m_jetComponentId;
  std::vector<int> m_jetComponentSource, m_jetComponentIEta, m_jetComponentIPhi;
  std::vector<float> m_jetComponentEnergy;
};

#endif  // JETCHARGETREEMAKER_H
