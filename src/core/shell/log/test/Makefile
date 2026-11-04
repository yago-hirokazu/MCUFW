CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -I../include -DNDEBUG
TARGETS = test_error.exe test_windows.exe test_clock.exe
HEADERS = $(wildcard ../include/*.h)

.PHONY: all test clean
all: $(TARGETS)

test_error.exe: test_error.c ../log_error.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

test_windows.exe: test_windows.c ../log_error.c ../dev/windows/log_adapt.c $(HEADERS)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

test_clock.exe: test_clock.c ../dev/windows/log_adapt.c $(HEADERS)
	$(CC) $(CFLAGS) test_clock.c -o $@

test: all
	./test_error.exe > dump.txt
	python -c "from pathlib import Path; s=Path('dump.txt').read_text(); assert s == 'index time wrap err file line\n0 4294967295 7 1201 12 42\n1 0 8 1202 12 43\n', repr(s)"
	./test_windows.exe
	./test_clock.exe

clean:
	rm -f $(TARGETS) dump.txt *.o *.d
