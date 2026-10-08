#!/bin/bash

set -euo pipefail

script_path=$(readlink -f "${BASH_SOURCE[0]}")
script_dir=$(dirname "${script_path}")
repo_dir=$(dirname "${script_dir}")
cd "${repo_dir}"

submissionFile=scripts/myCondor_KFP.job
listDir=fileLists

# Keep the run list here, as in the original submission script.
runs="79516"

mkdir -p log root
shopt -s nullglob

for run in ${runs}; do
  optionsFiles=("${listDir}"/file_run"${run}"_*.list)
  if (( ${#optionsFiles[@]} == 0 )); then
    echo "No input lists found for run ${run} under ${listDir}" >&2
    continue
  fi

  for optionsPath in "${optionsFiles[@]}"; do
    currentOptionsFile=$(awk '/^Queue / {n=split($NF,path,"/"); print path[n]}' "${submissionFile}")
    newOptionsFile=$(basename "${optionsPath}")

    if [[ -z "${currentOptionsFile}" ]]; then
      echo "Cannot find a Queue ... from LIST line in ${submissionFile}" >&2
      exit 1
    fi

    echo "Current options file: ${currentOptionsFile}"
    echo "New options file:     ${newOptionsFile}"
    sed -i -e "s/${currentOptionsFile}/${newOptionsFile}/g" "${submissionFile}"
    condor_submit "${submissionFile}"
  done
done
