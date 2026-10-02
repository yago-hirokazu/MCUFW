TARGET = test_pipe.exe
OUTDIR = _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -I../../include -I../../target/windows

OBJS = $(OUTDIR)/test_pipe.o
OBJS += $(OUTDIR)/file.o
OBJS += $(OUTDIR)/pipe.o
OBJS += $(OUTDIR)/sched.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_pipe.o: test_pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/file.o: ../../file.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/pipe.o: ../../pipe.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/sched.o: ../../sched.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
