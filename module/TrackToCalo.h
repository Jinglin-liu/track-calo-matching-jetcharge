// Tell emacs that this is a C++ source
//  -*- C++ -*-.
/*!
 *  \file TrackToCalo.h
 *  \brief Save matched track-EMCal pairs to a ROOT tree
 *  \author Xudong Yu <xyu3@bnl.gov>
 */

#ifndef TRACKTOCALO_H
#define TRACKTOCALO_H

#include <fun4all/SubsysReco.h>

#include <TVector3.h>

#include <string>

class PHCompositeNode;
class RawClusterContainer;
class RawTowerGeomContainer;
class SvtxTrack;
class SvtxTrackMap;
class TFile;
class TTree;

/**
 * Match reconstructed tracks to EMCal clusters and save the pair information.
 *
 * The input tracks must already contain a state at the configured EMCal
 * projection radius.  PHActsTrackProjection creates that state in the
 * accompanying Fun4All macro.  Each track-cluster pair that passes the
 * residual window becomes one entry in TrackCaloTree.
 */
class TrackToCalo : public SubsysReco
{
 public:
  explicit TrackToCalo(const std::string &name = "TrackToCalo");
  ~TrackToCalo() override = default;

  int Init(PHCompositeNode *topNode) override;
  int process_event(PHCompositeNode *topNode) override;
  int End(PHCompositeNode *topNode) override;

  const std::string &GetTrackMapName() const { return m_trackMapName; }
  void SetTrackMapName(const std::string &name) { m_trackMapName = name; }

  const std::string &GetOutputFileName() const { return m_outputFileName; }
  void SetOutputFileName(const std::string &name) { m_outputFileName = name; }

  // Keep the familiar configuration names used by the original analysis.
  void EMcalRadiusUser(bool use) { m_useEmcalRadiusUser = use; }
  void setEMcalRadius(float radius) { m_emcalRadiusUser = radius; }
  void setRawClusContEMName(const std::string &name) { m_emcalClusterNode = name; }
  void setRawTowerGeomContName(const std::string &name) { m_emcalGeometryNode = name; }
  void setUseDetailedGeometry(bool flag) { m_useDetailedGeom = flag; }

  void setTrackPtLowCut(float value) { m_trackPtMin = value; }
  void setEmcalELowCut(float value) { m_emcalEnergyMin = value; }
  void setnMvtxClusters(int value) { m_minMvtxClusters = value; }
  void setnInttClusters(int value) { m_minInttClusters = value; }
  void setnTpcClusters(int value) { m_minTpcClusters = value; }
  void setnTpotClusters(int value) { m_minTpotClusters = value; }
  void setTrackQuality(float value) { m_trackQualityMax = value; }
  void setdphicut(float value) { m_absDeltaPhiMax = value; }
  void setdzcut(float value) { m_absDeltaZMax = value; }
  void requireCrossingZero(bool value = true) { m_requireCrossingZero = value; }

 private:
  bool passesTrackSelection(const SvtxTrack *track) const;
  void createBranches();

  SvtxTrackMap *m_trackMap = nullptr;
  RawClusterContainer *m_emcalClusters = nullptr;
  RawTowerGeomContainer *m_emcalGeometry = nullptr;

  TFile *m_outputFile = nullptr;
  TTree *m_tree = nullptr;

  std::string m_outputFileName = "track_calo.root";
  std::string m_trackMapName = "SvtxTrackMap";
  std::string m_emcalClusterNode = "CLUSTERINFO_CEMC";
  std::string m_emcalGeometryNode = "TOWERGEOM_CEMC";

  bool m_useDetailedGeom = false;
  bool m_useEmcalRadiusUser = false;
  float m_emcalRadiusUser = 102.9;
  bool m_requireCrossingZero = false;

  float m_trackPtMin = 0.2;
  float m_emcalEnergyMin = 0.2;
  int m_minMvtxClusters = 0;
  int m_minInttClusters = 0;
  int m_minTpcClusters = 22;
  int m_minTpotClusters = 0;
  float m_trackQualityMax = 300.0;
  float m_absDeltaPhiMax = 0.10;
  float m_absDeltaZMax = 10.0;

  // One tree entry represents one accepted track-cluster pair.
  int m_runNumber = -1;
  int m_eventNumber = -1;
  unsigned int m_eventIndex = 0;

  unsigned int m_trackId = 0;
  int m_trackCharge = 0;
  int m_trackCrossing = 0;
  TVector3 m_trackMomentum;
  float m_trackQuality = 0.0;

  TVector3 m_statePosition_EMCal;
  TVector3 m_stateMomentum_EMCal;

  unsigned int m_clusterId = 0;
  float m_clusterEnergy = 0.0;
  TVector3 m_clusterPosition;

  float m_deltaPhi = 0.0;
  float m_deltaZ = 0.0;
  float m_eOverP = 0.0;
  unsigned long long m_totalPairs = 0;
};

#endif  // TRACKTOCALO_H
