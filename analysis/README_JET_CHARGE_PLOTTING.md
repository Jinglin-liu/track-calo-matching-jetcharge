# Jet-charge QA plotting for Run 79516: no large TTree hadd needed

This workflow reads the production `JetChargeInputTree` files with ROOT `TChain`
and stores **13 small histograms**, not a copy of the original event TTrees.
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
        group_00/jetcharge_histograms.root   # 13 histograms + metadata
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
jet_n_matched_tracks, n_jets and jet_charge_kappa) are accessed. The large
`jet_component_*` arrays are not read.

For a one-group test, *before processing the full set*:

```bash
bash analysis/make_jetcharge_groups.sh --run 79516 --group 00
```

The output is `analysis/plots_run79516_unsubtracted/groups/group_00/jetcharge_histograms.root`.
To rerun that group with the same output file, supply `--force`:

```bash
bash analysis/make_jetcharge_groups.sh --run 79516 --group 00 --force
```

Once you have processed group 00, run the main command **with `--force`**
to regenerate all 38 groups, or call `--group 01` ... `--group 37`
individually. The script deliberately refuses to overwrite an existing
group histogram without `--force`, preventing accidental loss of results.

### 3. Merge the 38 histogram files (not TTrees)

From `analysis/`:

```bash
cd analysis
root -l -b -q 'merge_jetcharge_histograms.C("plots_run79516_unsubtracted/groups","plots_run79516_unsubtracted/run79516_histograms.root",38)'
```

This validates the presence of **38 distinct group directories**, the
13 histograms' ROOT class/binning, jet mode labels, plotting cuts, and
metadata before summing their bin contents. It saves a small combined ROOT
file containing only the 13 histograms plus annotation and cut metadata.
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
and five PNG/PDF figure pairs in the same directory.

### Make + draw from all files in one pass (no groups)

From `analysis/`:

```bash
root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_unsub_run79516/jet_charge_inputs_unsubtracted_00079516_*.root","plots_run79516_all",5.0,0.7)'
```

This `TChain` approach creates only
`analysis/plots_run79516_all/jetcharge_histograms.root` and five PNG/PDF
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
