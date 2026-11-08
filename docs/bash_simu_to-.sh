#!/bin/sh

find . -depth -type f -name "*_*" -not -name "bash_simu_to-.sh" -not -name "bash_exec_to-.sh" | while read -r file; do
    # ディレクトリパスとファイル名を分離
    dir=$(dirname "$file")
    base=$(basename "$file")

    # ファイル名の中の _ を - に置換
    new_base=$(echo "$base" | tr '_' '-')

    echo "Rename: $file -> $dir/$new_base"
done
