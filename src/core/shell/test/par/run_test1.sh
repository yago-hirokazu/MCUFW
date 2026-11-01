#!/bin/sh

RZ_FNAME=./result_test1_sh.txt
AN_FNAME=./answer_test1_sh.txt

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

if ! ./test_par.exe test1.sh
then
    echo "error: test_par.exe test1.sh failed" >&2
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
