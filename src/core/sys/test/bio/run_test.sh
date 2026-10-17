#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

cleanup()
{
	make clean OUTDIR="$outdir" TARGET="$target"
}

for opt in -O0 -O2; do
	outdir="_output_${opt#-}"
	target="test_bio_${opt#-}.exe"
	trap cleanup EXIT
	make -B -j4 OUTDIR="$outdir" TARGET="$target" OPT="$opt"
	"./$target"
	cleanup
	trap - EXIT
done
