#!/bin/bash

# Per-run merge helper retained from the original analysis workflow.
script_path=$(readlink -f "${BASH_SOURCE[0]}")
script_dir=$(dirname "${script_path}")
repo_dir=$(dirname "${script_dir}")
cd "${repo_dir}/analysis"

runs=(79509 79510 79511 79512 79513 79514 79515 79516)

for ((i = 0; i < ${#runs[@]}; i++)); do
  echo "hadd run ${runs[$i]}"
  hadd "all_ee_reco_${runs[$i]}.root" \
    "${repo_dir}"/root/ee_reco/outputKFParticle_ee_reco_000"${runs[$i]}"_*_00000.root
done
