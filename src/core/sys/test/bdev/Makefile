TARGET ?= test_bdev.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -I../../h

OBJS = $(OUTDIR)/test_bdev.o
OBJS += $(OUTDIR)/bdev.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o
OBJS += $(OUTDIR)/ramdisk.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_bdev.o: test_bdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bdev.o: ../../dev/bdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bio.o: ../../dev/bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/conf.o: ../../conf/conf.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ramdisk.o: ../../dev/ramdisk.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
