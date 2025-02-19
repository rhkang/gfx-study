#!/bin/bash

script=$(readlink -f "$0")
script_dir=$(dirname "$script")

root=$(cd "$script_dir/../" && pwd)

directories=(
    "$root/src"
    "$root/sandbox"
)

linecount_files() {
    total_count=0
    for dir in "${directories[@]}"; do
        while IFS= read -r -d '' file; do
            file_count=$(wc -l < "$file")
            total_count=$((total_count + file_count))
            echo "$file: $file_count"
        done < <(find "$dir" \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0)
    done

    echo "Total line count: $total_count"
}

linecount_files