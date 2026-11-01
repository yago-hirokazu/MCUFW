#!/bin/sh

if [ "$#" -eq 0 ]; then
    echo "usage: bash ./update_test_answers.sh <# of tests>"
    exit 1
fi

echo "# of test = $1"
k=1
while [ "$k" -le $1 ]; do
    ./test_exe.exe "test${k}.sh";
    k=$((k + 1))
done

for f in result_test*_sh.txt; do cp -- "$f" "answer_${f#result_}"; done

for f in answer_test*_sh.txt; do
    awk '{ sub(/\r$/, ""); printf "%s\r\n", $0 }' "$f" > "${f}.tmp" &&
        mv -- "${f}.tmp" "$f"
done
