#!/bin/sh

for f in result_test*_sh.txt; do cp -- "$f" "answer_${f#result_}"; done

