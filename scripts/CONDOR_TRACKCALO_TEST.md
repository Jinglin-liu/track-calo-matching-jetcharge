# Run 79516: Track–Calo + jet-charge HTCondor test

This is a separate submission from the KFParticle campaign. It uses
`scripts/myCondor_TrackCaloMatch_test.job` and does not modify the existing
`scripts/submitCondorJobs.sh`.

## Prerequisites

- Work on an sPHENIX login/submit node with Condor available.
- `/sphenix/u/$USER/setup.sh` must initialize sPHENIX and `MYINSTALL`.
- `$MYINSTALL/lib/libtrack_to_calo.so` must be the working installed build
  containing both TrackToCalo and JetChargeTreeMaker.
- The `fileLists/file_run79516_00_test.list` file must exist **locally**.
  `fileLists/` is Git-ignored; it is not fetched by Git.
- Finish any ongoing Git rebase before switching branches.

## Prepare

From the repository root:

```bash
mkdir -p log root/trackcalomatch_test
test -f fileLists/file_run79516_00_test.list
wc -l fileLists/file_run79516_00_test.list
test -f /sphenix/u/"$(id -un)"/setup.sh
test -f /sphenix/u/"$(id -un)"/install/lib/libtrack_to_calo.so
```

The example file has ten rows:

```text
1000 DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root 0
...
1000 DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00009.root 0
```

The `queue events,file,skip from ...` statement creates one Condor job per row.
Each job runs:

```bash
scripts/runTrackCaloMatch.sh 1000 DST_TRKR_SEED_...root root/trackcalomatch_test 0 true
```

The final `true` enables the optional jet-charge output.

## Submit

Submit **from the repository root** so `$ENV(PWD)` resolves to its absolute
path and the relative queue-file path is found:

```bash
condor_submit scripts/myCondor_TrackCaloMatch_test.job
condor_q "$USER"
```

To test only one job first, make a one-row list and use a temporary copy of
the submit file:

```bash
head -n 1 fileLists/file_run79516_00_test.list > fileLists/file_run79516_00_smoke.list
sed 's/file_run79516_00_test.list/file_run79516_00_smoke.list/' \
  scripts/myCondor_TrackCaloMatch_test.job > scripts/myCondor_TrackCaloMatch_smoke.job
condor_submit scripts/myCondor_TrackCaloMatch_smoke.job
```

Both `*.list` files are ignored by Git; the temporary `*.job` file can be
deleted after testing.

## Inspect

```bash
condor_q "$USER"
ls -lh root/trackcalomatch_test/
ls -ltr log/trackcalomatch_test-*.out log/trackcalomatch_test-*.err
tail -n 80 log/trackcalomatch_test-CLUSTER-PROCESS.out
condor_history "$USER" -limit 10
```

Replace `CLUSTER-PROCESS` with the identifiers returned by `condor_submit`.

For each successfully processed segment, expect two output files in
`root/trackcalomatch_test/`:

- `track_calo_pairs_RUN_SEGMENT_00000.root`
- `jet_charge_inputs_RUN_SEGMENT_00000.root`

Output filenames contain the tracking segment, so the ten jobs should not
overwrite one another. Check event counts, nonzero track/jet content and
cross-node consistency before scaling up.

## Important caveats

- The macro currently derives the calorimeter DST from the tracking segment
  using `ratio_nCalo_over_nTrkr = 100`. Confirm that the tracking and calo
  event streams are aligned for Run 79516 before interpreting physics output.
- `getinputfiles.pl` resolves tracking DST basenames at job runtime; errors
  locating files will appear in the job logs.
- The script stages macros into `_CONDOR_SCRATCH_DIR` and writes output to the
  shared `root/trackcalomatch_test/` path. This submission assumes the
  sPHENIX shared filesystem is mounted on execute nodes.
- This file only prepares a submission. It does not run Condor jobs itself.
