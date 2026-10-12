TARGET ?= test_sched.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0
BACKEND ?= cortn

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../h
CFLAGS += -I../../../esh/include -I../../../esh/target/windows
CFLAGS += -DSYS_SCHED -DSCHED_TEST

ifeq ($(BACKEND),cortn)
BACKEND_OBJS = $(OUTDIR)/cortn.o $(OUTDIR)/cortn_context.o
BACKEND_OBJS += $(OUTDIR)/cortn_switch.o $(OUTDIR)/sched_cortn.o
else ifeq ($(BACKEND),thread)
BACKEND_OBJS = $(OUTDIR)/sched_thread.o
else
$(error Unknown BACKEND: $(BACKEND))
endif

OBJS = $(OUTDIR)/test_sched.o
OBJS += $(OUTDIR)/sched.o
OBJS += $(BACKEND_OBJS)
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/rdwri.o
OBJS += $(OUTDIR)/fio.o
OBJS += $(OUTDIR)/alloc.o
OBJS += $(OUTDIR)/iget.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o

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

$(OUTDIR)/cortn.o: ../../../esh/cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_context.o: ../../../esh/target/windows/cortn_context.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_switch.o: ../../../esh/target/windows/cortn_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_cortn.o: ../../../esh/target/windows/sched_cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_thread.o: ../../../esh/target/windows/sched_thread.c
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

clean:
	rm -rf $(OUTDIR) $(TARGET)
