#!/bin/bash
set -euo pipefail

script_path=$(readlink -f "${BASH_SOURCE[0]}")
script_dir=$(dirname "${script_path}")
repo_dir=$(dirname "${script_dir}")
cd "${repo_dir}"

input_dir="root/ee_reco"
part_dir="root/ee_reco_hadd_parts"
output="root/ee_reco_merged.root"
batch_size=500

mkdir -p "$part_dir"

files=()
part=0

merge_batch()
{
    local part_file
    printf -v part_file "%s/part_%04d.root" "$part_dir" "$part"

    echo "Merging ${#files[@]} files -> $part_file"
    hadd -f -n 100 "$part_file" "${files[@]}"

    files=()
    part=$((part + 1))
}

while IFS= read -r -d '' file; do
    files+=("$file")

    if (( ${#files[@]} >= batch_size )); then
        merge_batch
    fi
done < <(
    find "$input_dir" -maxdepth 1 -type f -name '*.root' -print0 |
        sort -z -V
)

if (( ${#files[@]} > 0 )); then
    merge_batch
fi

if (( part == 0 )); then
    echo "No ROOT files found under $input_dir" >&2
    exit 1
fi

echo "Final merge -> $output"
hadd -f -n 100 "$output" "$part_dir"/part_*.root
