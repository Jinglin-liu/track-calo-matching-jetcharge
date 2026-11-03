#!/usr/bin/env bash
# Make 13 QA histograms per numbered Condor file-list group without merging TTrees.
# The original fileLists/*_NN.list rows define the EXACT expected output ROOT
# filenames, including tracking segment and skip. Missing outputs fail loudly.
#
# From anywhere:
#   bash analysis/make_jetcharge_groups.sh --run 79516
#   bash analysis/make_jetcharge_groups.sh --run 79516 --group 00
#   bash analysis/make_jetcharge_groups.sh --run 79516 --force
set -euo pipefail

script_path=$(readlink -f "${BASH_SOURCE[0]}")
analysis_dir=$(dirname "$script_path")
repo_dir=$(dirname "$analysis_dir")
run=79516
mode=unsubtracted
min_pt=5.0
max_eta=0.7
input_dir=""
output_dir=""
groups=()
force=false
skip_existing=false

usage() {
  cat <<'EOF'
Usage: bash analysis/make_jetcharge_groups.sh [options]
  --run N           Run number (default: 79516)
  --mode MODE       unsubtracted|sub1 (default: unsubtracted)
  --min-jet-pt PT   Jet charge/QA pT threshold (default: 5.0)
  --max-abs-eta ETA Jet charge/QA eta limit (default: 0.7)
  --group NN        Process only selected numbered group(s); repeatable
  --input-dir DIR   Override the original ROOT-file directory
  --output-dir DIR  Override output directory for manifests and group histograms
  --force           Replace previously created per-group histogram files
  --skip-existing   Resume: skip nonempty per-group histogram files (verify cuts separately)
  -h, --help        Show this usage

Defaults:
  input:  repo/root/trackcalomatch_5GeV_unsub_run<run>/
  output: repo/analysis/plots_run<run>_<mode>/
  input list: repo/fileLists/file_run<run>_<NN>.list
No new merged TTrees are generated.
EOF
}

while (($#)); do
  case "$1" in
    --run) (($# >= 2)) || exit 2; run=$2; shift 2 ;;
    --mode) (($# >= 2)) || exit 2; mode=$2; shift 2 ;;
    --min-jet-pt) (($# >= 2)) || exit 2; min_pt=$2; shift 2 ;;
    --max-abs-eta) (($# >= 2)) || exit 2; max_eta=$2; shift 2 ;;
    --group) (($# >= 2)) || exit 2; groups+=("$2"); shift 2 ;;
    --input-dir) (($# >= 2)) || exit 2; input_dir=$2; shift 2 ;;
    --output-dir) (($# >= 2)) || exit 2; output_dir=$2; shift 2 ;;
    --force) force=true; shift ;;
    --skip-existing) skip_existing=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "ERROR: unknown argument $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ "$force" == true && "$skip_existing" == true ]]; then
  echo "ERROR: --force and --skip-existing are mutually exclusive" >&2
  exit 2
fi
[[ "$run" =~ ^[0-9]{5,8}$ ]] || { echo "ERROR: invalid run number" >&2; exit 2; }
[[ "$mode" == unsubtracted || "$mode" == sub1 ]] || {
  echo "ERROR: mode must be unsubtracted or sub1" >&2; exit 2;
}
for group in "${groups[@]}"; do
  [[ "$group" =~ ^[0-9][0-9]$ ]] || { echo "ERROR: expected two-digit group: $group" >&2; exit 2; }
done
[[ "$min_pt" =~ ^[0-9]+([.][0-9]+)?$ ]] || { echo "ERROR: invalid min pt" >&2; exit 2; }
[[ "$max_eta" =~ ^[0-9]+([.][0-9]+)?$ ]] || { echo "ERROR: invalid max eta" >&2; exit 2; }
printf -v run8 '%08d' "$((10#$run))"

if [[ -z "$input_dir" ]]; then
  input_dir="$repo_dir/root/trackcalomatch_5GeV_unsub_run$run"
fi
if [[ -z "$output_dir" ]]; then
  output_dir="$analysis_dir/plots_run${run}_${mode}"
fi
[[ -d "$input_dir" ]] || { echo "ERROR: input directory does not exist: $input_dir" >&2; exit 1; }
command -v root >/dev/null || { echo "ERROR: ROOT executable not in PATH" >&2; exit 1; }
input_dir=$(readlink -f "$input_dir")
mkdir -p "$output_dir/manifests" "$output_dir/groups"
output_dir=$(readlink -f "$output_dir")

shopt -s nullglob
lists=("$repo_dir"/fileLists/file_run"$run"_[0-9][0-9].list)
(("${#lists[@]}" > 0)) || { echo "ERROR: no numbered input lists" >&2; exit 1; }
selected=()
for list in "${lists[@]}"; do
  group=${list##*/}
  group=${group#file_run${run}_}
  group=${group%.list}
  if (("${#groups[@]}" > 0)); then
    include=false
    for wanted in "${groups[@]}"; do
      if [[ "$group" == "$wanted" ]]; then include=true; break; fi
    done
    [[ "$include" == true ]] || continue
  fi
  selected+=("$list")
done
(("${#selected[@]}" > 0)) || { echo "ERROR: no selected groups" >&2; exit 1; }

echo "ROOT input directory: $input_dir"
echo "Group histogram directory: $output_dir/groups"
echo "Run=$run Mode=$mode minJetPt=$min_pt maxAbsEta=$max_eta"
echo "Selected groups: ${#selected[@]}"

for list in "${selected[@]}"; do
  group=${list##*/}
  group=${group#file_run${run}_}
  group=${group%.list}
  manifest="$output_dir/manifests/group_${group}.txt"
  group_dir="$output_dir/groups/group_${group}"
  hist="$group_dir/jetcharge_histograms.root"
  if [[ -e "$hist" && "$force" == false ]]; then
    if [[ "$skip_existing" == true && -s "$hist" ]]; then
      echo "Skipping existing group $group: $hist"
      continue
    fi
    echo "ERROR: $hist exists. Use --force to replace, or --skip-existing to resume." >&2
    exit 1
  fi
  mkdir -p "$group_dir"
  tmp_manifest=$(mktemp "$output_dir/manifests/.group_${group}.XXXXXXXX")
  count=0
  while read -r nevents dst skip extra; do
    [[ -n "${nevents:-}" && "${nevents:0:1}" != "#" ]] || continue
    [[ -z "${extra:-}" && "$nevents" =~ ^[0-9]+$ &&
       "$dst" == DST_TRKR_*.root && "$skip" =~ ^[0-9]+$ ]] || {
      echo "ERROR: invalid row in $list: $nevents $dst $skip" >&2
      rm -f "$tmp_manifest"; exit 1;
    }
    segment=${dst%.root}
    segment=${segment##*-}
    [[ "$segment" =~ ^[0-9]{5}$ ]] || {
      echo "ERROR: invalid tracking segment $dst" >&2
      rm -f "$tmp_manifest"; exit 1;
    }
    printf -v skip5 '%05d' "$((10#$skip))"
    path="$input_dir/jet_charge_inputs_${mode}_${run8}_${segment}_${skip5}.root"
    [[ -f "$path" ]] || {
      echo "ERROR: missing expected ROOT file: $path" >&2
      rm -f "$tmp_manifest"; exit 1;
    }
    printf '%s\n' "$path" >> "$tmp_manifest"
    ((count += 1))
  done < "$list"
  if ((count == 0)); then
    echo "ERROR: empty group $list" >&2
    rm -f "$tmp_manifest"; exit 1
  fi
  if [[ $(sort -u "$tmp_manifest" | wc -l) -ne "$count" ]]; then
    echo "ERROR: duplicate segment/skip outputs in $list" >&2
    rm -f "$tmp_manifest"; exit 1
  fi
  mv -f "$tmp_manifest" "$manifest"
  echo "Processing group $group ($count ROOT files) ..."
  rm -f "$hist"
  (
    cd "$analysis_dir"
    root -l -b -q "make_jetcharge_histograms.C(\"$manifest\",\"$hist\",$min_pt,$max_eta)"
  )
  [[ -s "$hist" ]] || {
    echo "ERROR: ROOT histogram output missing for group $group" >&2
    exit 1
  }
done

echo "Finished ${#selected[@]} group(s). Histograms are under: $output_dir/groups"
echo "To merge all groups, run merge_jetcharge_histograms.C; then draw_jetcharge_histograms.C."
