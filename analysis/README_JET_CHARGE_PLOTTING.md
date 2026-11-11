# Jet-charge QA plotting for Run 79516: no large TTree hadd needed

This workflow reads the production `JetChargeInputTree` files with ROOT `TChain`
and stores **26 small histograms**, not a copy of the original event TTrees.
`make_jetcharge_histograms.C` fills histograms; `draw_jetcharge_histograms.C`
draws PNG/PDF figures from a histogram file; `plot_jetcharge_tree.C` remains
a one-command compatibility wrapper.

The Run 79516 reference is **unsubtracted anti-kt R=0.4, saved jets pT >=
5 GeV/c, minTowerPt=0, TrackToCalo disabled**. The extra plotting cuts
used in the examples below are **minChargeJetPt=5.0 GeV/c, |eta|<0.7**.
The histogram maker deliberately preserves the original plotting macro's
binning, inclusive jet-pT spectra, and charge-selection definitions. The
pT-slice histogram `hCharge2to5` is empty for this production because
the tree itself saves only jets with pT>=5 GeV/c.

## Paths: what is read and what is written?

All commands below start from the **repository root**, unless noted.
The main production directory is:

```text
root/trackcalomatch_5GeV_unsub_run79516/
    jet_charge_inputs_unsubtracted_00079516_00000_00000.root
    jet_charge_inputs_unsubtracted_00079516_00000_01000.root
    jet_charge_inputs_unsubtracted_00079516_00001_00000.root
    ... (up to 37,824 original files)
```

The numbered Condor queue lists, which have **three columns**, remain in:

```text
fileLists/file_run79516_00.list
fileLists/file_run79516_01.list
...
fileLists/file_run79516_37.list
```

They contain rows such as:

```text
1000 DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root 0
1000 DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root 1000
```

These are **production input lists**, not direct `TChain` manifests. The
group script converts each row's tracking segment and skip to an exact
matching `jet_charge_inputs_unsubtracted_...root` output path and checks
that the file exists. It does not look for or read DST files.

By default, the group script creates:

```text
analysis/plots_run79516_unsubtracted/
    manifests/
        group_00.txt                   # one absolute ROOT path per line
        ...
        group_37.txt
    groups/
        group_00/jetcharge_histograms.root   # 30 histograms + metadata
        ...
        group_37/jetcharge_histograms.root
    run79516_histograms.root                 # combined histograms
    figures/
        jet_qa.png             jet_qa.pdf
        jet_charge_qa.png      jet_charge_qa.pdf
        jet_charge_pt_slices.png   jet_charge_pt_slices.pdf
        jet_pt_spectrum.png    jet_pt_spectrum.pdf
        jet_mass.png           jet_mass.pdf
```

Group histograms, the combined histograms and the figures are **additional**
small files; the original ~19 GB of production TTrees are not modified,
copied or merged. ROOT files for the histograms should be small compared
with the original tree dataset (exact size depends on ROOT and metadata).

## Recommended production workflow: 38 histogram groups

### 1. Get the current macros

```bash
cd /sphenix/user/jlnliu/projects/track-calo-matching/track-calo-matching-jetcharge
git switch main
git pull --ff-only
```

No recompilation of `libtrack_to_calo.so` is needed; only analysis macros
and scripts have changed. Run from an SDCC shell with ROOT initialized.

### 2. Fill all 38 groups

```bash
bash analysis/make_jetcharge_groups.sh --run 79516
```

The script uses the exact queue files `file_run79516_NN.list` to build
`analysis/plots_run79516_unsubtracted/manifests/group_NN.txt`. Each manifest
lists **absolute paths** to the expected original `.root` files, one
per line. It checks for missing files and repeated output paths.

For each group it runs `make_jetcharge_histograms.C` once (via `TChain`)
and saves exactly one histogram ROOT file in
`analysis/plots_run79516_unsubtracted/groups/group_NN/`.

This can take time: every selected event must still be read, but only the
needed branches (jet_pt, jet_eta, jet_phi, jet_mass, jet_charge,
jet_n_matched_tracks, n_jets and jet_charge_kappa) are accessed. The `jet_component_offset`, `jet_component_source`, `jet_component_energy`,
`jet_component_eta`, and `jet_component_phi` branches are read in addition to
jet kinematics. These branches must exist in every input file.

For a one-group test, *before processing the full set*:

```bash
bash analysis/make_jetcharge_groups.sh --run 79516 --group 00
```

The output is `analysis/plots_run79516_unsubtracted/groups/group_00/jetcharge_histograms.root`.
To rerun that group with the same output file, supply `--force`:

```bash
bash analysis/make_jetcharge_groups.sh --run 79516 --group 00 --force
```

Once you have processed group 00 (or if a full run was interrupted),
**resume without recomputing the completed groups** using:

```bash
bash analysis/make_jetcharge_groups.sh --run 79516 --skip-existing
```

The histogram-making, merging, and drawing macros now return **integer status codes**
(0 on success, nonzero on failure) so ROOT batch-mode execution works
correctly with the script's `set -e`. Earlier versions returned `bool`,
which could cause a successful `(bool) true` macro to stop the group script
at group 00. After pulling the updated macros, resume with
`--skip-existing` rather than rerunning completed groups.

`--skip-existing` trusts any nonempty group histogram file already on
disk, so only use it when the earlier groups used the same cuts and
reconstruction input. The merge macro checks these settings across all
groups. Alternatively use `--force` to regenerate every selected group.
Without either flag the group script refuses to overwrite existing files.

### 3. Merge the 38 histogram files (not TTrees)

From `analysis/`:

```bash
cd analysis
root -l -b -q 'merge_jetcharge_histograms.C("plots_run79516_unsubtracted/groups","plots_run79516_unsubtracted/run79516_histograms.root",38)'
```

This validates the presence of **38 distinct group directories**, the
30 histograms' ROOT class/binning, jet mode labels, plotting cuts, and
metadata before summing their bin contents. It saves a small combined ROOT
file containing only the 30 histograms plus annotation and cut metadata.
Unlike raw `hadd`, it does **not accidentally sum per-group cut settings**.

Check the result:

```bash
root -l -b -q 'plots_run79516_unsubtracted/run79516_histograms.root'
```

For example, inside ROOT:

```cpp
_file0->ls();
auto* h = _file0->Get<TH1D>("hJetsPerEvent");
if (h) std::cout << "Events in multiplicity histogram: "
                 << h->GetEntries() << std::endl;
```

The stored `jetcharge_events_read` metadata sums processed events from
all groups. Make sure it agrees with the expected processed-event counts
(roughly 37.8 million if all jobs produced 1000 events).

### 4. Draw the final plots (no TTree rereading)

Still in `analysis/`:

```bash
root -l -b -q 'draw_jetcharge_histograms.C("plots_run79516_unsubtracted/run79516_histograms.root","plots_run79516_unsubtracted/figures")'
```

The five original figures include the standard `AnalysisUtils::makeSphenixTag()`
annotation (`sPHENIX Internal`, `p+p`, `sqrt(s) = 200 GeV`). The label is
applied only by the drawer; changing it does not alter any histogram contents.

The five output plot names (each PNG and PDF) are:

```text
jet_qa
jet_charge_qa
jet_charge_pt_slices
jet_pt_spectrum
jet_mass
```

Change drawing styles, overlays, annotations or log axes in
`draw_jetcharge_histograms.C` and rerun **only step 4**. No original
ROOT TTree is opened during this step.

## Quick alternatives

### Make + draw from a single file (unchanged old API)

From `analysis/`:

```bash
root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_00000_00000.root","single_jetcharge_plots",5.0,0.7)'
```

This writes `analysis/single_jetcharge_plots/jetcharge_histograms.root`
and 15 PNG/PDF figure pairs in the same directory.

### Make + draw from all files in one pass (no groups)

From `analysis/`:

```bash
root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_*.root","plots_run79516_all",5.0,0.7)'
```

This `TChain` approach creates only
`analysis/plots_run79516_all/jetcharge_histograms.root` and 12 PNG/PDF
figure pairs. It reads all 37,824 input ROOT files each time you run it,
so the grouped option is usually more manageable.

### Histogram maker only: wildcards or a manifest

From `analysis/`:

```bash
root -l -b -q 'make_jetcharge_histograms.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_*.root","plots_run79516_all/jetcharge_histograms.root",5.0,0.7)'
```

Or read exactly the files listed in one `.txt` manifest (one absolute ROOT
file path per line; blank or `#` comment lines are ignored):

```bash
root -l -b -q 'make_jetcharge_histograms.C("plots_run79516_unsubtracted/manifests/group_00.txt","plots_run79516_unsubtracted/groups/group_00/jetcharge_histograms.root",5.0,0.7)'
```

Arguments:
- `make_jetcharge_histograms(inputRootOrGlobOrManifest, outputHistRoot, minChargeJetPt, maxAbsEta)`
- `draw_jetcharge_histograms(inputHistRoot, outputFigureDir)`
- `merge_jetcharge_histograms(groupsDirectory, mergedHistRoot, expectedNumberOfGroups)`
- `plot_jetcharge_tree(inputRootOrGlobOrManifest, outputDir, minChargeJetPt, maxAbsEta)`

Output filenames from the histogram maker **must end in `.root`**; the
optional manifest filename must end in `.txt`. Quote ROOT expressions
with single quotes in the shell, preventing shell expansion of `*`.

## Warnings, reproducibility and storage

- **Do not run `hadd` on the 37,824 original TTrees** merely to make these
  QA figures; the histogram maker reads the original files in place.
- **Do not mix** jet modes, reconstruction configurations, run numbers,
  histogram binning or plotting cuts when combining groups. The group
  merge checks cuts and path-derived labels but cannot independently
  prove which underlying reconstruction configuration produced a file.
- **Do not reuse the same output path** with different cuts or inputs
  without explicitly choosing to replace earlier results.
- Before making plots, ensure production has completed, all expected
  output files exist, and ROOT files are readable. Exact manifest inputs
  prevent silently omitting missing segments.
- Histogram contents from the `TChain` workflow should match the old
  single-file/`hadd` workflow with the same event set and cuts. Tiny
  floating-point differences may arise from combining many partial
  histograms rather than filling a single histogram sequentially.
- A later change to physics selections or histogram binning requires
  rerunning the **maker**, then merge and draw. Pure drawing changes
  require rerunning only the **drawer**.
- The new group workflow is plotting-only. It does not change the
  reconstruction macro, Condor submission script, ROOT output files,
  event matching, or jet-charge calculation.

## Jet-constituent calorimeter geometry QA (feature branch)

The new geometry-enabled `JetChargeInputTree` is required. Old production
ROOT files without `jet_component_eta` and `jet_component_phi` cannot
produce these histograms. Regenerate the histograms from the new TTree files;
old small histogram ROOT files also lack the new plots.

The histogram maker produces **30 histograms** in total: 13 original jet-charge
and jet-kinematics histograms, one selected-jet `hJetEtaPhi`, and for each of
retowered EMCal, IHCal, OHCal four constituent histograms:
`hTowerEta<Layer>`, `hTowerPhi<Layer>`, `hTowerEtaPhi<Layer>`, and
`hTowerEtaPhiEnergy<Layer>`. Layer suffixes are `Cemc`, `Ihcal`, `Ohcal`.
The energy-weighted maps sum constituent energies in GeV (signed for Sub1),
not counts. Source IDs are 28/26/27 for unsubtracted and 29/30/31 for Sub1.

**Selection:** only constituents of jets satisfying both
`jet_pt >= minChargeJetPt` and `abs(jet_eta) < maxAbsEta` are included.
The tower coordinates themselves are **not** restricted to `abs(eta)<maxAbsEta`.
These are jet-constituent maps, not all-detector tower maps. All previous jet
histograms retain their selections. The maker checks flattened constituent
array lengths and offsets and fails on malformed entries.

The drawer additionally saves the following PNG/PDF pairs:

```text
jet_eta_phi
tower_geometry_1d_Cemc    tower_eta_phi_Cemc
tower_geometry_1d_Ihcal   tower_eta_phi_Ihcal
tower_geometry_1d_Ohcal   tower_eta_phi_Ohcal
```

Each `tower_geometry_1d_*` figure has eta and phi panels; each
`tower_eta_phi_*` figure shows occupancy and energy-weighted 2D maps.
The existing five figures remain unchanged.

### Quick test before group processing

From the repository root after checking out the feature branch:

```bash
cd analysis
root -l -b -q 'plot_jetcharge_tree.C("../root/YOUR_NEW_PRODUCTION_DIR/jet_charge_inputs_unsubtracted_00079516_00000_00000.root","plots_tower_qa_test",5.0,0.7)'
```

Replace `YOUR_NEW_PRODUCTION_DIR` with the path containing the newly
produced, geometry-enabled TTree. After the test, use the same group script
with explicit `--input-dir` and a fresh `--output-dir` to avoid mixing old
13-histogram and new 26-histogram files. Re-run group histogram creation,
then merge, then draw; the drawer alone cannot reconstruct missing plots.

## Extended track and jet-charge QA (feature branch)

All output ROOT files must be produced with the selected-track and matched-track
branches from `JetChargeTreeMaker`: `track_pt`, `track_charge`, `n_tracks`,
`jet_track_offset`, and `jet_track_delta_r`. These already exist in the current
production TTree; no new Fun4All production is required to make the additional
plots.

The histogram maker writes **30 histograms**. In particular:

- `hTrackPt`: all saved tracks' pT, with log-spaced bins from 0.5 to 10,000 GeV/c (track quality/production cuts apply). The maker prints the maximum observed pT and overflow count for unusually large tracks.
- `hTrackCharge`: charges of all saved tracks (not restricted to those matched to jets).
- `hMatchDeltaR`: track–jet matching ΔR of tracks assigned to jets passing the histogram-making jet pT and η cuts. The matching threshold in production is ΔR < 0.4.
- `hNMatched`: number of associated tracks for each selected jet, including zero.
- `hChargeVsPt`: jet charge versus jet pT for selected jets with at least one matched track.
- Jet-charge distributions `hCharge5to10`, `hCharge10to15`, and `hCharge15plus` (GeV/c), for jets with at least one matched track. `hCharge2to5` remains in the histogram ROOT file but is **not plotted**. The old `hCharge10plus` is replaced by the two narrower bins.

The drawer now writes PNG and PDF files into **two subdirectories** below the
requested figure directory:

```text
figures/
  QA/
    jet_qa.png/.pdf
    track_qa.png/.pdf
    jet_pt_spectrum.png/.pdf
    jet_mass.png/.pdf
    jet_eta_phi.png/.pdf
    tower_geometry_1d_Cemc.png/.pdf
    tower_eta_phi_Cemc.png/.pdf
    tower_geometry_1d_Ihcal.png/.pdf
    tower_eta_phi_Ihcal.png/.pdf
    tower_geometry_1d_Ohcal.png/.pdf
    tower_eta_phi_Ohcal.png/.pdf
  analysis/
    jet_charge_overview.png/.pdf
    jet_charge_pt_slices.png/.pdf
    associated_tracks_per_jet.png/.pdf
    jet_charge_vs_pt.png/.pdf
```

Both the standalone plot wrapper and the group/merge/draw workflows still accept
their existing arguments. Example for the newer Lustre production:

```bash
bash analysis/make_jetcharge_groups.sh --run 79516 --group 00 \\
  --input-dir /sphenix/tg/tg01/jets/jlnliu/JetChargeTree/trackcalomatch_5GeV_unsub_wtower_run79516 \\
  --output-dir analysis/plots_run79516_wtower_QA --min-jet-pt 5 --max-abs-eta 0.7
```

Recreate group histogram files (prefer a **fresh** output directory) before
merging and drawing: earlier 26-histogram outputs lack the new track QA and
10–15/15+ charge slices, and cannot be updated by rerunning the drawer alone.
