TARGET = test_pipeline_exec.exe
OUTDIR = _output
CC = gcc
CXX = g++

CFLAGS = -mno-ms-bitfields -g
CFLAGS += -Wall -Wextra
CFLAGS += -I. -I../../include -I../../../elib
CFLAGS += -I../../target/windows -I../../../../target/windows/Adapt
CFLAGS += -DUSE_STACKMACH
LDFLAGS = -lwinmm

CSOURCES = test_pipeline_exec.c
CSOURCES += break.c code.c cortn.c echo.c esh_debug.c exe.c expr.c
CSOURCES += file.c help.c lex.c par.c pipe.c run.c sched.c test.c
CSOURCES += tok.c tty.c cortn_context.c sched_cortn.c
CSOURCES += stdio_adapt.c tty_adapt.c cmd_sample.c qbyte.c symchar.c
ASOURCES = cortn_switch.S

OBJS = $(addprefix $(OUTDIR)/,$(CSOURCES:.c=.o))
OBJS += $(addprefix $(OUTDIR)/,$(ASOURCES:.S=.o))

vpath %.c . ../.. ../../../usr/cmd ../../../elib ../../target/windows
vpath %.S ../../target/windows

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
