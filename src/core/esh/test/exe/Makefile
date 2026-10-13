
# Default target
TARGET = test_exe.exe

# Commands
CC = gcc
include ../../../sys/dev/windows/sched_backend.mk
ifeq ($(MSYSTEM),MINGW64)
DEL = rm -f
COPY = cp
MKDIR = mkdir -p
RMDIR = rm -rf
else ifeq ($(OS),Windows_NT)
DEL = del /q
COPY = copy
REN = ren
MKDIR = md
RMDIR = rd /s /q
else
DEL = rm -rf
COPY = cp
MKDIR = mkdir
RMDIR = rm -rf
endif

# Output directory
OUTDIR = _output

# Search path
vpath %.c .
vpath %.c ../..
vpath %.c ../../../usr/cmd
vpath %.c ../../../elib
vpath %.c ../../../sys/dev
vpath %.c ../../../sys/conf
vpath %.c ../../../sys/sys
vpath %.c ../../../sys/dev/windows
ifeq ($(OS),Windows_NT)
vpath %.c ../../target/windows/
else
vpath %.c ../../target/cygwin/
endif
vpath %.S ../../../sys/dev/windows/
vpath %.o _output

# C sources

## .
CSOURCES := test_exe.c

## ../..
CSOURCES += break.c
CSOURCES += code.c
CSOURCES += echo.c
CSOURCES += esh_debug.c
CSOURCES += exe.c
CSOURCES += expr.c
CSOURCES += help.c
CSOURCES += par.c
CSOURCES += run.c
CSOURCES += lex.c
CSOURCES += test.c
CSOURCES += tok.c
CSOURCES += tty.c

## ../../../sys
CSOURCES += alloc.c
CSOURCES += bio.c
CSOURCES += conf.c
CSOURCES += fio.c
CSOURCES += iget.c
CSOURCES += pipe.c
CSOURCES += rdwri.c
CSOURCES += sched.c
CSOURCES += sys2.c

## ../../target/windows/
CSOURCES += $(SCHED_CSOURCES)
CSOURCES += stdio_adapt.c
CSOURCES += tty_adapt.c

ASOURCES := $(SCHED_ASOURCES)

## ../../../usr/cmd
CSOURCES += cmd_sample.c

## ../../../reuse
CSOURCES += qbyte.c
CSOURCES += symchar.c

# CPP sources
## reserved for future use

# Make object file lit from source file list
_OBJS = $(CSOURCES:.c=.o)
_OBJS += $(CPPSOURCES:.cpp=.o)
_OBJS += $(ASOURCES:.S=.o)
OBJS = $(addprefix $(OUTDIR)/, $(_OBJS))

# Dependencies
DEPS = $(OBJS:%.o=%.d)

# CFLAGS
CFLAGS = -mno-ms-bitfields
CFLAGS += -g -O0

INCLUDES := -I.
INCLUDES += -I../../include
INCLUDES += -I../../../elib
INCLUDES += -I../../../sys/h
INCLUDES += -I../../../sys/dev/windows
INCLUDES += -I../../target/windows
INCLUDES += -I../../../../target/windows/Adapt

CFLAGS += $(INCLUDES)
CFLAGS += -DUSE_STACKMACH
CFLAGS += -DESH_STDIO_PIPE
CFLAGS += $(SCHED_CPPFLAGS)

# CXXFLAGS
CXXFLAGS := $(CFLAGS)

# LDFLAGS
LDFLAGS := -lwinmm

all: $(OUTDIR) $(TARGET)

-include $(DEPS)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Other rules
$(OUTDIR):
	$(MKDIR) $(OUTDIR)

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -MMD -MP $< -o $@

$(OUTDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -MMD -MP $< -o $@

$(OUTDIR)/%.o: %.S
	$(CC) $(CFLAGS) -c -MMD -MP $< -o $@

.PHONY: clean
clean:
	$(DEL) *.exe
	$(DEL) *.dll
	$(RMDIR) $(OUTDIR)

CPPFLAGS=$(CFLAGS)

.PHONY: check-syntax
check-syntax:
	$(CC) -o nul $(CPPFLAGS) -S $(CHK_SOURCES)
