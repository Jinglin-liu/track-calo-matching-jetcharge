# Event-level track and calorimeter jet tree (first jet-charge step)

This feature is experimental. Run a small test and validate DST synchronization
before processing physics data.

## Design

- `module/JetChargeTreeMaker.{h,cc}` writes `JetChargeInputTree`.
- **One TTree entry per Fun4All event** (even if no tracks or jets pass cuts).
- Each entry has job-local `event_index`, plus `run` and `event`
  from the event header (or -1 when unavailable).
- All track and jet vector elements within an entry refer to the same
  Fun4All processing iteration and are therefore geometrically comparable
  *only after the input streams have been verified synchronized*.
- The module **does not** use EMCal matching to select charged tracks.
- Axis-based geometric track/jet matching and pT-weighted jet charge are calculated per jet.
- The existing TrackToCaloTree remains unchanged.

### Inputs

The existing macro processes tracking seeds or fitted tracks from tracking DST,
and additionally loads a tracking-cluster DST and a `DST_CALOFITTING` DST.
With the optional jet-charge mode enabled, calibrated towers are obtained from
`Process_Calo_Calib()`, CEMC is retowered with `RetowerCEMC`, then
`JetReco` clusters `Jet::CEMC_TOWERINFO_RETOWER`,
`Jet::HCALIN_TOWERINFO` and `Jet::HCALOUT_TOWERINFO` using anti-kT R=0.4
without background subtraction.

Jet node: `AntiKt_unsubtracted_r04`.
Track node: `SvtxTrackMap`.

The output stores a jet-level kinematic vector per variable and flattened
calorimeter tower-constituent vectors with `jet_component_offset`: for jet j
the constituent indices run from `offset[j]` (inclusive) to
`offset[j+1]` (exclusive). The component `source`, `id` (channel),
`energy`, `ieta` and `iphi` are saved.

Track branches include ID, charge, crossing, fit/position/momentum quantities,
DCA information, and tracking-subdetector cluster multiplicities. Default cuts
match TrackToCalo's *track selection* (pT >= 0.2 GeV, quality <= 300,
TPC clusters >= 22, crossing == 0). All accepted tracks are saved,
including tracks without EMCal cluster matches. Track cuts can be adjusted
through `JetChargeTreeMaker` setters.

## Compile in your sPHENIX setup

```bash
cd module
bash autogen.sh --prefix="$MYINSTALL"
make -j8 install
```

Follow the existing project's build instructions if your environment requires
a different invocation of `autogen.sh`.

## Small test (single tracking ROOT file)

From the repository root, run:

```bash
./scripts/runTrackCaloMatch.sh 10 \
  /path/to/DST_TRKR_SEED_run3pp_ana538_2025p011_v001-00079516-00000.root \
  root 0 true
```

The fifth argument `true` enables the jet-charge tree; omitted or `false`
leaves previous output behavior unchanged.

The macro automatically derives a calorimeter DST path from the run and
tracking segment. For the first segment of Run 79516 it currently derives:

```text
/sphenix/lustre01/sphnxpro/production/run3pp/physics/pro001_pcdb001_v001/DST_CALOFITTING/run_00079500_00079600/DST_CALOFITTING_run3pp_pro001_pcdb001_v001-00079516-00000.root
```

It also opens a separate tracking-cluster DST with the currently hardcoded
tag `ana556_2025p013_v002`. Verify compatibility with any seed file tagged
`ana538_2025p011_v001`.

### Outputs

Two files are written in the specified output directory:

- `track_calo_pairs_<run>_<segment>_<skip>.root` (existing module)
- `jet_charge_inputs_<run>_<segment>_<skip>.root` (new module)

Inspect with:

```bash
root -l root/jet_charge_inputs_00079516_00000_00000.root
```

```cpp
JetChargeInputTree->Print();
JetChargeInputTree->Scan("run:event:event_index:n_tracks:n_jets");
```

## Critical preflight checks

1. Confirm every input manager is consuming the **same collision/event**.
   `run`, `event` and local `event_index` in a merged topNode are not
   an independent proof that each originating DST contained the same event.
   Check input DST event headers and/or the Fun4All sync mechanisms, especially
   when tracking and calorimeter DSTs have different segment boundaries.
2. Confirm the 100-to-1 tracking/calo segment conversion used by the existing
   macro is correct for this data production; it is **not** an event offset.
3. The original script and macro do not properly parse custom three-column
   files of the form `1000 filename.root 0`. Use a single ROOT file for
   the first test. File-list handling should be corrected separately before
   bulk Condor production.
4. Verify input calibration, vertex choice, tower quality, and jet energy
   selection. This first implementation is **unsubtracted** pp calorimeter
   jets, not a calibrated final physics-level jet-charge measurement.
5. Run a small event sample on an sPHENIX node and confirm compilation,
   expected input nodes, stable event counts, and sensible kinematics.
   The GitHub changes were not compiled against the site's installed release.

## Jet-charge matching and output

For each accepted jet, the module loops over all selected tracks and computes
`dR = hypot(track_eta - jet_eta, remainder(track_phi - jet_phi, 2*pi))`.
If `dR < 0.4`, that track contributes to the jet charge:

`Q_kappa = sum(q_track * pow(track_pt, kappa)) / pow(jet_pt, kappa)`.

Default `kappa=1.0` and matching radius `R=0.4`. To change these, call
`jetTree->setJetChargeKappa(0.5)` and/or
`jetTree->setTrackJetMatchR(0.4)` in the macro before registering it.
These settings are also saved as event-level branches `jet_charge_kappa`
and `track_jet_match_r`.

New jet-level vector branches:

- `jet_charge`: calculated charge, NaN for zero-pT jet
- `jet_charge_numerator`: signed weighted track pT sum
- `jet_n_matched_tracks`: number of accepted tracks inside cone
- `jet_matched_track_pt_sum`: scalar matched-track pT sum

Flattened matching arrays:

- `jet_track_offset`: length `n_jets + 1`; jet j matches in
  `[jet_track_offset[j], jet_track_offset[j+1])`
- `jet_track_index`: indices into event-level `track_*` arrays
- `jet_track_delta_r`: corresponding match distance

No match gives charge zero for positive-pT jets. Track associations are not
exclusive: a track within R of two jet axes contributes to both. Track
directions use reconstructed momentum eta/phi, **not** track extrapolations
at the calorimeter surface.

## Offline geometric association

For each event entry and each jet j, loop through its track arrays:

```cpp
float dphi = TVector2::Phi_mpi_pi(track_phi[i] - jet_phi[j]);
float deta = track_eta[i] - jet_eta[j];
float dr = std::hypot(deta, dphi);
if (dr < 0.4) {
  // candidate charged track in this calorimeter jet
}
```

Jet charge `Q_kappa = sum(q_i * pt_i^kappa)/jet_pt^kappa` can be
calculated after deciding quality cuts, eta acceptance, jet pT selections and
the track-to-jet association convention.
