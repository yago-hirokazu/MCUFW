TARGET = test_pipe.exe
OUTDIR = _output
CC = gcc
OPT ?= -O0
include ../../../sys/dev/windows/sched_backend.mk
include ../../target/windows/sched_compat.mk

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -I../../include -I../../../sys/dev/windows -I../../../sys/h
CFLAGS += $(SCHED_COMPAT_CPPFLAGS)

OBJS = $(OUTDIR)/test_pipe.o
OBJS += $(addprefix $(OUTDIR)/,$(SCHED_COMPAT_CSOURCES:.c=.o))
OBJS += $(addprefix $(OUTDIR)/,$(SCHED_ASOURCES:.S=.o))
OBJS += $(OUTDIR)/file.o
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/sched.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_pipe.o: test_pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn.o: ../../cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_context.o: ../../../sys/dev/windows/cortn_context.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_switch.o: ../../../sys/dev/windows/cortn_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/file.o: ../../file.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/pipe.o: ../../pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched.o: ../../sched.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bio.o: ../../../sys/dev/bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/conf.o: ../../../sys/conf/conf.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_cortn_compat.o: ../../target/windows/sched_cortn_compat.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_thread_compat.o: ../../target/windows/sched_thread_compat.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
