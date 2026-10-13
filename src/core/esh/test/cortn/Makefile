TARGET = test_cortn.exe
OUTDIR = _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -I../../include -I../../../sys/dev/windows

OBJS = $(OUTDIR)/test_cortn.o
OBJS += $(OUTDIR)/cortn.o
OBJS += $(OUTDIR)/cortn_context.o
OBJS += $(OUTDIR)/cortn_switch.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_cortn.o: test_cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn.o: ../../cortn.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_context.o: ../../../sys/dev/windows/cortn_context.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/cortn_switch.o: ../../../sys/dev/windows/cortn_switch.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
