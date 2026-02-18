# makefile for lua interpreter

# BIN= $(LUA)/bin
# INC= $(LUA)/include
# LIB= $(LUA)/lib
BIN= ../../bin
INC= ../../include
LIB= ../../lib

CC= gcc
# CFLAGS= -g -Wall -O2 -I$(INC)
CFLAGS= -g -Wall -O2 -fPIC -I$(INC)

OBJS= lua.o
T=$(BIN)/lua

$T: $(OBJS)
	$(CC) -o $@ $(OBJS) -L$(LIB) -llua -llualib -lm

clean:
	rm -f $T $(OBJS)
