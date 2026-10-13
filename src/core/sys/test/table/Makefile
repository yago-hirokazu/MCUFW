TARGET ?= test_table.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../h

OBJS = $(OUTDIR)/test_table.o
OBJS += $(OUTDIR)/conf.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_table.o: test_table.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/conf.o: ../../conf/conf.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
