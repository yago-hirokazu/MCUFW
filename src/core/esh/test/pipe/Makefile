TARGET = test_pipe.exe
OUTDIR = _output
CC = gcc
OPT ?= -O0
include ../../../sys/dev/windows/sched_backend.mk

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../include -I../../../sys/dev/windows -I../../../sys/h
CFLAGS += $(SCHED_CPPFLAGS)

OBJS = $(OUTDIR)/test_pipe.o
OBJS += $(addprefix $(OUTDIR)/,$(SCHED_CSOURCES:.c=.o))
OBJS += $(addprefix $(OUTDIR)/,$(SCHED_ASOURCES:.S=.o))
OBJS += $(OUTDIR)/alloc.o
OBJS += $(OUTDIR)/fio.o
OBJS += $(OUTDIR)/iget.o
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/rdwri.o
OBJS += $(OUTDIR)/sched.o
OBJS += $(OUTDIR)/sys2.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o
OBJS += $(OUTDIR)/ramdisk.o
OBJS += $(OUTDIR)/bdev.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_pipe.o: test_pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn.o: ../../../sys/dev/windows/cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_context.o: ../../../sys/dev/windows/cortn_context.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_switch.o: ../../../sys/dev/windows/cortn_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/alloc.o: ../../../sys/sys/alloc.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/fio.o: ../../../sys/sys/fio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/iget.o: ../../../sys/sys/iget.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/pipe.o: ../../../sys/sys/pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/rdwri.o: ../../../sys/sys/rdwri.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched.o: ../../../sys/sys/sched.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sys2.o: ../../../sys/sys/sys2.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bio.o: ../../../sys/dev/bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/conf.o: ../../../sys/conf/conf.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_cortn.o: ../../../sys/dev/windows/sched_cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_thread.o: ../../../sys/dev/windows/sched_thread.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bdev.o: ../../../sys/dev/bdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ramdisk.o: ../../../sys/dev/ramdisk.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
