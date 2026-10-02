TARGET = test_sched.exe
OUTDIR = _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -I../../include -I../../target/windows

OBJS = $(OUTDIR)/test_sched.o
OBJS += $(OUTDIR)/cortn.o
OBJS += $(OUTDIR)/cortn_context.o
OBJS += $(OUTDIR)/cortn_switch.o
OBJS += $(OUTDIR)/file.o
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/sched.o
OBJS += $(OUTDIR)/sched_cortn.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_sched.o: test_sched.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn.o: ../../cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_context.o: ../../target/windows/cortn_context.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_switch.o: ../../target/windows/cortn_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/file.o: ../../file.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/pipe.o: ../../pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched.o: ../../sched.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched_cortn.o: ../../target/windows/sched_cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
