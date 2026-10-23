TARGET ?= test_pipe.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../h
CFLAGS += -I../stub

OBJS = $(OUTDIR)/test_pipe.o
OBJS += $(OUTDIR)/sys2.o
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/rdwri.o
OBJS += $(OUTDIR)/fio.o
OBJS += $(OUTDIR)/alloc.o
OBJS += $(OUTDIR)/iget.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o
OBJS += $(OUTDIR)/ramdisk.o
OBJS += $(OUTDIR)/bdev.o
OBJS += $(OUTDIR)/sched_stub.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_pipe.o: test_pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sys2.o: ../../sys/sys2.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/pipe.o: ../../sys/pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/rdwri.o: ../../sys/rdwri.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/fio.o: ../../sys/fio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/alloc.o: ../../sys/alloc.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/iget.o: ../../sys/iget.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bio.o: ../../dev/bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/conf.o: ../../conf/conf.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_stub.o: ../stub/sched_stub.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bdev.o: ../../dev/bdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ramdisk.o: ../../dev/ramdisk.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
