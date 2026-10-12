#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

for opt in -O0 -O2; do
	outdir="_output_${opt#-}"
	target="test_bio_${opt#-}.exe"
	make -B -j4 OUTDIR="$outdir" TARGET="$target" OPT="$opt"
	"./$target"
done
