# Run25 event-level track and calorimeter jet-charge inputs

This document describes the current `main` implementation of
`macro/Fun4All_TrackCaloMatch.C` and `module/JetChargeTreeMaker.{h,cc}`.
It is an analysis development workflow, not a validated physics measurement.

## What gets written

When `writeJetChargeTree=true`, `JetChargeTreeMaker` writes a ROOT TTree
named **`JetChargeInputTree`**, with **one entry per processed Fun4All event**
including events that contain zero accepted tracks or jets.

- `run`, `event`: EventHeader identifiers (or -1 when absent).
- `event_index`: entry index within this job, starting at zero.
- `vertex_z`, `jet_charge_kappa`, `track_jet_match_r`.
- `n_tracks` and `track_*` vectors: selected reconstructed tracks.
- `n_jets` and `jet_*` vectors: selected calorimeter jets and derived jet charge.
- `jet_component_offset` and `jet_component_{source,id,energy,ieta,iphi}`:
  flattened tower constituents; jet `j` owns entries
  `[jet_component_offset[j], jet_component_offset[j+1])`.
- `jet_track_offset`, `jet_track_index`, `jet_track_delta_r`: flattened
  matched-track indices and distances; indices refer to `track_*` vectors
  in the **same event**.

The optional `TrackToCalo` module writes a **separate** `TrackCaloTree`
containing track–EMCal-cluster matched pairs. It does **not** determine
which tracks enter jet charge, and is currently disabled in jet-charge
Condor examples.

### Current selections in Fun4All_TrackCaloMatch.C

| Quantity | Macro configuration |
| --- | --- |
| Calorimeter jet algorithm | anti-kT, R = 0.4 |
| Minimum saved jet pT | 5.0 GeV/c |
| Minimum selected track pT | 0.5 GeV/c |
| Maximum track quality | 300 |
| Minimum TPC clusters | 22 |
| Track crossing | 0 required |
| Track–jet matching | closest saved jet axis, strict DeltaR < 0.4 |
| Jet-charge kappa | 1.0 (module default) |
| Additional tower constituent pT | configurable `minTowerPt`; **0.0 by macro default** |

All passing tracks are saved, whether or not they match a jet or EMCal
cluster. A track can match **at most one** saved jet; rejected jets are
not candidates for matching. For jet `j`:

`Q_kappa = [sum over matched tracks (q_track * track_pt^kappa)] / jet_pt^kappa`.

For positive-pT jets without matched tracks, jet charge is zero. The
`jet_charge_numerator`, `jet_n_matched_tracks`, and
`jet_matched_track_pt_sum` branches provide cross-checks.

## Calorimeter jet modes and tower cuts

Two modes can be selected independently of the `TrackToCalo` switch:

| `jetMode` | Constituent cut | Saved jet node |
| --- | --- | --- |
| `unsubtracted` | `minTowerPt = 0` | `AntiKt_unsubtracted_r04` |
| `unsubtracted` | `minTowerPt > 0` | `AntiKt_unsubtracted_r04` (filtered inputs) |
| `sub1` | `minTowerPt = 0` | `AntiKt_Tower_r04_Sub1` |
| `sub1` | `minTowerPt > 0` | `AntiKt_Tower_r04_Sub1_PtCut` |

- Unsubtracted reconstruction uses calibrated HCal towers and retowered CEMC
  with a local `PtCutTowerJetInput` wrapper.
- Sub1 reconstruction runs the official `HIJetReco()` background/seed/tower
  subtraction stages unchanged. When the additional cut is positive, it
  reconstructs a **separate final R=0.4** jet collection from filtered
  Sub1 towers, and the TTree reads that collection.
- The custom cut is on the **magnitude** of tower transverse momentum
  (`Jet::get_pt()`), including for negative-energy Sub1 tower inputs;
  this is different from a signed-energy cut.
- Standard unfiltered Sub1 jet nodes coexist in the Fun4All node tree,
  but the current `JetChargeTreeMaker` saves **only the selected node**,
  with the additional saved-jet pT threshold.
- No cluster-based jet inputs are currently used by the jet-charge module.

Changing constituent cuts can change jet energy, mass, axis, track
association, and jet charge. Compare against `minTowerPt=0` before
interpreting differences as physics.

### Input streams

The macro accepts a tracking ROOT DST or a standard file list, loads
tracking-cluster DSTs when needed, and loads `DST_CALOFITTING`. The
present Run25 pp configuration uses tracking-cluster tag
`ana538_2025p011_v001` and calorimeter tag `pro001_pcdb001_v001`.

The mapping `calo_segment = tracking_segment / 100` is hardcoded
for this data production. It is **not an event offset**. Verify input stream
synchronization independently when using a different run or production.
The TTree's `run/event` fields alone do not prove cross-stream alignment.

## Build and run

If `JetChargeTreeMaker.cc` or `.h` changed, rebuild the local module:

```bash
cd module
bash autogen.sh --prefix="$MYINSTALL"
make -j8 install
cd ..
```

For updates limited to the ROOT macro, plotting macro, submission script,
or Markdown, rebuilding `libtrack_to_calo.so` is unnecessary as long
as the installed library already includes the tested `JetChargeTreeMaker`.

From the repository root, the wrapper signature is:

```text
scripts/runTrackCaloMatch.sh NEVENTS INPUT_ROOT_OR_LIST OUTDIR NSKIP WRITE_JET_TREE JET_MODE ENABLE_TRACK_TO_CALO MIN_TOWER_PT
```

Example (10 events, unsubtracted jets, tower pT >= 0.5 GeV/c,
TrackToCalo disabled):

```bash
./scripts/runTrackCaloMatch.sh 10 \
  DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root \
  root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet \
  0 true unsubtracted false 0.5
```

The wrapper uses `$HOME/setup.sh`, supports interactive execution and
Condor scratch execution, and resolves DST basenames with `getinputfiles.pl`.
Use `scripts/myCondor_TrackCaloMatch_test.job` for the current test.
The corresponding queue file is `fileLists/file_run79516_00_100Test.list`,
which is **Git-ignored and must exist on the submit host**.

The macro parameter `minTowerPt` defaults to 0.0, but a Condor job
passes its configured eighth argument and overrides that default.

## Expected outputs and QA

With jet-tree output enabled:

```text
OUTDIR/jet_charge_inputs_<mode>_<run:8digits>_<segment:5digits>_<skip:5digits>.root
```

For example:

```text
root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet/jet_charge_inputs_unsubtracted_00079516_00000_00000.root
```

When `enableTrackToCalo=true`, the macro additionally writes
`track_calo_pairs_<run>_<segment>_<skip>.root`. When disabled, this
second file is **not** created.

Check the output using ROOT:

```cpp
TFile *f = TFile::Open("jet_charge_inputs_unsubtracted_00079516_00000_00000.root");
TTree *t = nullptr;
f->GetObject("JetChargeInputTree", t);
if (t) {
  t->Print();
  t->Scan("run:event:event_index:n_tracks:n_jets", "", "", 10);
}
```

Generate jet and jet-charge QA plots from the `analysis` directory:

```bash
root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet/jet_charge_inputs_unsubtracted_00079516_00000_00000.root","plots_unsub",5.0,0.7)'
```

The plotting macro infers **Sub1 versus unsubtracted** from the input ROOT
filename first, falling back to the directory name. It can also annotate
directory tags such as `5GeV` and `0.5tower`; these are **unverified
filename labels**, not authoritative cut metadata inside the tree. Plotting
cuts passed as the third and fourth arguments are independent of the
reconstruction selections.

## Production checks

- Test a small event sample after environment or reconstruction changes.
- Confirm input streams are synchronized and the selected jet node exists.
- Compare jet multiplicity, pT, mass, and charge with and without the
  additional tower selection; validate tower constituent energies.
- Confirm `JetChargeInputTree` entry count, vector lengths, offsets,
  and unique `(run,event)` identifiers across merged segments.
- Validate your Condor queue file, output directory, and input production
  before large-scale submission.
- If enabling `TrackToCalo` in the future: the macro currently registers
  a **second EMCal cluster builder** alongside `Process_Calo_Calib()`.
  Consolidate those builders before relying on TrackToCalo cluster results.
