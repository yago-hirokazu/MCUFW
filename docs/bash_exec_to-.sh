#!/bin/sh

find . -depth -type f -name "*_*" -not -name "bash_simu_to-.sh" -not -name "bash_exec_to-.sh" | while read -r file; do
    dir=$(dirname "$file")
    base=$(basename "$file")
    new_base=$(echo "$base" | tr '_' '-')

    # 実際に名前を変更
    mv "$file" "$dir/$new_base"
done
