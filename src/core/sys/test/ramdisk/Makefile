TARGET ?= test_ramdisk.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT) -std=c11
CFLAGS += -Wall -Wextra -Werror -pedantic -I../../h

OBJS = $(OUTDIR)/test_ramdisk.o $(OUTDIR)/ramdisk.o
OBJS += $(OUTDIR)/bdev.o $(OUTDIR)/bio.o $(OUTDIR)/conf.o

OBJS += $(OUTDIR)/sched_stub.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/test_ramdisk.o: test_ramdisk.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTDIR)/ramdisk.o: ../../dev/ramdisk.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTDIR)/bdev.o: ../../dev/bdev.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTDIR)/bio.o: ../../dev/bio.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTDIR)/conf.o: ../../conf/conf.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

$(OUTDIR)/sched_stub.o: ../stub/sched_stub.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
