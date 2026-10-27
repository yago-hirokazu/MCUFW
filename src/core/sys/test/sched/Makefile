TARGET ?= test_sched.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0
include ../../dev/windows/sched_adapt.mk

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../h
CFLAGS += -I../../dev/windows
CFLAGS += -DSCHED_TEST $(SCHED_CPPFLAGS)
CFLAGS += $(EXTRA_CPPFLAGS)

OBJS = $(OUTDIR)/test_sched.o
OBJS += $(OUTDIR)/sched.o
OBJS += $(addprefix $(OUTDIR)/,$(SCHED_CSOURCES:.c=.o))
OBJS += $(addprefix $(OUTDIR)/,$(SCHED_ASOURCES:.S=.o))
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/rdwri.o
OBJS += $(OUTDIR)/fio.o
OBJS += $(OUTDIR)/alloc.o
OBJS += $(OUTDIR)/iget.o
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

$(OUTDIR)/test_sched.o: test_sched.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched.o: ../../sys/sched.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn.o: ../../dev/windows/cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_context.o: ../../dev/windows/cortn_context.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_switch.o: ../../dev/windows/cortn_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_cortn.o: ../../dev/windows/sched_cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_thread.o: ../../dev/windows/sched_thread.c
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

$(OUTDIR)/bdev.o: ../../dev/bdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/ramdisk.o: ../../dev/ramdisk.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
