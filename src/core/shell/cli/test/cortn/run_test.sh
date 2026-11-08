#!/usr/bin/env bash

if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
	echo 'Run this script with: bash ./run_test.sh' >&2
	# Keep caller running even with set -e
	return 0
fi

set -eu

make clean
make OPT=-O0
./test_cortn.exe

make clean
make OPT=-O2
./test_cortn.exe

echo all tests passed
