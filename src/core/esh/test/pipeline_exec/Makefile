TARGET = test_pipeline_exec.exe
OUTDIR = _output
CC = gcc
CXX = g++
include ../../../sys/dev/windows/sched_adapt.mk

CFLAGS = -mno-ms-bitfields -g
CFLAGS += -Wall -Wextra
CFLAGS += -I. -I../../include -I../../../elib -I../../../sys/h
CFLAGS += -I../../target/windows -I../../../sys/dev/windows
CFLAGS += -I../../../../target/windows/Adapt
CFLAGS += -DUSE_STACKMACH
CFLAGS += -DESH_STDIO_PIPE
CFLAGS += -DESH_SCRIPT
CFLAGS += $(SCHED_CPPFLAGS)
LDFLAGS = -lwinmm

CSOURCES = test_pipeline_exec.c
CSOURCES += break.c code.c echo.c esh_debug.c exe.c expr.c
CSOURCES += cat.c script.c
CSOURCES += help.c lex.c par.c run.c test.c tok.c tty.c
CSOURCES += esh_adapt.c stdio_adapt.c tty_adapt.c
CSOURCES += cmd_sample.c qbyte.c symchar.c
CSOURCES += $(SCHED_CSOURCES)
CSOURCES += alloc.c bio.c conf.c fio.c iget.c pipe.c rdwri.c sched.c sys2.c
CSOURCES += bdev.c ramdisk.c
ASOURCES = $(SCHED_ASOURCES)

OBJS = $(addprefix $(OUTDIR)/,$(CSOURCES:.c=.o))
OBJS += $(addprefix $(OUTDIR)/,$(ASOURCES:.S=.o))

vpath %.c . ../.. ../../../usr/cmd ../../../elib ../../target/windows
vpath %.c ../../../sys/dev ../../../sys/conf
vpath %.c ../../../sys/sys
vpath %.c ../../../sys/dev/windows
vpath %.S ../../../sys/dev/windows

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -c -MMD -MP $< -o $@

$(OUTDIR)/%.o: %.S
	$(CC) $(CFLAGS) $(OPTFLAGS) -c -MMD -MP $< -o $@

-include $(OBJS:.o=.d)

clean:
	rm -rf $(OUTDIR) $(TARGET)
