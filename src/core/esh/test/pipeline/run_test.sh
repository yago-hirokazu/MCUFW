#!/usr/bin/env bash
set -eu

for opt in -O0 -O2; do
	make clean
	make OPTFLAGS="$opt"
	./test_pipeline.exe
done
