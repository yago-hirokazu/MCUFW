#!/bin/sh

k=1
while [ "$k" -le 10 ]; do
    ./test_exe.exe "test${k}.sh";
    k=$((k + 1))
done

for f in result_test*_sh.txt; do cp -- "$f" "answer_${f#result_}"; done

for f in answer_test*_sh.txt; do
    awk '{ sub(/\r$/, ""); printf "%s\r\n", $0 }' "$f" > "${f}.tmp" &&
        mv -- "${f}.tmp" "$f"
done
