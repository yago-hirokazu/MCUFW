#!/bin/sh

set -u

cd "$(dirname "$0")" || exit 1

MSYSTEM=${MSYSTEM:-MINGW64}
export MSYSTEM

if ! make clean || ! make; then
	echo "error: make failed" >&2
	exit 1
fi

k=1
while [ "$k" -le 2 ]; do
	result="result_test${k}_sh.txt"
	answer="answer_test${k}_sh.txt"

	rm -f "$result"

	if ! ./test_par.exe "test${k}.sh"; then
		echo "error: test${k}.sh failed" >&2
		exit 1
	fi

	if ! diff -u "$answer" "$result"; then
		echo "error: test${k}.sh result differs" >&2
		exit 1
	fi

	k=$((k + 1))
done

echo "all tests passed"
