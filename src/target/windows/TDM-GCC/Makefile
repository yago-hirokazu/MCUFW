
# Default target
TARGET = main.exe

# Commands
CXX = g++
CC = gcc
DEL = del /q
COPY = copy
REN = ren
MKDIR = md
RMDIR = rd /s /q

# Output directory
OUTDIR = _output

# Search path
vpath %.c ../Src
vpath %.c ../Adapt
vpath %.c ../../../core/usr/cmd
vpath %.c ../../../core/elib
vpath %.c ../../../core/esh
vpath %.c ../../../core/esh/target/windows
vpath %.o _output
vpath %.dll ./DLL

# C source

## ./Src
CSOURCES := main.c

## ./Adapt
CSOURCES += delay_adapt.c
CSOURCES += tick_adapt.c

## ../../../core/usr/cmd
CSOURCES += cmd_sample.c

## ../../../core/elib
CSOURCES += getopt.c
CSOURCES += qbyte.c

## ../../../core/esh
CSOURCES += echo.c
CSOURCES += esh.c
CSOURCES += expr.c
CSOURCES += help.c
CSOURCES += run.c
CSOURCES += test.c
CSOURCES += tty.c

## ../../../core/shell/target/windows
CSOURCES += esh_adapt.c
CSOURCES += stdio_adapt.c
CSOURCES += tty_adapt.c

# CPP sources
## reserved for future use

# Make object file list from source file list
_OBJS = $(CSOURCES:.c=.o)
_OBJS += $(CPPSOURCES:.cpp=.o)
OBJS = $(addprefix $(OUTDIR)/, $(_OBJS))

# Dependencies
DEPS = $(OBJS:%.o=%.d)

# CFLAGS
CFLAGS = -mno-ms-bitfields
CFLAGS += -g -O0

INCLUDES := -I../Inc
INCLUDES += -I../Adapt
INCLUDES += -I../Drivers/adafruit_ft232h/include
INCLUDES += -I../Drivers/adafruit_ft232h/include/windows
INCLUDES += -I../../../core/elib
INCLUDES += -I../../../core/esh/include
INCLUDES += -I../../../core/esh/target/windows

CFLAGS += $(INCLUDES)
# CFLAGS += -DADAFRUIT

# CXXFLAGS
CXXFLAGS := $(CFLAGS)

# LDFLAGS
LDFLAGS := -lwinmm
# LDFLAGS += -lMPSSE
# LDFLAGS += -lMPSSE_i2c

# DLLs
CYGWIN_DLL_SPI = ../Drivers/adafruit_ft232h/lib/windows/i386/libMPSSE.dll
CYGWIN_DLL_I2C = ../Drivers/adafruit_ft232h/lib/windows/i386/libMPSSE_i2c.dll
DLL_SPI = $(subst /,\,$(CYGWIN_DLL_SPI))
DLL_I2C = $(subst /,\,$(CYGWIN_DLL_I2C))

all: $(OUTDIR) $(TARGET)
	$(COPY) $(DLL_SPI) .
	$(COPY) $(DLL_I2C) .

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
