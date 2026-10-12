OUTDIR ?= _output
CC = gcc
CXX = g++
OPT ?= -O0

CPPFLAGS = -I../../h
CFLAGS = -mno-ms-bitfields -g $(OPT) -std=c11
CFLAGS += -Wall -Wextra -Werror -pedantic
CXXFLAGS = -mno-ms-bitfields -g $(OPT) -std=c++11
CXXFLAGS += -Wall -Wextra -Werror -pedantic

OBJS = $(OUTDIR)/headers_c.o
OBJS += $(OUTDIR)/headers_c_reverse.o
OBJS += $(OUTDIR)/headers_cpp.o
OBJS += $(OUTDIR)/headers_cpp_reverse.o

.PHONY: all clean

all: $(OUTDIR) $(OBJS)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/headers_c.o: test_headers.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OUTDIR)/headers_c_reverse.o: test_headers.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTEST_REVERSE -c $< -o $@

$(OUTDIR)/headers_cpp.o: test_headers.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ -c $< -o $@

$(OUTDIR)/headers_cpp_reverse.o: test_headers.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DTEST_REVERSE -x c++ -c $< -o $@

clean:
	rm -rf $(OUTDIR)
