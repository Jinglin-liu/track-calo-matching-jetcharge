#!/usr/bin/env bash
# Submit Run25 TrackCaloMatch jobs from numbered file_run<run>_<NN>.list groups.
# The *_test.list and *_100Test.list files are intentionally excluded.
#
# Preview (safe default):
#   bash scripts/submitCondorTrackCaloMatch.sh --run 79516
# Submit one group:
#   bash scripts/submitCondorTrackCaloMatch.sh --run 79516 --group 00 --submit
# Submit all numbered groups:
#   bash scripts/submitCondorTrackCaloMatch.sh --run 79516 --submit

set -euo pipefail

script_path=$(readlink -f "${BASH_SOURCE[0]}")
script_dir=$(dirname "$script_path")
repo_dir=$(dirname "$script_dir")
submit_file="$script_dir/myCondor_TrackCaloMatch.job"
list_dir="$repo_dir/fileLists"

submit=false
runs=()
groups=()

usage() {
  cat <<'EOF'
Usage: bash scripts/submitCondorTrackCaloMatch.sh [--run RUN] [--group NN] [--submit]

Options:
  --run RUN       Run number, repeatable (default: 79516).
  --group NN      Two-digit file-list group, repeatable (default: all).
  --submit        Actually submit jobs. Without this: preflight and preview only.
  -h, --help      Show this message.

Only file_run<run>_<NN>.list is accepted, e.g. file_run79516_00.list.
Files such as file_run79516_00_test.list and _100Test.list are excluded.
The .job template controls JetMode, minTowerPt, base OutDir and memory settings.
The driver appends _run<run> to OutDir in each temporary submit file.
Each line in a numbered list is: EVENTS DST_BASENAME SKIP.
EOF
}

while (($#)); do
  case "$1" in
    --run)
      (($# >= 2)) || { echo "Missing argument for --run" >&2; exit 2; }
      [[ "$2" =~ ^[0-9]{5,8}$ ]] || { echo "Invalid run number: $2" >&2; exit 2; }
      runs+=("$2")
      shift 2
      ;;
    --group)
      (($# >= 2)) || { echo "Missing argument for --group" >&2; exit 2; }
      [[ "$2" =~ ^[0-9][0-9]$ ]] || { echo "Invalid group (expected NN): $2" >&2; exit 2; }
      groups+=("$2")
      shift 2
      ;;
    --submit)
      submit=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if (("${#runs[@]}" == 0)); then
  runs=(79516)
fi

[[ -f "$submit_file" ]] || { echo "Submit template missing: $submit_file" >&2; exit 1; }
[[ -d "$list_dir" ]] || { echo "Input directory missing: $list_dir" >&2; exit 1; }
# We submit from the repository root, matching the established workflow.
cd "$repo_dir"

shopt -s nullglob
selected=()
selected_runs=()
counts=()
total_jobs=0

for run in "${runs[@]}"; do
  # The bracket expression restricts matches to exactly two digits; *_test
  # and *_100Test cannot match even if they exist in the same directory.
  candidates=("$list_dir"/file_run"${run}"_[0-9][0-9].list)
  matched_for_run=0
  for path in "${candidates[@]}"; do
    group=${path##*/}
    group=${group#file_run${run}_}
    group=${group%.list}

    if (("${#groups[@]}" != 0)); then
      include=false
      for wanted in "${groups[@]}"; do
        if [[ "$group" == "$wanted" ]]; then include=true; break; fi
      done
      [[ "$include" == true ]] || continue
    fi

    printf -v padded_run '%08d' "$((10#$run))"
    # Check the same three columns consumed by the submit-file queue:
    # events, tracking DST basename, skip. Validate embedded run number.
    job_count=$(awk -v run="$padded_run" '
      /^[[:space:]]*($|#)/ { next }
      {
        if (NF != 3 || $1 !~ /^[0-9]+$/ || $1+0 <= 0 ||
            $3 !~ /^[0-9]+$/ || $2 !~ /^DST_TRKR_.*[.]root$/ ||
            index($2, "-" run "-") == 0) {
          printf "Invalid queue row %s:%d: %s\n", FILENAME, FNR, $0 > "/dev/stderr"
          errors++
        } else {
          n++
        }
      }
      END {
        if (n == 0) { print "Input file list is empty" > "/dev/stderr"; errors++ }
        if (errors) exit 1
        print n
      }
    ' "$path") || exit 1

    selected+=("$path")
    selected_runs+=("$run")
    counts+=("$job_count")
    ((total_jobs += job_count))
    ((matched_for_run += 1))
  done

  if ((matched_for_run == 0)); then
    echo "No numbered input lists for run $run (or chosen groups)." >&2
    exit 1
  fi
done

# Don't modify the tracked submit template: make a temporary copy with
# run-specific OutDir and the selected input list.
temp_job=$(mktemp "${TMPDIR:-/tmp}/trackcalomatch_submit.XXXXXXXX.job")
trap 'rm -f "$temp_job"' EXIT

make_submit_file() {
  local input_list=$1
  local run=$2

  # Only the temporary submit description is changed. Every group of the
  # same run shares one output directory, while different runs are separate.
  awk -v input_list="$input_list" -v run="$run" '
    tolower($0) ~ /^[[:space:]]*outdir[[:space:]]*=/ {
      sub(/[[:space:]]*$/, "", $0)
      print $0 "_run" run
      found_outdir++
      next
    }
    tolower($0) ~ /^[[:space:]]*queue[[:space:]]+events,file,skip[[:space:]]+from[[:space:]]/ {
      print "queue events,file,skip from " input_list
      found_queue++
      next
    }
    { print }
    END {
      if (found_outdir != 1 || found_queue != 1) {
        print "Expected exactly one OutDir and queue events,file,skip from line" > "/dev/stderr"
        exit 1
      }
    }
  ' "$submit_file" > "$temp_job"
}

# Preflight the submit-file template before any jobs are submitted.
make_submit_file "${selected[0]}" "${selected_runs[0]}"
echo "Submit template: $submit_file"
echo "Run(s): ${runs[*]}"
echo "Groups: ${#selected[@]}"
echo "Total Condor jobs (queue rows): $total_jobs"
echo "Settings from submit template (OutDir is a base path):"
grep -Ei '^[[:space:]]*(JetMode|minTowerPt|OutDir|arguments)[[:space:]]*=' "$submit_file" || true
for i in "${!selected[@]}"; do
  echo "  Run ${selected_runs[i]}, ${selected[i]##*/}: ${counts[i]} jobs"
done

if [[ "$submit" != true ]]; then
  echo
  echo "PREVIEW ONLY: no jobs submitted. Add --submit to launch."
  exit 0
fi

command -v condor_submit >/dev/null || {
  echo "condor_submit not available; run this on an HTCondor submit host." >&2
  exit 1
}

# The template uses shared log/root paths and /tmp/<user> for event logs.
mkdir -p "$repo_dir/log" "$repo_dir/root" "/tmp/$(id -un)"
echo "Submitting ${#selected[@]} groups ($total_jobs jobs)."
for i in "${!selected[@]}"; do
  make_submit_file "${selected[i]}" "${selected_runs[i]}"
  echo "Submitting Run ${selected_runs[i]}, ${selected[i]##*/} (${counts[i]} jobs)"
  condor_submit "$temp_job"
done
echo "Submission complete; check with: condor_q -nobatch"
