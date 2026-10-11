#!/bin/sh

set -u

cd "$(dirname "$0")" || exit 1

MSYSTEM=${MSYSTEM:-MINGW64}
export PATH MSYSTEM

if ! make; then
	echo "error: make failed" >&2
	exit 1
fi

output=$(./test_exe.exe test11.sh)
status=$?
if [ "$status" -ne 0 ]; then
	echo "error: test11.sh failed" >&2
	exit 1
fi

actual=$(printf '%s\n' "$output" | tr -d '\r' | tail -n 8)
expected='outer
1
inner
1
inner
2
break 2
after loops'

if [ "$actual" != "$expected" ]; then
	echo "error @ test11" >&2
	printf '%s\n' "$output"
	echo "error detected!!!" >&2
	exit 1
fi

echo "argument break test passed"

output=$(./test_exe.exe test12.sh)
status=$?
if [ "$status" -ne 0 ]; then
	echo "error: test12.sh failed" >&2
	exit 1
fi

actual=$(printf '%s\n' "$output" | tr -d '\r' | tail -n 8)
expected='outer
k=1
inner
l=1
inner
l=2
break 2
after loops'

if [ "$actual" != "$expected" ]; then
	echo "error @ test12" >&2
	printf '%s\n' "$output"
	echo "error detected!!!" >&2
	exit 1
fi

echo "quoted variable argument test passed"
