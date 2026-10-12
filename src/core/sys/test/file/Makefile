TARGET ?= test_file.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic -std=c11
CFLAGS += -I../../h
CFLAGS += -I../stub

OBJS = $(OUTDIR)/test_file.o
OBJS += $(OUTDIR)/fio.o
OBJS += $(OUTDIR)/alloc.o
OBJS += $(OUTDIR)/iget.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o
OBJS += $(OUTDIR)/sched_stub.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_file.o: test_file.c
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

clean:
	rm -rf $(OUTDIR) $(TARGET)
