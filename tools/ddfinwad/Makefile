#
# Linux MAKEFILE
#

PROGRAM=ddfinwad

CC=gcc
CFLAGS=-O3 -Wall
LDFLAGS=-s -Xlinker --warn-common
DEFINES=-DLINUX
LIBS=

OBJS=wad_io.o main.o

all: $(PROGRAM)

clean:
	rm -f $(PROGRAM) $(OBJS) core

PHONY: all clean

#
# Main Source Compilation
# ^^^^^^^^^^^^^^^^^^^^^^^

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

