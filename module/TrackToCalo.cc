/*!
 *  \file TrackToCalo.cc
 *  \brief Save matched track-EMCal pairs to a ROOT tree
 *  \author Xudong Yu <xyu3@bnl.gov>
 */

#include "TrackToCalo.h"

#include <calobase/RawCluster.h>
#include <calobase/RawClusterContainer.h>
#include <calobase/RawTowerGeomContainer.h>

#include <ffaobjects/EventHeader.h>

#include <fun4all/Fun4AllReturnCodes.h>

#include <phool/PHCompositeNode.h>
#include <phool/getClass.h>

#include <trackbase/TrkrDefs.h>
#include <trackbase_historic/SvtxTrack.h>
#include <trackbase_historic/SvtxTrackMap.h>
#include <trackbase_historic/SvtxTrackState.h>
#include <trackbase_historic/TrackSeed.h>

#include <TFile.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

namespace
{
  // A fitted track can carry cluster keys in both its silicon and TPC seeds.
  std::vector<TrkrDefs::cluskey> getClusterKeys(const SvtxTrack *track)
  {
    std::vector<TrkrDefs::cluskey> keys;
    for (const auto *seed : {track->get_silicon_seed(), track->get_tpc_seed()})
    {
      if (!seed)
      {
        continue;
      }
      std::copy(seed->begin_cluster_keys(), seed->end_cluster_keys(),
                std::back_inserter(keys));
    }
    return keys;
  }

  template <int detectorId>
  int countClusters(const std::vector<TrkrDefs::cluskey> &keys)
  {
    return std::count_if(
        keys.begin(), keys.end(), [](const TrkrDefs::cluskey key) {
          return TrkrDefs::getTrkrId(key) == detectorId;
        });
  }

  // Wrap a phi difference into [-pi, pi].
  float piRange(float deltaPhi)
  {
    constexpr float pi = 3.14159265358979323846;
    constexpr float twoPi = 2.0 * pi;
    while (deltaPhi > pi)
    {
      deltaPhi -= twoPi;
    }
    while (deltaPhi < -pi)
    {
      deltaPhi += twoPi;
    }
    return deltaPhi;
  }
}  // namespace

TrackToCalo::TrackToCalo(const std::string &name)
  : SubsysReco(name)
{
}

int TrackToCalo::Init(PHCompositeNode * /*topNode*/)
{
  m_outputFile = TFile::Open(m_outputFileName.c_str(), "RECREATE");
  if (!m_outputFile || m_outputFile->IsZombie())
  {
    std::cerr << Name() << ": cannot create output file " << m_outputFileName
              << std::endl;
    delete m_outputFile;
    m_outputFile = nullptr;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  m_outputFile->cd();
  m_tree = new TTree("TrackCaloTree", "Accepted track-EMCal cluster pairs");
  createBranches();

  return Fun4AllReturnCodes::EVENT_OK;
}

void TrackToCalo::createBranches()
{
  m_tree->Branch("run", &m_runNumber, "run/I");
  m_tree->Branch("event", &m_eventNumber, "event/I");
  m_tree->Branch("event_index", &m_eventIndex, "event_index/i");

  m_tree->Branch("track_id", &m_trackId, "track_id/i");
  m_tree->Branch("track_charge", &m_trackCharge, "track_charge/I");
  m_tree->Branch("track_crossing", &m_trackCrossing, "track_crossing/I");
  m_tree->Branch("track_momentum", &m_trackMomentum);
  m_tree->Branch("track_quality", &m_trackQuality, "track_quality/F");

  m_tree->Branch("state_position_emcal", &m_statePosition_EMCal);
  m_tree->Branch("state_momentum_emcal", &m_stateMomentum_EMCal);

  m_tree->Branch("cluster_id", &m_clusterId, "cluster_id/i");
  m_tree->Branch("cluster_energy", &m_clusterEnergy, "cluster_energy/F");
  m_tree->Branch("cluster_position", &m_clusterPosition);

  m_tree->Branch("delta_phi", &m_deltaPhi, "delta_phi/F");
  m_tree->Branch("delta_z", &m_deltaZ, "delta_z/F");
  m_tree->Branch("e_over_p", &m_eOverP, "e_over_p/F");
}

int TrackToCalo::process_event(PHCompositeNode *topNode)
{
  ++m_eventIndex;

  const auto *eventHeader = findNode::getClass<EventHeader>(topNode, "EventHeader");
  m_runNumber = eventHeader ? eventHeader->get_RunNumber() : -1;
  m_eventNumber = eventHeader ? eventHeader->get_EvtSequence() : -1;

  // Resolve the input nodes on the first event, then reuse the pointers.
  if (!m_trackMap)
  {
    m_trackMap = findNode::getClass<SvtxTrackMap>(topNode, m_trackMapName);
  }
  if (!m_emcalClusters)
  {
    m_emcalClusters = findNode::getClass<RawClusterContainer>(topNode, m_emcalClusterNode);
  }
  if (!m_emcalGeometry)
  {
    if (m_useDetailedGeom) { m_emcalGeometryNode += "_DETAILED"; }
    m_emcalGeometry = findNode::getClass<RawTowerGeomContainer>(topNode, m_emcalGeometryNode);
  }

  if (!m_trackMap || !m_emcalClusters || !m_emcalGeometry)
  {
    std::cerr << Name() << ": required node is missing:\n"
              << "  " << m_trackMapName << " = " << m_trackMap << '\n'
              << "  " << m_emcalClusterNode << " = " << m_emcalClusters
              << '\n'
              << "  " << m_emcalGeometryNode << " = " << m_emcalGeometry
              << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  const float emcalRadius = m_useEmcalRadiusUser
                                ? m_emcalRadiusUser
                                : m_emcalGeometry->get_radius();

  std::cout << "use user emcal radius or not? " << m_useEmcalRadiusUser
	  << " : user emcal radius = " << m_emcalRadiusUser
	  << " , geometry radius = " << m_emcalGeometry->get_radius()
	  << std::endl;

  unsigned int matchedTracks = 0;
  unsigned int matchedPairs = 0;
  unsigned int tracksWithoutProjection = 0;

  for (const auto &[trackKey, track] : *m_trackMap)
  {
    if (!passesTrackSelection(track))
    {
      continue;
    }

    // PHActsTrackProjection stores a state whose path-length key is the
    // configured calorimeter radius.  No extrapolation is done in this class.
    const SvtxTrackState *state = track->get_state(emcalRadius);
    if (!state)
    {
      ++tracksWithoutProjection;
      continue;
    }

    m_trackId = trackKey;
    m_trackCharge = track->get_charge();
    m_trackCrossing = track->get_crossing();
    // No particle-mass hypothesis is needed for track-calo matching, so a
    // three-vector is the natural representation of the track momentum.
    m_trackMomentum.SetXYZ(track->get_px(), track->get_py(), track->get_pz());
    m_trackQuality = track->get_quality();

    m_statePosition_EMCal.SetXYZ(state->get_x(), state->get_y(), state->get_z());
    m_stateMomentum_EMCal.SetXYZ(state->get_px(), state->get_py(), state->get_pz());

    bool trackHasMatch = false;
    const auto clusterRange = m_emcalClusters->getClusters();
    for (auto clusterIterator = clusterRange.first;
         clusterIterator != clusterRange.second; ++clusterIterator)
    {
      const RawCluster *cluster = clusterIterator->second;
      if (!cluster || cluster->get_energy() < m_emcalEnergyMin)
      {
        continue;
      }

      const TVector3 clusterPosition(cluster->get_x(), cluster->get_y(), cluster->get_z());
      const float clusterRadius = clusterPosition.Perp();
      if (!(clusterRadius > 0.0))
      {
        continue;
      }

      const float clusterZAtRadius = clusterPosition.Z() * emcalRadius / clusterRadius;
      const float deltaPhi = piRange(m_statePosition_EMCal.Phi() - clusterPosition.Phi());
      const float deltaZ = m_statePosition_EMCal.Z() - clusterZAtRadius;

      if (std::abs(deltaPhi) >= m_absDeltaPhiMax ||
          std::abs(deltaZ) >= m_absDeltaZMax)
      {
        continue;
      }

      m_clusterId = clusterIterator->first;
      m_clusterEnergy = cluster->get_energy();
      m_clusterPosition = clusterPosition;
      m_deltaPhi = deltaPhi;
      m_deltaZ = deltaZ;
      m_eOverP = m_trackMomentum.Mag() > 0.0
                     ? m_clusterEnergy / m_trackMomentum.Mag()
                     : std::numeric_limits<float>::quiet_NaN();

      m_tree->Fill();
      ++matchedPairs;
      ++m_totalPairs;
      trackHasMatch = true;

      if (Verbosity() > 2)
      {
        std::cout << Name() << ": saved track " << m_trackId
                  << " with cluster " << m_clusterId
                  << " (dphi=" << m_deltaPhi << ", dz=" << m_deltaZ << ')'
                  << std::endl;
      }
    }

    if (trackHasMatch)
    {
      ++matchedTracks;
    }
  }

  if (Verbosity() > 0)
  {
    std::cout << Name() << ": event " << m_eventIndex << ", input tracks = "
              << m_trackMap->size() << ", matched tracks = " << matchedTracks
              << ", saved pairs = " << matchedPairs
              << ", no CEMC state = " << tracksWithoutProjection << std::endl;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

bool TrackToCalo::passesTrackSelection(const SvtxTrack *track) const
{
  if (!track || track->get_pt() < m_trackPtMin ||
      track->get_quality() > m_trackQualityMax)
  {
    return false;
  }

  if (m_requireCrossingZero && track->get_crossing() != 0)
  {
    return false;
  }

  const auto clusterKeys = getClusterKeys(track);
  return countClusters<TrkrDefs::mvtxId>(clusterKeys) >= m_minMvtxClusters &&
         countClusters<TrkrDefs::inttId>(clusterKeys) >= m_minInttClusters &&
         countClusters<TrkrDefs::tpcId>(clusterKeys) >= m_minTpcClusters &&
         countClusters<TrkrDefs::micromegasId>(clusterKeys) >=
             m_minTpotClusters;
}

int TrackToCalo::End(PHCompositeNode * /*topNode*/)
{
  if (m_outputFile)
  {
    m_outputFile->cd();
    if (m_tree)
    {
      m_tree->Write();
    }
    m_outputFile->Close();
    delete m_outputFile;
    m_outputFile = nullptr;
    m_tree = nullptr;
  }

  std::cout << Name() << ": wrote " << m_totalPairs
            << " matched pairs to " << m_outputFileName << std::endl;
  return Fun4AllReturnCodes::EVENT_OK;
}
