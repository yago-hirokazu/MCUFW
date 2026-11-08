#!/usr/bin/env bash

if [[ ${BASH_SOURCE[0]} != "$0" ]]; then
	echo 'Run this script with: bash ./run_test.sh' >&2
	# Keep caller running even with set -e
	return 0
fi

set -u

cd "$(dirname "$0")" || exit 1

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

	if ! diff --strip-trailing-cr -u "answer_test${k}_sh.txt" \
		"result_test${k}_sh.txt"; then
		echo "error: test${k}.sh output comparison failed" >&2
		exit 1
	fi

	k=$((k + 1))
done

echo "all tests passed"
