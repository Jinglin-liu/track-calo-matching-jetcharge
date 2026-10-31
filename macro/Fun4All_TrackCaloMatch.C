// Direct TrackToCalo tutorial.
//
// The macro accepts several tracking DST levels and only executes the
// reconstruction stages missing from the input.  Keep this common workflow
// synchronized with Fun4All_KFP_Conv.C; only the final analysis module differs.

#include <fun4all/Fun4AllUtils.h>
#include <G4_ActsGeom.C>
#include <HIJetReco.C>
#include <G4_Global.C>
#include <G4_Magnet.C>
#include <G4_Mbd.C>
#include <GlobalVariables.C>
#include <Trkr_Clustering.C>
#include <Trkr_LaserClustering.C>
#include <Trkr_Reco.C>
#include <Trkr_RecoInit.C>
#include <Trkr_TpcReadoutInit.C>
#include <Calo_Calib.C>

#include <ffamodules/CDBInterface.h>

#include <fun4all/Fun4AllDstInputManager.h>
#include <fun4all/Fun4AllDstOutputManager.h>
#include <fun4all/Fun4AllInputManager.h>
#include <fun4all/Fun4AllOutputManager.h>
#include <fun4all/Fun4AllRunNodeInputManager.h>
#include <fun4all/Fun4AllServer.h>

#include <phool/recoConsts.h>

#include <trackreco/PHActsTrackProjection.h>

#include <jetbase/FastJetAlgo.h>
#include <jetbase/Jet.h>
#include <jetbase/JetReco.h>
#include <jetbase/TowerJetInput.h>

#include <cmath>
#include <vector>
#include <jetbackground/RetowerCEMC.h>

#include <caloreco/CaloGeomMapping.h>
#include <caloreco/RawClusterBuilderTemplate.h>

#include <track_to_calo/TrackToCalo.h>
#include <track_to_calo/JetChargeTreeMaker.h>
#include <trackbase_historic/SvtxTrack.h>

#include <stdio.h>

namespace TrackToCaloReco
{
    // These values mirror the reconstruction switches used by the KFP
    // conversion example.  Matching-specific cuts are grouped near the final
    // TrackToCalo registration block.
    bool get_detector_info = true;
    bool get_dEdx_info = true;
    bool get_calo_info = true;
    float cemc_proj_radius = 102.9;
}

// Filter tower inputs before FastJet, without modifying central sPHENIX libraries.
// Jet::get_pt() is a magnitude, including for negative-energy Sub1 towers.
class PtCutTowerJetInput : public TowerJetInput
{
 public:
    PtCutTowerJetInput(Jet::SRC source, double minPt,
                       const std::string &prefix = "TOWERINFO_CALIB")
      : TowerJetInput(source, prefix)
      , m_minPt(minPt)
    {
    }

    std::vector<Jet *> get_input(PHCompositeNode *topNode) override
    {
        auto inputs = TowerJetInput::get_input(topNode);
        if (m_minPt <= 0.0) return inputs;

        std::vector<Jet *> accepted;
        accepted.reserve(inputs.size());
        for (auto *tower : inputs)
        {
            if (tower && std::isfinite(tower->get_pt()) &&
                tower->get_pt() >= m_minPt)
            {
                accepted.push_back(tower);
            }
            else
            {
                delete tower;
            }
        }
        return accepted;
    }

 private:
    double m_minPt;
};

using namespace TrackToCaloReco;

R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libffamodules.so)
R__LOAD_LIBRARY(libphool.so)
R__LOAD_LIBRARY(libcdbobjects.so)
R__LOAD_LIBRARY(libtrack_to_calo.so)
R__LOAD_LIBRARY(libjetbase.so)
R__LOAD_LIBRARY(libjetbackground.so)

bool IsListFile = false;
bool IsDstFile = false;
bool DoUnpacking = false;
bool DoClustering = false;
bool DoSeeding = false;
bool DoFitting = false;

void CheckDstType(const std::string inputDST)
{
    // Start from the latest reconstruction stage already present in the DST.
    // This is why the same macro can accept streaming-event, cluster, seed, or
    // fitted-track input.
    if (inputDST.find("DST_STREAMING_EVENT") != std::string::npos)
    {
        DoUnpacking = true;
        DoClustering = true;
        DoSeeding = true;
        DoFitting = true;
    }
    else if (inputDST.find("DST_TRKR_CLUSTER") != std::string::npos)
    {
        DoUnpacking = false;
        DoClustering = false;
        DoSeeding = true;
        DoFitting = true;
    }
    else if (inputDST.find("DST_TRKR_SEED") != std::string::npos)
    {
        DoUnpacking = false;
        DoClustering = false;
        DoSeeding = false;
        DoFitting = true;
    }
    else if (inputDST.find("DST_TRKR_TRACKS") != std::string::npos)
    {
        DoUnpacking = false;
        DoClustering = false;
        DoSeeding = false;
        DoFitting = false;
    }
    return;
}

std::string GetRunInfo(const std::string &filename)
{
    const std::vector<std::string> runspecies_patterns = {"run2pp", "run3pp", "run3auau"};

    boost::char_separator<char> sep("_");
    boost::tokenizer<boost::char_separator<char>> tok(filename, sep);

    for (const auto &t : tok)
    {
        for (const auto &pattern : runspecies_patterns)
        {
            if (t == pattern)
            {
                return t;
            }
        }
    }

    std::cout << "Cannot extract run info from filename!" << std::endl;
    return "";
}

void Fun4All_TrackCaloMatch(const int nEvents = 100,
                const std::string inputDST = "DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root",
                const std::string outDir = "./",
                const int nSkip = 0,
                const bool convertSeeds = false,
                const bool writeJetChargeTree = true,
                const std::string jetMode = "unsubtracted",
                const bool enableTrackToCalo = false,
                const double minTowerPt = 0.0)
{
    if (writeJetChargeTree && jetMode != "unsubtracted" && jetMode != "sub1")
    {
        std::cerr << "Invalid jetMode: " << jetMode << std::endl;
        gSystem->Exit(1);
        return;
    }

    if (!std::isfinite(minTowerPt) || minTowerPt < 0.0)
    {
        std::cerr << "Invalid minTowerPt: " << minTowerPt
                  << " (must be finite and >= 0 GeV/c)" << std::endl;
        gSystem->Exit(1);
        return;
    }

    auto se = Fun4AllServer::instance();
    se->Verbosity(1);
    // se->VerbosityDownscale(500);
    auto rc = recoConsts::instance();

    if (inputDST.find(".list") != std::string::npos)
    {
        IsListFile = true;
        std::cout << "Input is list file" << std::endl;
    }
    if (inputDST.find(".root") != std::string::npos)
    {
        IsDstFile = true;
        std::cout << "Input is dst file" << std::endl;
    }
    if (!IsListFile && !IsDstFile)
    {
        std::cout << "Check your input! Exit" << std::endl;
        gSystem->Exit(0);
    }

    int runnumber = std::numeric_limits<int>::quiet_NaN();
    int segment = std::numeric_limits<int>::quiet_NaN();
    std::string runspecies = "";

    if (IsListFile)
    {
        std::ifstream ifs(inputDST);
        std::string filepath;
        int i = 0;
        while (std::getline(ifs, filepath))
        {
            std::cout << "Adding DST with filepath: " << filepath << std::endl;
            if (i == 0)
            {
                std::pair<int, int> runseg = Fun4AllUtils::GetRunSegment(filepath);
                runnumber = runseg.first;
                segment = runseg.second;
                runspecies = GetRunInfo(filepath);
                CheckDstType(filepath);
            }
            std::string inputname = "InputManager" + std::to_string(i);
            auto *hitsin = new Fun4AllDstInputManager(inputname);
            hitsin->fileopen(filepath);
            se->registerInputManager(hitsin);
            i++;
        }
    }

    if (IsDstFile)
    {
        std::pair<int, int> runseg = Fun4AllUtils::GetRunSegment(inputDST);
        runnumber = runseg.first;
        segment = runseg.second;
        runspecies = GetRunInfo(inputDST);

        CheckDstType(inputDST);

        auto *hitsin = new Fun4AllDstInputManager("InputManager");
        hitsin->fileopen(inputDST);
        se->registerInputManager(hitsin);
    }

    rc->set_IntFlag("RUNNUMBER", runnumber);
    rc->set_IntFlag("RUNSEGMENT", segment);
    rc->set_uint64Flag("TIMESTAMP", runnumber);

    Enable::CDB = true;
    rc->set_StringFlag("CDB_GLOBALTAG", "newcdbtag");
    rc->set_uint64Flag("TIMESTAMP", runnumber);

    std::stringstream nice_runnumber;
    nice_runnumber << std::setw(8) << std::setfill('0') << to_string(runnumber);

    int rounded_up = 100 * (std::ceil((float)runnumber / 100));
    std::stringstream nice_rounded_up;
    nice_rounded_up << std::setw(8) << std::setfill('0') << to_string(rounded_up);

    int rounded_down = 100 * (std::floor((float)runnumber / 100));
    std::stringstream nice_rounded_down;
    nice_rounded_down << std::setw(8) << std::setfill('0') << to_string(rounded_down);

    std::stringstream nice_segment;
    nice_segment << std::setw(5) << std::setfill('0') << to_string(segment);

    std::stringstream nice_skip;
    nice_skip << std::setw(5) << std::setfill('0') << to_string(nSkip);

    if (!DoSeeding && (get_dEdx_info || get_detector_info))
    {
        std::string clus_anacdbver = "ana538_2025p011_v001";
        std::string clus_file = "";
        if (runspecies == "run3pp" || runspecies == "run3auau")
        {
            clus_file = "/sphenix/lustre01/sphnxpro/production/"
                + runspecies + "/physics/" + clus_anacdbver
                + "/DST_TRKR_CLUSTER/run_" + nice_rounded_down.str() + "_" + nice_rounded_up.str()
                + "/DST_TRKR_CLUSTER_" + runspecies + "_" + clus_anacdbver + "-" + nice_runnumber.str() + "-" + nice_segment.str() + ".root";
        }
        if (runspecies == "run2pp")
        {
            clus_file = "/sphenix/lustre01/sphnxpro/production/"
                + runspecies + "/physics/" + clus_anacdbver
                + "/DST_TRKR_CLUSTER/run_" + nice_rounded_down.str() + "_" + nice_rounded_up.str()
                + "/dst/DST_TRKR_CLUSTER_" + runspecies + "_" + clus_anacdbver + "-" + nice_runnumber.str() + "-" + nice_segment.str() + ".root";
        }
        std::cout << "Input cluster DST: " << clus_file << std::endl;

        auto hitsinclus = new Fun4AllDstInputManager("ClusterInputManager");
        hitsinclus->fileopen(clus_file);
        se->registerInputManager(hitsinclus);
    }

    if (get_calo_info)
    {
        // IMPORTANT: this segment conversion and the production tag are
        // specific to the input production used to prepare the tutorial.
        // Verify event synchronization before launching a large campaign.
        int ratio_nCalo_over_nTrkr = 100; // number of events per calo DST divided by number of events per tracking DST
        std::stringstream nice_segment_calo;
        nice_segment_calo << std::setw(5) << std::setfill('0') << to_string(segment / ratio_nCalo_over_nTrkr);

        std::string calo_anacdbver = "pro001_pcdb001_v001";
        std::string calo_file = "";
        calo_file = "/sphenix/lustre01/sphnxpro/production/"
                + runspecies + "/physics/" + calo_anacdbver
                + "/DST_CALOFITTING/run_" + nice_rounded_down.str() + "_" + nice_rounded_up.str()
                + "/DST_CALOFITTING_" + runspecies + "_" + calo_anacdbver + "-" + nice_runnumber.str() + "-" + nice_segment_calo.str() + ".root";
        std::cout << "Input calo DST: " << calo_file << std::endl;

        auto hitsincalo = new Fun4AllDstInputManager("CaloInputManager");
        hitsincalo->fileopen(calo_file);
        se->registerInputManager(hitsincalo);
    }

    std::string geofile = CDBInterface::instance()->getUrl("Tracking_Geometry");

    Fun4AllRunNodeInputManager *ingeo = new Fun4AllRunNodeInputManager("GeoIn");
    ingeo->AddFile(geofile);
    se->registerInputManager(ingeo);

    // set flags
    TRACKING::streaming_mode = true;

    Enable::MVTX_APPLYMISALIGNMENT = true;
    ACTSGEOM::mvtx_applymisalignment = Enable::MVTX_APPLYMISALIGNMENT;

    // distortion calibration mode
    /*
     * set to true to enable residuals in the TPC with
     * TPC clusters not participating to the ACTS track fit
     */
    G4TRACKING::SC_CALIBMODE = false;

    TpcReadoutInit(runnumber);
    // these lines show how to override the drift velocity and time offset values set in TpcReadoutInit
    // G4TPC::tpc_drift_velocity_reco = 0.0073844; // cm/ns
    // TpcClusterZCrossingCorrection::_vdrift = G4TPC::tpc_drift_velocity_reco;
    // G4TPC::tpc_tzero_reco = -5*50;  // ns
    std::cout << " run: " << runnumber << " samples: " << TRACKING::reco_tpc_maxtime_sample << " pre: " << TRACKING::reco_tpc_time_presample << " vdrift: " << G4TPC::tpc_drift_velocity_reco << std::endl;

    G4TPC::REJECT_LASER_EVENTS = true;
    G4TPC::ENABLE_MODULE_EDGE_CORRECTIONS = true;
    // Flag for running the tpc hit unpacker with zero suppression on
    TRACKING::tpc_zero_supp = true;

    // to turn on the default static corrections, enable the two lines below
    G4TPC::ENABLE_STATIC_CORRECTIONS = true;
    G4TPC::USE_PHI_AS_RAD_STATIC_CORRECTIONS = false;

    // to turn on the average corrections, enable the three lines below
    // note: these are designed to be used only if static corrections are also applied
    G4TPC::ENABLE_AVERAGE_CORRECTIONS = true;
    G4TPC::USE_PHI_AS_RAD_AVERAGE_CORRECTIONS = false;
    // to use a custom file instead of the database file:
    G4TPC::average_correction_filename = CDBInterface::instance()->getUrl("TPC_LAMINATION_FIT_CORRECTION");

    G4MAGNET::magfield_rescale = 1;
    TrackingInit();

    if (DoUnpacking)
    {
        for (int felix = 0; felix < 6; felix++)
        {
            Mvtx_HitUnpacking(std::to_string(felix));
        }
        for (int server = 0; server < 8; server++)
        {
            Intt_HitUnpacking(std::to_string(server));
        }
        std::ostringstream ebdcname;
        if (runspecies == "run3pp" || runspecies == "run3auau")
        {
            for (int ebdc = 0; ebdc < 24; ebdc++)
            {
                for (int endpoint = 0; endpoint < 2; endpoint++)
                {
                    ebdcname.str("");
                    if (ebdc < 10)
                    {
                        ebdcname << "0";
                    }
                    ebdcname << ebdc << "_" << endpoint;
                    Tpc_HitUnpacking(ebdcname.str());
                }
            }
        }
        if (runspecies == "run2pp")
        {
            for (int ebdc = 0; ebdc < 24; ebdc++)
            {
                ebdcname.str("");
                if (ebdc < 10)
                {
                    ebdcname << "0";
                }
                ebdcname << ebdc;
                Tpc_HitUnpacking(ebdcname.str());
            }
        }

        Micromegas_HitUnpacking();
    }

    if (DoClustering)
    {
        Mvtx_Clustering();

        Intt_Clustering();

        Tpc_LaserEventIdentifying();

        TPC_Clustering_run2pp();

        Micromegas_Clustering();

        Reject_Laser_Events();
    }

    if (DoSeeding)
    {
        Tracking_Reco_TrackSeed_run2pp();
    }

    if (DoFitting)
    {
        Tracking_Reco_TrackMatching_run2pp();

        G4TRACKING::convert_seeds_to_svtxtracks = convertSeeds;
        std::cout << "Converting to seeds : " << G4TRACKING::convert_seeds_to_svtxtracks << std::endl;
        /*
         * Either converts seeds to tracks with a straight line/helix fit
         * or run the full Acts track kalman filter fit
         */
        if (G4TRACKING::convert_seeds_to_svtxtracks)
        {
            auto *converter = new TrackSeedTrackMapConverter;
            // Default set to full SvtxTrackSeeds. Can be set to
            // SiliconTrackSeedContainer or TpcTrackSeedContainer
            converter->setTrackSeedName("SvtxTrackSeedContainer");
            converter->setFieldMap(G4MAGNET::magfield_tracking);
            converter->Verbosity(0);
            se->registerSubsystem(converter);
        }
        else
        {
            Tracking_Reco_TrackFit_run2pp();
        }

        // vertexing and propagation to vertex
        Tracking_Reco_Vertex_run2pp();
    }

    if (get_calo_info)
    {
        std::cout << "Begin my calo reco" << std::endl;

        Process_Calo_Calib();

        Global_Reco();

        bool doEMcalRadiusCorr = true;
        auto projection = new PHActsTrackProjection("CaloProjection");
        if (doEMcalRadiusCorr)
        {
            projection->setLayerRadius(SvtxTrack::CEMC, cemc_proj_radius);
        }
        se->registerSubsystem(projection);

	// no longer needed, TOWERGEOM_CEMC_DETAILED is already loaded by the calo_geo
	/*
        CaloGeomMapping *cgm = new CaloGeomMapping();
        cgm->set_detector_name("CEMC");
        cgm->set_UseDetailedGeometry(true);
        se->registerSubsystem(cgm);
	*/

        // The standard Process_Calo_Calib() above already builds CEMC clusters.
        // Only retain this legacy second builder for TrackToCalo-enabled runs.
        // NOTE: when enabled, both builders still target CLUSTERINFO_CEMC.
        if (enableTrackToCalo)
        {
            //////////////////
            // Clusters
            std::cout << "Building clusters" << std::endl;
            RawClusterBuilderTemplate *ClusterBuilder = new RawClusterBuilderTemplate("EmcRawClusterBuilderTemplate");
            ClusterBuilder->Detector("CEMC");
            ClusterBuilder->set_UseDetailedGeometry(true);
            ClusterBuilder->set_threshold_energy(0.070); // for when using basic calibration
            const char *calibrationRoot = getenv("CALIBRATIONROOT");
            if (!calibrationRoot)
            {
                std::cerr << "CALIBRATIONROOT is not set; did you run sphenix_setup.sh?" << std::endl;
                gSystem->Exit(1);
                return;
            }
            std::string emc_prof = calibrationRoot;
            emc_prof += "/EmcProfile/CEMCprof_Thresh30MeV.root";
            // ClusterBuilder->set_UseAltZVertex(3); //0: GlobalVertexMap, 1: MbdVertexMap, 2: Nothing, 3: G4TruthInfo
            ClusterBuilder->LoadProfile(emc_prof);
            ClusterBuilder->set_UseTowerInfo(1); // to use towerinfo objects rather than old RawTower
            se->registerSubsystem(ClusterBuilder);
        }
    }


    // Optional event-level jet-charge inputs. Register after calibrated towers
    // and global vertex reconstruction, before the output writer. Retowered
    // CEMC, HCALIN and HCALOUT are clustered into anti-kT R=0.4 calo jets.
    // Track-jet geometric association is intentionally deferred to analysis.
    if (writeJetChargeTree)
    {
        std::cout << "Calo-jet constituent pT threshold: " << minTowerPt
                  << " GeV/c (0 = original tower inputs)" << std::endl;
        if (!get_calo_info)
        {
            std::cerr << "JetChargeTreeMaker requires get_calo_info" << std::endl;
            gSystem->Exit(1);
            return;
        }

        if (jetMode == "unsubtracted")
        {
            auto *retower = new RetowerCEMC("RetowerCEMC_JetCharge");
            retower->set_towerinfo(true);
            retower->set_frac_cut(0.5);
            se->registerSubsystem(retower);

            auto *jetReco = new JetReco("JetReco_JetCharge");
            jetReco->add_input(new PtCutTowerJetInput(Jet::CEMC_TOWERINFO_RETOWER, minTowerPt));
            jetReco->add_input(new PtCutTowerJetInput(Jet::HCALIN_TOWERINFO, minTowerPt));
            jetReco->add_input(new PtCutTowerJetInput(Jet::HCALOUT_TOWERINFO, minTowerPt));
            jetReco->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.4),
                              "AntiKt_unsubtracted_r04");
            jetReco->set_algo_node("ANTIKT");
            jetReco->set_input_node("TOWER");
            se->registerSubsystem(jetReco);
        }
        else if (jetMode == "sub1")
        {
            // Official sPHENIX HIJetReco.C tower subtraction sequence:
            // retower -> raw R=0.2 seeds -> background -> subtracted seeds
            // -> refined background -> subtracted towers -> anti-kT jets.
            HIJETS::tower_prefix = "TOWERINFO_CALIB";
            HIJETS::do_flow = 0;
            HIJETS::do_CS = false;
            HIJETS::is_pp = true;
            Enable::HIJETS_MC = false;
            Enable::HIJETS_TRUTH = false;
            Enable::HIJETS_TOWER = true;
            Enable::HIJETS_TRACK = false;
            Enable::HIJETS_PFLOW = false;
            HIJetReco();

            if (minTowerPt > 0.0)
            {
                // Keep official seed finding and background subtraction unchanged.
                // Cluster an additional final R=0.4 collection using |tower pT|
                // >= minTowerPt. Negative-energy Sub1 towers are retained by
                // magnitude and FastJetAlgoSub handles their signed energies.
                auto *filteredReco = new JetReco("JetReco_Sub1_TowerPtCut");
                auto *cemcInput = new PtCutTowerJetInput(
                    Jet::CEMC_TOWERINFO_SUB1, minTowerPt, HIJETS::tower_prefix);
                auto *ihcalInput = new PtCutTowerJetInput(
                    Jet::HCALIN_TOWERINFO_SUB1, minTowerPt, HIJETS::tower_prefix);
                auto *ohcalInput = new PtCutTowerJetInput(
                    Jet::HCALOUT_TOWERINFO_SUB1, minTowerPt, HIJETS::tower_prefix);
                if (HIJETS::do_vertex_type)
                {
                    cemcInput->set_GlobalVertexType(HIJETS::vertex_type);
                    ihcalInput->set_GlobalVertexType(HIJETS::vertex_type);
                    ohcalInput->set_GlobalVertexType(HIJETS::vertex_type);
                }
                filteredReco->add_input(cemcInput);
                filteredReco->add_input(ihcalInput);
                filteredReco->add_input(ohcalInput);
                filteredReco->add_algo(HIJETS::GetFJAlgo(0.4),
                                       "AntiKt_Tower_r04_Sub1_PtCut");
                filteredReco->set_algo_node(HIJETS::jet_node);
                filteredReco->set_input_node("TOWER");
                se->registerSubsystem(filteredReco);
            }
        }
        else
        {
            std::cerr << "Unknown jetMode '" << jetMode
                      << "'; expected 'unsubtracted' or 'sub1'" << std::endl;
            gSystem->Exit(1);
            return;
        }

        std::string jetTreePath = outDir;
        if (!jetTreePath.empty() && jetTreePath.back() != '/')
        {
            jetTreePath += "/";
        }
        gSystem->mkdir(jetTreePath.c_str(), true);
        jetTreePath += "jet_charge_inputs_" + jetMode + "_" + nice_runnumber.str()
                    + "_" + nice_segment.str()
                    + "_" + nice_skip.str() + ".root";

        auto *jetTree = new JetChargeTreeMaker("JetChargeTreeMaker");
        jetTree->setOutputFile(jetTreePath);
        const std::string jetNode = (jetMode == "unsubtracted")
            ? "AntiKt_unsubtracted_r04"
            : (minTowerPt > 0.0 ? "AntiKt_Tower_r04_Sub1_PtCut"
                                : "AntiKt_Tower_r04_Sub1");
        std::cout << "JetChargeTreeMaker jet node: " << jetNode << std::endl;
        jetTree->setJetNode(jetNode);
        jetTree->setUseSubtractedTowers(jetMode == "sub1");
        jetTree->setTrackPtMin(0.5); // 0.2 GeV/c by track calo matching
        jetTree->setTrackQualityMax(300.);
        jetTree->setMinTpcClusters(22);
        jetTree->requireCrossingZero(true);
        jetTree->setJetPtMin(5.0);
        // JetChargeTreeMaker logs an event summary every 100 entries when Verbosity > 0.
        jetTree->Verbosity(1);
        se->registerSubsystem(jetTree);
    }

    // ---------------------------------------------------------------------
    // The reconstruction workflow above is kept aligned with
    // Fun4All_KFP_Conv.C.  This final registration block is the direct
    // TrackToCalo-specific part of the tutorial.
    // ---------------------------------------------------------------------
    std::string output_path;
    if (enableTrackToCalo)
    {
        output_path = outDir;
        if (!output_path.empty() && output_path.back() != '/')
        {
            output_path += "/";
        }
        gSystem->mkdir(output_path.c_str(), true);
        output_path += "track_calo_pairs_" + nice_runnumber.str() + "_" + nice_segment.str() + "_" + nice_skip.str() + ".root";
    
        auto *matcher = new TrackToCalo("TrackToCalo");
        matcher->SetOutputFileName(output_path);
        matcher->EMcalRadiusUser(true);
        matcher->setEMcalRadius(cemc_proj_radius);
        matcher->setTrackPtLowCut(0.2);
        matcher->setTrackQuality(300.);
        matcher->setnTpcClusters(22);
        matcher->requireCrossingZero(true);
        matcher->setEmcalELowCut(0.2);
        matcher->setdphicut(0.10);
        matcher->setdzcut(10.);
        matcher->Verbosity(1);
        se->registerSubsystem(matcher);
    }
    else
    {
        std::cout << "TrackToCalo disabled (no track_calo_pairs output)." << std::endl;
    }

    se->skip(nSkip);
    se->run(nEvents);
    se->End();
    se->PrintTimer();

    delete se;

    if (enableTrackToCalo)
    {
        std::cout << "Finished. Track-calo pairs were written to " << output_path
                  << std::endl;
    }
    else
    {
        std::cout << "Finished. TrackToCalo was disabled." << std::endl;
    }
    gSystem->Exit(0);
}
