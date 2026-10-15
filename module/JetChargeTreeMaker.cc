#include "JetChargeTreeMaker.h"

#include <ffaobjects/EventHeader.h>
#include <fun4all/Fun4AllReturnCodes.h>
#include <globalvertex/GlobalVertex.h>
#include <globalvertex/GlobalVertexMap.h>
#include <jetbase/Jet.h>
#include <jetbase/JetContainer.h>
#include <phool/PHCompositeNode.h>
#include <phool/getClass.h>
#include <trackbase/TrkrDefs.h>
#include <trackbase_historic/SvtxTrack.h>
#include <trackbase_historic/SvtxTrackMap.h>
#include <trackbase_historic/TrackSeed.h>

#include <TFile.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <vector>

namespace
{
  std::vector<TrkrDefs::cluskey> clusterKeys(const SvtxTrack* track)
  {
    std::vector<TrkrDefs::cluskey> result;
    for (const auto* seed : {track->get_silicon_seed(), track->get_tpc_seed()})
    {
      if (!seed) continue;
      std::copy(seed->begin_cluster_keys(), seed->end_cluster_keys(),
                std::back_inserter(result));
    }
    return result;
  }

  int countDetector(const std::vector<TrkrDefs::cluskey>& keys, int detector)
  {
    return static_cast<int>(std::count_if(keys.begin(), keys.end(),
      [detector](TrkrDefs::cluskey key)
      {
        return static_cast<int>(TrkrDefs::getTrkrId(key)) == detector;
      }));
  }
}

JetChargeTreeMaker::JetChargeTreeMaker(const std::string& name)
  : SubsysReco(name)
{
}

int JetChargeTreeMaker::Init(PHCompositeNode*)
{
  m_output = TFile::Open(m_outputName.c_str(), "RECREATE");
  if (!m_output || m_output->IsZombie())
  {
    std::cerr << Name() << ": failed to create " << m_outputName << '\n';
    delete m_output;
    m_output = nullptr;
    return Fun4AllReturnCodes::ABORTRUN;
  }
  m_output->cd();
  m_tree = new TTree("JetChargeInputTree",
                     "One event per entry: reconstructed tracks and calorimeter jets");

#define BRANCH(name, member) m_tree->Branch(name, &member)
  BRANCH("run", m_run);
  BRANCH("event", m_event);
  BRANCH("event_index", m_entryIndex);
  BRANCH("vertex_z", m_vertexZ);
  BRANCH("n_tracks", m_nTracks);
  BRANCH("n_jets", m_nJets);
  BRANCH("track_id", m_trackId);
  BRANCH("track_charge", m_trackCharge);
  BRANCH("track_crossing", m_trackCrossing);
  BRANCH("track_vertex_id", m_trackVertexId);
  BRANCH("track_ndf", m_trackNdf);
  BRANCH("track_nmvtx", m_trackNMvtx);
  BRANCH("track_nintt", m_trackNIntt);
  BRANCH("track_ntpc", m_trackNTpc);
  BRANCH("track_ntpot", m_trackNTpot);
  BRANCH("track_px", m_trackPx);
  BRANCH("track_py", m_trackPy);
  BRANCH("track_pz", m_trackPz);
  BRANCH("track_pt", m_trackPt);
  BRANCH("track_eta", m_trackEta);
  BRANCH("track_phi", m_trackPhi);
  BRANCH("track_x", m_trackX);
  BRANCH("track_y", m_trackY);
  BRANCH("track_z", m_trackZ);
  BRANCH("track_quality", m_trackQuality);
  BRANCH("track_chi2", m_trackChi2);
  BRANCH("track_dca2d", m_trackDca2d);
  BRANCH("track_dca2d_error", m_trackDca2dError);
  BRANCH("track_dca3d_xy", m_trackDca3dXY);
  BRANCH("track_dca3d_z", m_trackDca3dZ);
  BRANCH("jet_id", m_jetId);
  BRANCH("jet_px", m_jetPx);
  BRANCH("jet_py", m_jetPy);
  BRANCH("jet_pz", m_jetPz);
  BRANCH("jet_e", m_jetE);
  BRANCH("jet_pt", m_jetPt);
  BRANCH("jet_et", m_jetEt);
  BRANCH("jet_eta", m_jetEta);
  BRANCH("jet_phi", m_jetPhi);
  BRANCH("jet_mass", m_jetMass);
  BRANCH("jet_emcal_frac", m_jetEmcalFrac);
  BRANCH("jet_ihcal_frac", m_jetIhcalFrac);
  BRANCH("jet_ohcal_frac", m_jetOhcalFrac);
  BRANCH("jet_component_offset", m_jetComponentOffset);
  BRANCH("jet_component_source", m_jetComponentSource);
  BRANCH("jet_component_id", m_jetComponentId);
#undef BRANCH
  return Fun4AllReturnCodes::EVENT_OK;
}

void JetChargeTreeMaker::resetBranches()
{
  m_run = -1;
  m_event = -1;
  m_vertexZ = std::numeric_limits<float>::quiet_NaN();
  m_nTracks = 0;
  m_nJets = 0;
#define CLEAR(member) member.clear()
  CLEAR(m_trackId); CLEAR(m_trackCharge); CLEAR(m_trackCrossing);
  CLEAR(m_trackVertexId); CLEAR(m_trackNdf);
  CLEAR(m_trackNMvtx); CLEAR(m_trackNIntt); CLEAR(m_trackNTpc); CLEAR(m_trackNTpot);
  CLEAR(m_trackPx); CLEAR(m_trackPy); CLEAR(m_trackPz);
  CLEAR(m_trackPt); CLEAR(m_trackEta); CLEAR(m_trackPhi);
  CLEAR(m_trackX); CLEAR(m_trackY); CLEAR(m_trackZ);
  CLEAR(m_trackQuality); CLEAR(m_trackChi2);
  CLEAR(m_trackDca2d); CLEAR(m_trackDca2dError);
  CLEAR(m_trackDca3dXY); CLEAR(m_trackDca3dZ);
  CLEAR(m_jetId); CLEAR(m_jetPx); CLEAR(m_jetPy); CLEAR(m_jetPz);
  CLEAR(m_jetE); CLEAR(m_jetPt); CLEAR(m_jetEt);
  CLEAR(m_jetEta); CLEAR(m_jetPhi); CLEAR(m_jetMass);
  CLEAR(m_jetEmcalFrac); CLEAR(m_jetIhcalFrac); CLEAR(m_jetOhcalFrac);
  CLEAR(m_jetComponentOffset); CLEAR(m_jetComponentSource); CLEAR(m_jetComponentId);
#undef CLEAR
}

int JetChargeTreeMaker::process_event(PHCompositeNode* topNode)
{
  resetBranches();
  m_entryIndex = m_eventIndex++;

  const auto* tracks = findNode::getClass<SvtxTrackMap>(topNode, m_trackNode);
  auto* jets = findNode::getClass<JetContainer>(topNode, m_jetNode);
  if (!tracks || !jets)
  {
    std::cerr << Name() << ": missing required input "
              << (!tracks ? m_trackNode : m_jetNode) << '\n';
    return Fun4AllReturnCodes::ABORTRUN;
  }

  const auto* header = findNode::getClass<EventHeader>(topNode, "EventHeader");
  if (header)
  {
    m_run = header->get_RunNumber();
    m_event = header->get_EvtSequence();
  }

  const auto* vertices = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");
  if (vertices && !vertices->empty() && vertices->begin()->second)
    m_vertexZ = vertices->begin()->second->get_z();

  for (const auto& [key, track] : *tracks)
  {
    if (!track || !std::isfinite(track->get_pt()) ||
        track->get_pt() < m_trackPtMin ||
        !std::isfinite(track->get_quality()) ||
        track->get_quality() > m_trackQualityMax ||
        (m_requireCrossingZero && track->get_crossing() != 0)) continue;

    const auto keys = clusterKeys(track);
    const int ntpc = countDetector(keys, TrkrDefs::tpcId);
    if (ntpc < m_minTpcClusters) continue;

    m_trackId.push_back(key);
    m_trackCharge.push_back(track->get_charge());
    m_trackCrossing.push_back(track->get_crossing());
    m_trackVertexId.push_back(track->get_vertex_id());
    m_trackNdf.push_back(static_cast<int>(track->get_ndf()));
    m_trackNMvtx.push_back(countDetector(keys, TrkrDefs::mvtxId));
    m_trackNIntt.push_back(countDetector(keys, TrkrDefs::inttId));
    m_trackNTpc.push_back(ntpc);
    m_trackNTpot.push_back(countDetector(keys, TrkrDefs::micromegasId));
    m_trackPx.push_back(track->get_px());
    m_trackPy.push_back(track->get_py());
    m_trackPz.push_back(track->get_pz());
    m_trackPt.push_back(track->get_pt());
    m_trackEta.push_back(track->get_eta());
    m_trackPhi.push_back(track->get_phi());
    m_trackX.push_back(track->get_x());
    m_trackY.push_back(track->get_y());
    m_trackZ.push_back(track->get_z());
    m_trackQuality.push_back(track->get_quality());
    m_trackChi2.push_back(track->get_chisq());
    m_trackDca2d.push_back(track->get_dca2d());
    m_trackDca2dError.push_back(track->get_dca2d_error());
    m_trackDca3dXY.push_back(track->get_dca3d_xy());
    m_trackDca3dZ.push_back(track->get_dca3d_z());
  }

  m_jetComponentOffset.push_back(0);
  for (auto* jet : *jets)
  {
    if (!jet || !std::isfinite(jet->get_pt()) ||
        jet->get_pt() < m_jetPtMin) continue;

    m_jetId.push_back(jet->get_id());
    m_jetPx.push_back(jet->get_px());
    m_jetPy.push_back(jet->get_py());
    m_jetPz.push_back(jet->get_pz());
    m_jetE.push_back(jet->get_e());
    m_jetPt.push_back(jet->get_pt());
    m_jetEt.push_back(jet->get_et());
    m_jetEta.push_back(jet->get_eta());
    m_jetPhi.push_back(jet->get_phi());
    m_jetMass.push_back(jet->get_mass());
    m_jetEmcalFrac.push_back(jet->get_emcal_frac());
    m_jetIhcalFrac.push_back(jet->get_ihcal_frac());
    m_jetOhcalFrac.push_back(jet->get_ohcal_frac());

    for (const auto& component : jet->get_comp_vec())
    {
      m_jetComponentSource.push_back(static_cast<int>(component.first));
      m_jetComponentId.push_back(component.second);
    }
    m_jetComponentOffset.push_back(
        static_cast<unsigned int>(m_jetComponentId.size()));
  }

  m_nTracks = static_cast<int>(m_trackId.size());
  m_nJets = static_cast<int>(m_jetId.size());
  m_tree->Fill(); // include events with zero accepted tracks or zero jets

  if (Verbosity() > 0 && m_entryIndex % 100 == 0)
    std::cout << Name() << ": entry " << m_entryIndex
              << " run/event " << m_run << '/' << m_event
              << " tracks " << m_nTracks << " jets " << m_nJets << '\n';

  return Fun4AllReturnCodes::EVENT_OK;
}

int JetChargeTreeMaker::End(PHCompositeNode*)
{
  if (m_output)
  {
    m_output->cd();
    if (m_tree) m_tree->Write();
    m_output->Close();
    delete m_output;
    m_output = nullptr;
    m_tree = nullptr;
  }
  std::cout << Name() << ": saved " << m_eventIndex
            << " event entries to " << m_outputName << '\n';
  return Fun4AllReturnCodes::EVENT_OK;
}
