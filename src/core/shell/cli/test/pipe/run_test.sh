#!/usr/bin/env bash

if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
	echo 'Run this script with: bash ./run_test.sh' >&2
	# Keep caller running even with set -e
	return 0
fi

set -eu
cd "$(dirname "$0")"

for backend in cortn thread; do
	for opt in -O0 -O2; do
		outdir="_output_${backend}_${opt#-}"
		target="test_pipe_${backend}_${opt#-}.exe"
		make -B -j4 SCHED_BACKEND="$backend" OUTDIR="$outdir" \
			TARGET="$target" OPT="$opt"
		timeout 30s "./$target"
	done
done
