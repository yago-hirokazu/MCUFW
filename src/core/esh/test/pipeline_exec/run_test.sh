#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

for backend in cortn thread; do
	for opt in -O0 -O2; do
		outdir="_output_${backend}_${opt#-}"
		target="test_pipeline_${backend}_${opt#-}.exe"
		make -B -j4 SCHED_BACKEND="$backend" OUTDIR="$outdir" \
			TARGET="$target" OPTFLAGS="$opt"
		timeout 30s "./$target"
	done
done
