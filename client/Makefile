#!/bin/sh

OPT=-O3
DEBUG=-g

CFLAGS=-std=c++11 -Wall -Wno-format -fPIC $(OPT) $(DEBUG) 
CC=g++
LIBS=-ljsoncpp -lcurl -lX11 -ljpeg -lpthread
LIB_PATH=
INCLUDE_PATH=-I/usr/include/jsoncpp/ -I./

OBJS=ImageProcessor.o main.o Requester.o 

all: client

client: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LIB_PATH)
	

%.o: %.cpp
	$(CC) $(INCLUDE_PATH) -c $(CFLAGS) $(INCLUDE_PATH) -o $@ $^

clean:
	rm -f *.o client

image_test: ImageProcessor.cpp
	$(CC) $(INCLUDE_PATH) $(CFLAGS) -DIMAGETEST -o $@ $^  $(LIBS) $(LIB_PATH) 


rebuild: clean all



