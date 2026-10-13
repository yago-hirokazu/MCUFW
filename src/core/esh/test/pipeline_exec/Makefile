TARGET = test_pipeline_exec.exe
OUTDIR = _output
CC = gcc
CXX = g++
include ../../../sys/dev/windows/sched_backend.mk
include ../../target/windows/sched_compat.mk

CFLAGS = -mno-ms-bitfields -g
CFLAGS += -Wall -Wextra
CFLAGS += -I. -I../../include -I../../../elib -I../../../sys/h
CFLAGS += -I../../target/windows -I../../../sys/dev/windows
CFLAGS += -I../../../../target/windows/Adapt
CFLAGS += -DUSE_STACKMACH
CFLAGS += -DESH_STDIO_PIPE
CFLAGS += -DESH_SCRIPT
CFLAGS += $(SCHED_COMPAT_CPPFLAGS)
LDFLAGS = -lwinmm

CSOURCES = test_pipeline_exec.c
CSOURCES += break.c code.c echo.c esh_debug.c exe.c expr.c
CSOURCES += cat.c script.c
CSOURCES += file.c help.c lex.c par.c pipe.c run.c sched.c test.c
CSOURCES += tok.c tty.c $(SCHED_COMPAT_CSOURCES)
CSOURCES += stdio_adapt.c tty_adapt.c cmd_sample.c qbyte.c symchar.c
CSOURCES += bio.c conf.c
ASOURCES = $(SCHED_ASOURCES)

OBJS = $(addprefix $(OUTDIR)/,$(CSOURCES:.c=.o))
OBJS += $(addprefix $(OUTDIR)/,$(ASOURCES:.S=.o))

vpath %.c . ../.. ../../../usr/cmd ../../../elib ../../target/windows
vpath %.c ../../../sys/dev ../../../sys/conf
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
