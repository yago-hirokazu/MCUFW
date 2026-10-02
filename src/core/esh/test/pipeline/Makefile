TARGET = test_pipeline.exe
OUTDIR = _output
CC = gcc
CXX = g++

CFLAGS = -mno-ms-bitfields -g
CFLAGS += -Wall -Wextra
CFLAGS += -I. -I../../include -I../../../elib
CFLAGS += -I../../target/windows -I../../../../target/windows/Adapt
CFLAGS += -DUSE_STACKMACH

LDFLAGS = -lwinmm

CSOURCES = test_pipeline.c
CSOURCES += break.c code.c echo.c esh_debug.c expr.c help.c
CSOURCES += par.c run.c lex.c test.c tok.c tty.c
CSOURCES += stdio_adapt.c tty_adapt.c cmd_sample.c qbyte.c symchar.c

OBJS = $(addprefix $(OUTDIR)/,$(CSOURCES:.c=.o))

vpath %.c . ../.. ../../../usr/cmd ../../../elib ../../target/windows

.PHONY: all clean

all: $(OUTDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -c -MMD -MP $< -o $@

-include $(OBJS:.o=.d)

clean:
	rm -rf $(OUTDIR) $(TARGET)
