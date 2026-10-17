#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

outdir=
target=
cleanup()
{
	if [ -n "$outdir" ]; then
		make clean OUTDIR="$outdir" TARGET="$target"
	fi
}
trap cleanup EXIT

for opt in -O0 -O2; do
	outdir="_output_${opt#-}"
	target="test_ramdisk_${opt#-}.exe"
	make -B -j4 OUTDIR="$outdir" TARGET="$target" OPT="$opt"
	"./$target"
	cleanup
	outdir=
done
