
# Default target
TARGET = tstdrv.exe

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
vpath %.c ..
vpath %.c ../../../esh/target/windows/
vpath %.o _output

# C sources

## .
CSOURCES := getopt.c
CSOURCES += tstdrv.c
CSOURCES += stdio_adapt.c

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
INCLUDES += -I../../../elib
INCLUDES += -I../../../esh/target/windows

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

