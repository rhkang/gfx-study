#!/bin/bash

# set -e

script=$(readlink -f "$0")
script_dir=$(dirname "$script")
project_root=$(cd "$script_dir/../" && pwd)
dxc_root=$(cd "$project_root/tool/dxc" && pwd)

dxc="$dxc_root/bin/dxc"

if [ ! -f "$dxc" ]; then
    echo "Compiler: $dxc not exists."
    exit 1
else
    echo "Compiler: $dxc"
fi

root=$(cd "$project_root/shaders/" && pwd)

if [ ! -d "$root" ]; then
    echo "Root directory does not exist: $root"
    exit 1
fi

echo "Root Directory: $root"
echo

# Ensure the output directory exists
mkdir -p "$root/out"

hlsl_compile() {
    local files=("$@")
    if [ ${#files[@]} -eq 0 ]; then
        files=($(find $root -name "*.hlsl"))
    fi

    for file in "${files[@]}"; do
        echo "Processing file: $file"
        relative_path="${file#$root/}"
        output_dir="$root/out/$(dirname "$relative_path")"
        mkdir -p "$output_dir"

        echo "Compile $file -> $output_dir/$(basename "$file" .hlsl).vert.spv"
        "$dxc" -spirv -T vs_6_0 -E vert "$file" -Fo "$output_dir/$(basename "$file" .hlsl).vert.spv"

        echo "Compile $file -> $output_dir/$(basename "$file" .hlsl).frag.spv"
        "$dxc" -spirv -T ps_6_0 -E frag "$file" -Fo "$output_dir/$(basename "$file" .hlsl).frag.spv"
    done
}

hlsl_compile "$@"