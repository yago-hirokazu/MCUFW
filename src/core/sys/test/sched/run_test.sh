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

for capacity in default small; do
	extra_flags=
	suffix=
	if [ "$capacity" = small ]; then
		extra_flags=-DNBUF=3
		suffix=_small
	fi
	for backend in cortn thread; do
		for opt in -O0 -O2; do
			outdir="_output_${backend}_${opt#-}${suffix}"
			target="test_sched_${backend}_${opt#-}${suffix}.exe"
			make -B -j4 SCHED_BACKEND="$backend" OUTDIR="$outdir" \
				TARGET="$target" OPT="$opt" \
				EXTRA_CPPFLAGS="$extra_flags"
			timeout 30s "./$target"
			cleanup
			outdir=
		done
	done
done
