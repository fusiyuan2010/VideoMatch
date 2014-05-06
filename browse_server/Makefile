#!/bin/sh

OPT=-O3
DEBUG=-g

CFLAGS=-std=c++11 -Wall -Wno-format -fPIC $(OPT) $(DEBUG)
CC=g++
LIBS=-pthread -lboost_system-mt
LIB_PATH=
INCLUDE_PATH=-I./

OBJS=http_server.o main.o 

all: browse_server

browse_server: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LIB_PATH)
	

%.o: %.cpp
	$(CC) $(INCLUDE_PATH) -c $(CFLAGS) $(INCLUDE_PATH) -o $@ $^

clean:
	rm -f *.o browse_server

rebuild: clean all



