#!/usr/bin/env bash

if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
	echo 'Run this script with: bash ./run_test2.sh' >&2
	# Keep caller running even with set -e
	return 0
fi

RZ_FNAME=result_test2_sh.txt
AN_FNAME=answer_test2_sh.txt

rm -f "$RZ_FNAME"

if ! make clean
then
    echo "error: make failed" >&2
    exit 1
fi


if ! make
then
    echo "error: make failed" >&2
    exit 1
fi

if ! ./test_par.exe test2.sh
then
    echo "error: ./test_par.exe test2.sh failed" >&2
    exit 1
fi

if diff "$RZ_FNAME" "$AN_FNAME"
then
    echo test pass
    exit 0
else
    echo test fail
    exit 1
fi
