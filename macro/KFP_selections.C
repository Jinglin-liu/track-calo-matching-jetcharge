#include <fun4all/Fun4AllServer.h>

#include <calotrigger/TriggerRunInfoReco.h>
#include <globalvertex/GlobalVertexReco.h>

#include <kfparticleqa/QAKFParticle.h>
#include <kfparticleqa/QAKFParticleTrackPtAsymmetry.h>

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wundefined-internal"

#include <kfparticle_sphenix/KFParticle_sPHENIX.h>

#pragma GCC diagnostic pop

R__LOAD_LIBRARY(libkfparticle_sphenix.so)
R__LOAD_LIBRARY(libcalotrigger.so)
R__LOAD_LIBRARY(libkfparticleqa.so)

// ---------------------------------------------------------------------------
// Photon-conversion tutorial configuration
// gamma -> e+ e-
// ---------------------------------------------------------------------------
namespace ConversionReco
{
  int VERBOSITY_CONV = 0;

  bool run_ee_reco = true;

  std::string output_dir = "./"; //Top dir of where the output nTuples will be written
  std::string kfp_header = "outputKFParticle_";
  std::string processing_folder = "inReconstruction/";
  std::string trailer = ".root";

  // https://wiki.bnl.gov/sPHENIX/index.php/KFParticle
  std::string ee_decay_descriptor = "[gamma -> e^+ e^-]cc"; //See twiki on how to set this
  std::string ee_reconstruction_name = "ee_reco"; //Used for naming output folder, file and node
  std::string ee_output_reco_file;
  std::string ee_output_dir;

  bool save_kfpntuple = true;
  bool use_pid = false;
  bool save_tracks_to_DST = true;
  bool dont_use_global_vertex = false;
  bool require_track_and_vertex_match = false;
  bool save_all_vtx_info = false;
  bool use_2D_matching = false;
  bool get_trigger_info = true;
  bool get_detector_info = true;
  bool get_dEdx_info = true;
  bool get_calo_info = true;
  float pid_frac = 0.4;
  float cemc_proj_radius = 102.9; //Virgile recommendation according to DetailedCalorimeterGeometry
  bool extraolate_tracks_to_secondary_vertex = true; // Set to false to ensure the pT map is accurate for asymmetry study
};  // namespace ConversionReco

using namespace ConversionReco;

void init_kfp_dependencies()
{
  //dE/dx needs TRKR_CLUSTER and CYLINDERCELLGEOM_SVTX which need to be in the DST or loaded from a geo file
  Fun4AllServer *se = Fun4AllServer::instance();

  GlobalVertexReco* gblvertex = new GlobalVertexReco();
  gblvertex->Verbosity(VERBOSITY_CONV);
  se->registerSubsystem(gblvertex);


  if (get_trigger_info)
  {
    TriggerRunInfoReco *triggerruninforeco = new TriggerRunInfoReco();
    se->registerSubsystem(triggerruninforeco);
  }
}

void create_conv_directories(std::string reconstruction_name, std::string &final_output_dir, std::string &output_reco_file)
{
  std::string output_file_name = kfp_header + reconstruction_name + trailer;
  final_output_dir = output_dir + reconstruction_name + "/";
  std::string output_reco_dir = final_output_dir + processing_folder;
  output_reco_file = output_reco_dir + output_file_name;

  std::string makeDirectory = "mkdir -p " + output_reco_dir;
  system(makeDirectory.c_str());
}

void reconstruct_ee_mass()
{
  Fun4AllServer *se = Fun4AllServer::instance();

  KFParticle_sPHENIX *kfparticle = new KFParticle_sPHENIX(ee_reconstruction_name);

  kfparticle->setDecayDescriptor(ee_decay_descriptor);

  kfparticle->extraolateTracksToSV(extraolate_tracks_to_secondary_vertex);

  kfparticle->saveOutput(save_kfpntuple);

  //kfparticle->usePID(use_pid);
  //kfparticle->setPIDacceptFraction(pid_frac);
  kfparticle->dontUseGlobalVertex(dont_use_global_vertex);
  kfparticle->requireTrackVertexBunchCrossingMatch(require_track_and_vertex_match);
  kfparticle->getAllPVInfo(save_all_vtx_info);
  kfparticle->allowZeroMassTracks();
  kfparticle->use2Dmatching(use_2D_matching);
  kfparticle->getTriggerInfo(get_trigger_info);
  kfparticle->getDetectorInfo(get_detector_info);
  kfparticle->get_dEdx_info(get_dEdx_info);
  kfparticle->getCaloInfo(get_calo_info);
  kfparticle->saveDST(save_tracks_to_DST);
  kfparticle->setContainerName(ee_reconstruction_name);
  kfparticle->saveParticleContainer(true);
  kfparticle->magFieldFile("FIELDMAP_TRACKING");

  //PV to SV cuts
  kfparticle->constrainToPrimaryVertex();
  kfparticle->setMotherPV_DCA_StdDev(FLT_MAX);
  kfparticle->setFlightDistancechi2(-1.);
  kfparticle->setMinDIRA(-1.1);
  kfparticle->setMinDIRA_XY(-1.1);
  kfparticle->setDecayLengthRange(0., FLT_MAX);
  kfparticle->setDecayLengthRange_XY(-10000, FLT_MAX);
  kfparticle->setDecayTimeRange_XY(-10000, FLT_MAX);
  kfparticle->setDecayTimeRange(-10000, FLT_MAX);
  kfparticle->setMinDecayTimeSignificance(-1e5);
  kfparticle->setMinDecayLengthSignificance(-1e5);
  kfparticle->setMinDecayLengthSignificance_XY(-1e5);

  //Track parameters
  kfparticle->bunchCrossingZeroOnly(true);
  kfparticle->setMinimumTrackPT(0.2);
  kfparticle->setMaximumTrackPTchi2(FLT_MAX);
  kfparticle->setMinimumTrackPV_DCA_StdDev(-1.);
  kfparticle->setMinimumTrackPV_DCA_StdDev_XY(-1.);
  kfparticle->setMinimumTrackPV_DCA(-1.);
  kfparticle->setMinimumTrackPV_DCA_XY(-100.);
  kfparticle->setMaximumTrackchi2nDOF(300.);
  kfparticle->setMinMVTXhits(0);
  kfparticle->setMinINTThits(0);
  kfparticle->setMinTPChits(25);

  // Track-Calo matching.  PHActsTrackProjection in Fun4All_KFP_Conv.C must use the
  // same radius.  Plot the residuals before tightening these windows for a
  // different production.
  kfparticle->set_emcal_radius_user(cemc_proj_radius);
  //narrow window
  kfparticle->set_dphi_cut_low(-0.02); //rad
  kfparticle->set_dphi_cut_high(0.09); //rad
  kfparticle->set_dz_cut_low(-5); //cm
  kfparticle->set_dz_cut_high(5); //cm
  //loose window
  /*
  kfparticle->set_dphi_cut_low(-0.2); //rad
  kfparticle->set_dphi_cut_high(0.2); //rad
  kfparticle->set_dz_cut_low(-10); //cm
  kfparticle->set_dz_cut_high(10); //cm
  */
  kfparticle->set_emcal_e_low_cut(0.2); //GeV
  kfparticle->requireTrackEMCalMatch(true);

  //Vertex parameters
  kfparticle->setMaximumVertexchi2nDOF(FLT_MAX);
  kfparticle->setMaximumDaughterDCA(0.5);
  kfparticle->setMaximumDaughterDCA_XY(100);

  //Parent parameters
  kfparticle->setMotherPT(0);
  kfparticle->setMinimumMass(0.0);
  kfparticle->setMaximumMass(0.1);
  kfparticle->setMaximumMotherVertexVolume(0.1);

  kfparticle->setOutputName(ee_output_reco_file);

  se->registerSubsystem(kfparticle);
}

void end_kfparticle(std::string full_file_name, std::string final_path)
{
  ifstream file(full_file_name.c_str());
  if (file.good())
  {
    string moveOutput = "mv " + full_file_name + " " + final_path;
    system(moveOutput.c_str());
  }
}
