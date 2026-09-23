#!/bin/sh

set -u

cd "$(dirname "$0")" || exit 1

TDMGCC=../../../../target/windows/TDM-GCC
PATH="$TDMGCC/tdm/bin:$TDMGCC/make-3.81-bin/bin:$TDMGCC/make-3.81-dep/bin:$PATH"
MSYSTEM=${MSYSTEM:-MINGW64}
export PATH MSYSTEM

if ! make clean || ! make; then
	echo "error: make failed" >&2
	exit 1
fi

k=1
while [ "$k" -le 10 ]; do
	if ! ./test_exe.exe "test${k}.sh"; then
		echo "error: test${k}.sh failed" >&2
		exit 1
	fi

	if ! diff -q "result_test${k}_sh.txt" "answer_test${k}_sh.txt" >/dev/null; then
		echo "error @ test${k}" >&2
		echo "error detected!!!" >&2
		exit 1
	fi

	k=$((k + 1))
done

echo "all tests passed"
