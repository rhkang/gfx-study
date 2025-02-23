#!/bin/bash

script=$(readlink -f "$0")
script_dir=$(dirname "$script")

root=$(cd "$script_dir/../" && pwd)

directories=(
    "$root/src"
    "$root/sandbox"
)

format_files() {
    for dir in "${directories[@]}"; do
        find "$dir" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read -r file; do
            clang-format -i --style=file:$root/.clang-format "$file"
            echo "Formatted $file"
        done
    done
}

format_files