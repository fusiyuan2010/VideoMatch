#!/bin/sh

OPT=-O3
DEBUG=-g

CFLAGS=-std=c++11 -Wall -Wno-format -fPIC $(OPT) $(DEBUG) -DNG #-DAP #-DNG #-DREUTERS
CC=g++
LIBS=-ljsoncpp -pthread -lboost_system-mt
LIB_PATH=
INCLUDE_PATH=-I/usr/include/jsoncpp/ -I./

OBJS=Log.o http_server.o main.o RequestProcessor.o VideoDB.o 

all: server

server: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LIB_PATH)
	

%.o: %.cpp
	$(CC) $(INCLUDE_PATH) -c $(CFLAGS) $(INCLUDE_PATH) -o $@ $^

clean:
	rm -f *.o server

rebuild: clean all



