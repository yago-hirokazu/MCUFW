TARGET ?= test_bio.exe
OUTDIR ?= _output
CC = gcc
OPT ?= -O0

CFLAGS = -mno-ms-bitfields -g $(OPT)
CFLAGS += -Wall -Wextra -Werror -pedantic
CFLAGS += -I../../h

OBJS = $(OUTDIR)/test_bio.o
OBJS += $(OUTDIR)/bio.o
OBJS += $(OUTDIR)/conf.o

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/test_bio.o: test_bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/bio.o: ../../dev/bio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/conf.o: ../../conf/conf.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTDIR) $(TARGET)
