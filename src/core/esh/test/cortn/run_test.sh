#!/bin/sh

set -eu

make clean
make OPT=-O0
./test_cortn.exe

make clean
make OPT=-O2
./test_cortn.exe

echo all tests passed
