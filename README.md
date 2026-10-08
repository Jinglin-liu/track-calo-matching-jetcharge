# Track-Calo Matching Tutorial for sPHENIX

Hands-on examples for track projection, calorimeter matching, and a
KFParticle photon-conversion analysis in sPHENIX.

This repository is organized by file purpose rather than by separate
tutorials.  The same matching concepts are demonstrated in two ways:

1. `TrackToCalo` exposes the track-cluster loop directly and writes the
   accepted track-cluster pairs to a ROOT tree.
2. `KFParticle_sPHENIX` uses track-EMCal matching while reconstructing
   `gamma -> e+ e-` candidates.

The existing HTCondor submission workflow is retained so that the example
remains close to the analysis setup from which it was prepared.

## Repository layout

```text
track-calo-matching-tutorial/
├── module/                       # user-written TrackToCalo package
│   ├── TrackToCalo.h/.cc
│   └── autogen.sh                # configure the module build
├── macro/                        # ROOT/Fun4All macros
│   ├── Fun4All_TrackCaloMatch.C  # direct matching example
│   ├── Fun4All_KFP_Conv.C        # KFParticle conversion workflow
│   └── KFP_selections.C          # KFParticle configuration and cuts
├── analysis/                     # ROOT macros for both tutorial workflows
│   ├── AnalysisUtils.h           # shared plotting and path helpers
│   ├── plot_track_calo.C         # direct TrackCaloTree example
│   ├── plot_conversion.C         # KFParticle conversion diagnostics
│   ├── skimer_eop.C              # compact charge-ordered E/p tree
│   └── plot_eop.C                # E/p comparisons and eta-phi maps
└── scripts/                      # run, input-list, merge, and Condor scripts
    ├── runTrackCaloMatch.sh
    ├── runConvReco.sh
    ├── getFiles.sh
    ├── getFiles_rawhit.sh
    ├── merge_ee_reco.sh / merge_ee_reco_small.sh
    ├── run.sh                    # run the per-run E/p skimmer
    ├── submitCondorJobs.sh
    └── myCondor.job
```

Runtime directories such as `fileLists/`, `root/`, `log/`, and
`analysis/figure/` are created locally and ignored by Git.

## Prerequisites

The examples are intended to run on an sPHENIX analysis node.  You need:

- a working sPHENIX software environment;
- access to the required sPHENIX DSTs and calibration database;
- a writable local installation area, conventionally `${MYINSTALL}`;
- mutually compatible tracking and calorimeter inputs.

The production tags, segment mapping, CDB configuration, projection radius,
and matching cuts in this repository are starting values for the tutorial.
They must be checked when moving to another production.

## Part 1: direct track-EMCal matching

The minimal matching workflow is:

```text
SvtxTrackMap
    -> track quality and cluster-count cuts
    -> CEMC-projected SvtxTrackState
    -> loop over EMCal clusters
    -> delta-phi and delta-z window
    -> TrackCaloTree in a ROOT file
```

Each accepted track-cluster pair becomes one tree entry.  If several clusters
pass for one track, all pairs are saved rather than silently selecting the
first one.  This keeps matching ambiguities visible for later studies; a
physics analysis can subsequently define a best-match rule if needed.

### Build the module

Use the conventional local-install workflow:

```bash
source /opt/sphenix/core/bin/sphenix_setup.sh -n new

export MYINSTALL=/path/to/your/install
source /opt/sphenix/core/bin/setup_local.sh "$MYINSTALL"

cd module
./autogen.sh --prefix="$MYINSTALL"
make -j4
make install
cd ..
```

The tutorial library builds only `TrackToCalo.cc`; the module directory does
not contain unrelated ntuple, truth-matching, or detector-only classes.

### Configure the matching

The final `TrackToCalo` registration block in
`macro/Fun4All_TrackCaloMatch.C` contains the settings students are expected to
inspect:

```cpp
matcher->setEMcalRadius(102.9);  // cm
matcher->setTrackPtLowCut(0.2);  // GeV/c
matcher->setEmcalELowCut(0.2);   // GeV
matcher->setdphicut(0.10);       // rad
matcher->setdzcut(10.0);         // cm
```

The projection and matching modules must use exactly the same CEMC radius.

### Run a short matching job

From the repository root:

```bash
./scripts/runTrackCaloMatch.sh \
  10 \
  fileLists/file_run79516_00.list \
  root \
  0
```

The arguments are the number of events, input ROOT file or list, output
directory, and number of events to skip.  The input handling and reconstruction
steps are the same as `Fun4All_KFP_Conv.C`.  The output is named
`track_calo_pairs_RUN_SEGMENT_SKIP.root` and contains `TrackCaloTree`.

The tree stores event identifiers, track ID, `track_momentum`, the projected
`state_position_emcal` and `state_momentum_emcal`, cluster ID/energy and
`cluster_position`, plus `delta_phi`, `delta_z`, and `e_over_p`.
All four position or momentum quantities are stored as `TVector3` branches.
They provide methods such as `X()`, `Y()`, `Z()`, `Perp()`, `Mag()`, `Eta()`,
and `Phi()` without duplicating derived quantities in separate branches.  For
example:

```bash
root -l root/track_calo_pairs_*.root
TrackCaloTree->Draw("delta_z:delta_phi", "", "colz")
```

The main code sections are:

- `TrackToCalo::Init`: creates the ROOT file and `TrackCaloTree`;
- `TrackToCalo::process_event`: performs the matching loop;
- `passesTrackSelection`: applies track requirements;
- the source-local `piRange` helper wraps the angular residual to `[-pi, pi]`;
- `TrackToCalo::End`: writes and closes the ROOT file.

### Inspect the direct matching output

The tutorial analysis macro draws the matching residuals, E/p, charge
dependence, and the number of accepted clusters per track:

```bash
cd analysis
root -l -b -q 'plot_track_calo.C("../root/track_calo_pairs_*.root")'
```

Because every accepted pair is retained, repeated `(run, event_index,
track_id)` values identify matching ambiguity offline.  The example includes
the input-file index in that key so event indices that restart in separate
files are not mixed.

## Part 2: photon conversions with KFParticle

The second example reconstructs

```text
gamma -> e+ e-
```

and applies the track-calo matching implemented in KFParticle:

```text
tracking DST + calorimeter DST
    -> missing tracking reconstruction stages, if needed
    -> Global_Reco
    -> PHActsTrackProjection
    -> CEMC cluster reconstruction
    -> KFParticle gamma -> e+ e-
    -> track-EMCal matching inside KFParticle
    -> KFParticle ROOT output
    -> analysis/*.C
```

### Inspect the KFParticle configuration

The main configuration is in `macro/KFP_selections.C`:

```cpp
bool run_ee_reco = true;
bool get_calo_info = true;
float cemc_proj_radius = 102.9;
std::string ee_decay_descriptor = "[gamma -> e^+ e^-]cc";
```

`reconstruct_ee_mass()` contains the track requirements, track-calo matching
windows, and conversion-candidate selections.  These cuts should be treated as
analysis starting points rather than universal recommendations.

### Create input lists

For a high-level tracking DST, edit the configuration block in
`scripts/getFiles.sh`
and run, for example:

```bash
./scripts/getFiles.sh 79516
```

This creates lists such as:

```text
fileLists/file_run79516_00.list
```

Use `scripts/getFiles_rawhit.sh` only if the exercise is intended to start from
raw-hit DSTs.  That route is considerably slower.

### Run a short KFParticle job

Always validate a few events before submitting a campaign:

```bash
./scripts/runConvReco.sh 10 fileLists/file_run79516_00.list root 0
```

The arguments are:

```text
runConvReco.sh NEVENTS INPUT_FILE_OR_LIST OUTPUT_DIRECTORY NSKIP
```

The wrapper supports both interactive use and the original Condor-scratch
workflow.  It retrieves inputs with `getinputfiles.pl` and calls
`Fun4All_KFP_Conv.C`.

### Submit HTCondor jobs

The established submission procedure is preserved:

1. Set the desired runs in `scripts/submitCondorJobs.sh`.
2. Check that the corresponding files exist under `fileLists/`.
3. From the repository root, submit with:

```bash
source scripts/submitCondorJobs.sh
```

The script creates `log/` and `root/`.  KFParticle output is written below
`root/ee_reco/`.

### Merge and inspect the output

Merge a large campaign in batches with:

```bash
./scripts/merge_ee_reco.sh
```

Then make the basic diagnostic plots:

```bash
cd analysis
root -l -b -q 'plot_conversion.C("../root/ee_reco_merged.root")'
```

The first argument of `plot_conversion.C` is the input ROOT file; the default
is `../root/ee_reco_merged.root`.  The optional second argument selects the
output directory.  Output file names do not carry a run-number suffix.  Its
E/p figure charge-orders the two
tracks, overlays e+ in red and e- in blue, performs separate Gaussian fits,
and reports both fitted means and widths on the canvas.
Every output figure uses the common sPHENIX tag defined in
`analysis/AnalysisUtils.h`.  The `p+p` label is enabled by default; commented
`Au+Au` and `O+O` alternatives are kept next to it for a one-line switch.

For the E/p examples, first create a compact charge-ordered tree.  The skimmer
reads the input file name directly; both arguments have the defaults shown
below:

```bash
root -l -b -q \
  'skimer_eop.C("../root/ee_reco_merged.root","../root/ee_reco_skim.root")'
root -l -b -q \
  'plot_eop.C("../root/ee_reco_skim.root")'
```

The original per-run operation is also retained:

```bash
./scripts/merge_ee_reco_small.sh
./scripts/run.sh
```

In this mode `scripts/run.sh` passes `analysis/all_ee_reco_79516.root` and
`root/ee_reco_skim_79516.root` explicitly to the skimmer.  `plot_eop.C`
directly produces the inclusive tracker-calo comparisons, signed E/p eta-phi
maps, and curvature-residual eta-phi maps.

## Matching conventions

The direct tutorial defines

```text
delta_phi = wrap(phi_track_state - phi_cluster)
delta_z   = z_track_state - z_cluster
```

The track state is evaluated at the configured CEMC radius.  The cluster `z`
position is rescaled to that radius before `delta_z` is calculated.

Always inspect the residual distributions versus charge, transverse momentum,
eta, phi, and run before choosing final matching windows.  Asymmetric
`delta_phi` windows may be appropriate when charge bending and the chosen
residual definition are taken into account.

## Input synchronization

Tracking and calorimeter productions may contain different numbers of events
per segment.  `Fun4All_KFP_Conv.C` and `Fun4All_TrackCaloMatch.C` map tracking
segments to calorimeter segments
using `ratio_nCalo_over_nTrkr`.  Verify this value and the event alignment for
the selected production before submitting many jobs.

## Common problems

### A required node is missing

- Missing `SvtxTrackMap`: use a fitted-track DST or run track fitting first.
- Missing `SvtxVertexMap`: verify that the tracking input contains vertices.
- Missing `CLUSTERINFO_CEMC`: check the calo input, calibration, geometry, and
  cluster builder.
- Missing tracker clusters or dE/dx: load a compatible cluster DST or disable
  the corresponding optional KFParticle information.

### The CEMC track state is missing

Not every track reaches the calorimeter.  Also confirm that
`PHActsTrackProjection` is registered before the matching code and that both
modules use the same radius.

### The KFParticle output is empty

Check, in order, the input track map, projected states, track-calo residuals,
track requirements, and candidate mass window.  Start with a small job and
loose diagnostic selections.

## Status

This is a teaching repository rather than a centrally maintained production
package.  Validate the examples against the current sPHENIX release and data
production before using them in a physics result.
