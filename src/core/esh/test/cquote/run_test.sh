#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

trap 'make clean' EXIT
for opt in -O0 -O2; do
	make clean
	make -B -j4 OPT="$opt"
	./test_cquote.exe
done
