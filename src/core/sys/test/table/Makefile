TARGET ?= test_table.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../h

OBJS = $(OUTDIR)/test_table.o
OBJS += $(OUTDIR)/conf.o
OBJS += $(OUTDIR)/ramdisk.o
OBJS += $(OUTDIR)/bdev.o
OBJS += $(OUTDIR)/bio.o

OBJS += $(OUTDIR)/sched_stub.o

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

$(OUTDIR)/bio.o: ../../dev/bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bdev.o: ../../dev/bdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ramdisk.o: ../../dev/ramdisk.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_stub.o: ../stub/sched_stub.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
