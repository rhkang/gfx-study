#!/bin/bash

script=$(readlink -f "$0")
script_dir=$(dirname "$script")

root=$(cd "$script_dir/../" && pwd)

directories=(
    "$root/src"
    "$root/sandbox"
)

echo "Script directory: $script_dir"
echo "Root directory: $root"
echo "Target directories: ${directories[@]}"

format_files() {
    for dir in "${directories[@]}"; do
        find "$dir" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read -r file; do
            temp_file=$(mktemp)
            cp "$file" "$temp_file"
            clang-format -i --style=file:$root/.clang-format "$file"

            if ! cmp -s "$temp_file" "$file"; then  # Compare original and formatted content
                echo "Formatted: $file"
            fi
            
            rm "$temp_file"
        done
    done
}

format_files