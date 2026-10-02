#!/bin/sh

set -eu

make clean
make OPT=-O0
./test_pipe.exe

make clean
make OPT=-O2
./test_pipe.exe

echo all tests passed
