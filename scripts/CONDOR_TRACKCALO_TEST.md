# Run 79516: jet-charge HTCondor test

This guide documents `scripts/myCondor_TrackCaloMatch_test.job` on
the current `main` branch. It is separate from the KFParticle campaign.
The default test enables `JetChargeTreeMaker` and disables
`TrackToCalo`.

## Requirements

- Submit from an sPHENIX node with HTCondor and shared project paths.
- The wrapper sources `/sphenix/u/$USER/setup.sh`; it must set up
  sPHENIX and your local `MYINSTALL`.
- `$MYINSTALL/lib/libtrack_to_calo.so` must contain the installed
  `JetChargeTreeMaker`; rebuilding is needed **only** if module C++ code
  changed or the installation is outdated.
- `fileLists/file_run79516_00_100Test.list` must exist **locally**.
  The `fileLists/` directory is Git-ignored.
- Create `log/` and ensure `/tmp/jlnliu/` is writable at the submit
  node, because the submit file uses those logging locations.

## Current submission settings

The tracked `scripts/myCondor_TrackCaloMatch_test.job` contains:

```text
JetMode    = unsubtracted
minTowerPt = 0.5
arguments  = $(events) $(file) $(OutDir) $(skip) true $(JetMode) false $(minTowerPt)
```

The eight script arguments are:

| Number | Value / macro meaning |
| --- | --- |
| 1 | `events`: number of events |
| 2 | `file`: input tracking DST basename |
| 3 | `OutDir`: ROOT output directory |
| 4 | `skip`: initial events skipped |
| 5 | `true`: write `JetChargeInputTree` |
| 6 | `JetMode`: `unsubtracted` or `sub1` |
| 7 | `false`: disable `TrackToCalo` |
| 8 | `minTowerPt`: minimum tower constituent pT in GeV/c |

The Run25 macro currently saves jets with pT >= 5.0 GeV/c and
tracks with pT >= 0.5 GeV/c, quality <= 300, at least 22 TPC clusters,
and crossing zero. JetChargeTreeMaker is independent of TrackToCalo.
Changing `JetMode` to `sub1` uses the Sub1 workflow; when
`minTowerPt>0`, `AntiKt_Tower_r04_Sub1_PtCut` is the saved node.

The test output directory configured in the tracked job is:

```text
root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet
```

Directory names describe the intended settings only. Keep them consistent
with the actual job arguments; they are not automatic reconstruction settings.

## Prepare and submit

From the repository root:

```bash
git switch main
git pull --ff-only
mkdir -p log root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet /tmp/jlnliu
test -f fileLists/file_run79516_00_100Test.list
wc -l fileLists/file_run79516_00_100Test.list
test -f "/sphenix/u/$(id -un)/setup.sh"
test -f "/sphenix/u/$(id -un)/install/lib/libtrack_to_calo.so"
bash -n scripts/runTrackCaloMatch.sh
condor_submit -dry-run /tmp/trackcalomatch_submit.ad scripts/myCondor_TrackCaloMatch_test.job
condor_submit scripts/myCondor_TrackCaloMatch_test.job
condor_q -nobatch
```

The queue file uses **three space-separated columns**:

```text
1000 DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root 0
1000 DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00001.root 0
```

Each non-comment row produces one Condor job. The number of queued jobs
depends on the **local file contents**, not on the submit-file comments.

For a one-row smoke test without editing the tracked job file:

```bash
awk 'NR==1 {$1=10; print}' fileLists/file_run79516_00_100Test.list > fileLists/file_run79516_00_smoke.list
sed -e 's/file_run79516_00_100Test.list/file_run79516_00_smoke.list/' \
    -e 's@trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet@trackcalomatch_smoke@' \
  scripts/myCondor_TrackCaloMatch_test.job > scripts/myCondor_TrackCaloMatch_smoke.job
mkdir -p root/trackcalomatch_smoke
condor_submit scripts/myCondor_TrackCaloMatch_smoke.job
```

## Inspect jobs and output

```bash
condor_q -nobatch
condor_history "$USER" -limit 10
ls -ltr log/trackcalomatch_test-*.out log/trackcalomatch_test-*.err
ls -lh root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet/
```

Successful jobs should write, for each tracking segment / skip offset:

```text
jet_charge_inputs_unsubtracted_00079516_<segment>_<skip>.root
```

With `TrackToCalo=false`, **no `track_calo_pairs_*.root` is expected**.

Example: for tracking segment `00000` and skip `00000`:

```text
root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet/jet_charge_inputs_unsubtracted_00079516_00000_00000.root
```

The macro maps tracking to calorimeter segments using
`calo_segment = tracking_segment / 100`. Validate the input streams'
event correspondence for the chosen production; changing segment numbering
does not replace an event-synchronization check.

The wrapper stages the macro under `_CONDOR_SCRATCH_DIR` and writes the
ROOT output into the shared `OutDir`. Failed DST basename resolution,
missing environment setup, or library loading problems appear in the
job stdout/stderr. The log filename uses the submit-file `Cluster` and
`Process` macros.

## Plot the results

From `analysis/`:

```bash
root -l -b -q 'plot_jetcharge_tree.C("../root/trackcalomatch_5GeV_0.5tower_unsub_00000CaloJet/jet_charge_inputs_unsubtracted_00079516_00000_00000.root","plots_unsub",5.0,0.7)'
```

The plotting macro uses the **input ROOT filename** first, otherwise its
directory, to label Sub1 or unsubtracted jets. It recognizes optional
directory tags such as `5GeV` and `0.5tower`, without changing any
selection. Details: `analysis/README_JET_CHARGE_INPUTS.md`.
