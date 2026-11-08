#!/usr/bin/env bash

if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
	echo 'Run this script with: bash ./run_test.sh' >&2
	# Keep caller running even with set -e
	return 0
fi

set -eu

for opt in -O0 -O2; do
	make clean
	make OPTFLAGS="$opt"
	./test_pipeline.exe
done
