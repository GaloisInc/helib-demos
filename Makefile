CWD=$(shell pwd)
CC=g++
CFLAGS=-I$(CWD)/HElib/src -I$(CWD)/ntl-6.2.1/include
LFLAGS=-L/usr/local/lib -L$(CWD)/ntl-6.2.1/src -lntl

all: aes

aes: enc_aes.cc
	$(CC) $(CFLAGS) enc_aes.cc $(CWD)/HElib/src/fhe.a $(CWD)/ntl-6.2.1/src/ntl.a -o aes $(LFLAGS)

clean:
	rm -f aes
