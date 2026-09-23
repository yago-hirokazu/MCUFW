
# CFLAGS
CFLAGS=-I./include -I./target/windows -I../elib -I./stub

.PHONY: check-syntax
check-syntax:
	gcc $(CFLAGS) -o null -Wall -Wextra -pedantic -fsyntax-only $(CHK_SOURCES)

