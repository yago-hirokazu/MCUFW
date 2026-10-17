#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")"

for opt in -O0 -O2; do
	outdir="_output_${opt#-}"
	make -B -j4 OUTDIR="$outdir" OPT="$opt"
	for header in buf.h conf.h param.h sys_error.h file.h inode.h pipe.h sched.h \
		sched_backend.h; do
		gcc -I../../h -std=c11 -Wall -Wextra -Werror -pedantic \
			"$opt" -DTEST_HEADER=\"$header\" -fsyntax-only \
			test_single.c
		g++ -I../../h -std=c++11 -Wall -Wextra -Werror -pedantic \
			"$opt" -DTEST_HEADER=\"$header\" -x c++ \
			-fsyntax-only test_single.c
	done
done

echo "all tests passed"
