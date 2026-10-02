#!/bin/sh

set -eu

make clean
make OPT=-O0
./test_sched.exe

make clean
make OPT=-O2
./test_sched.exe

echo all tests passed
