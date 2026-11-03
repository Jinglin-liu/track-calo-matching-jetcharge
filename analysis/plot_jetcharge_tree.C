// Backwards-compatible one-command entry point.
// For large productions prefer make_jetcharge_histograms.C (TChain / groups),
// then draw_jetcharge_histograms.C (no TTree reading).
//
// Legacy example:
// root -l -b -q 'plot_jetcharge_tree.C("input.root","plots",5.0,0.7)'
//
// New wildcard example:
// root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_*.root","plots_run79516",5.0,0.7)'

#include "make_jetcharge_histograms.C"
#include "draw_jetcharge_histograms.C"

void plot_jetcharge_tree(const char* filename,
                         const char* outputDir = "jetcharge_plots",
                         double minChargeJetPt = 0.0,
                         double maxAbsEta = 0.7)
{
  if (!filename || !*filename || !outputDir || !*outputDir)
  {
    std::cerr << "ERROR: invalid filename/output directory.\n";
    return;
  }
  const std::string histPath = std::string(outputDir) + "/jetcharge_histograms.root";
  if (!make_jetcharge_histograms(filename, histPath.c_str(),
                                 minChargeJetPt, maxAbsEta))
  {
    std::cerr << "ERROR: could not generate histograms; skipping drawing.\n";
    return;
  }
  if (!draw_jetcharge_histograms(histPath.c_str(), outputDir))
    std::cerr << "ERROR: histogram generation succeeded but drawing failed.\n";
}
