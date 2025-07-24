
# Default target
TARGET = test_par.exe

# Commands
CC = gcc
ifeq ($(OS),Windows_NT)
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
ifeq ($(OS),Windows_NT)
vpath %.c ../../target/windows/
else
vpath %.c ../../target/cygwin/
endif
vpath %.o _output

# C sources

## .
CSOURCES := test_par.c

## ../..
CSOURCES += code.c
CSOURCES += echo.c
CSOURCES += expr.c
CSOURCES += help.c
CSOURCES += par.c
CSOURCES += run.c
CSOURCES += slex.c
CSOURCES += test.c
CSOURCES += tok.c
CSOURCES += tty.c

## ../../target/windows/
CSOURCES += stdio_adapt.c
CSOURCES += tty_adapt.c

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
OBJS = $(addprefix $(OUTDIR)/, $(_OBJS))

# Dependencies
DEPS = $(OBJS:%.o=%.d)

# CFLAGS
CFLAGS = -mno-ms-bitfields
CFLAGS += -g -O0

INCLUDES := -I.
INCLUDES += -I../../include
INCLUDES += -I../../../elib
INCLUDES += -I../../target/windows
INCLUDES += -I../../../../target/windows/Adapt

CFLAGS += $(INCLUDES)

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

.PHONY: clean
clean:
	$(DEL) *.exe
	$(DEL) *.dll
	$(RMDIR) $(OUTDIR)

CPPFLAGS=$(CFLAGS)

.PHONY: check-syntax
check-syntax:
	$(CC) -o nul $(CPPFLAGS) -S $(CHK_SOURCES)

