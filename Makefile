HELIB=${HOME}/HElib
NTL=${HOME}/ntl-6.2.1
CC=clang++
CFLAGS=-std=c++11 -I$(HELIB)/src -I$(NTL)/include -g --static
LFLAGS=-L/usr/local/lib

all: aes example simon simon-naive

simon-naive: simon-naive.cc
	$(CC) $(CFLAGS) simon-naive.cc $(HELIB)/src/fhe.a $(NTL)/src/ntl.a -o simon-naive $(LFLAGS)

simon: simon.cc
	$(CC) $(CFLAGS) simon.cc $(HELIB)/src/fhe.a $(NTL)/src/ntl.a -o simon $(LFLAGS)

aes: enc_aes.cc
	$(CC) $(CFLAGS) enc_aes.cc $(HELIB)/src/fhe.a $(NTL)/src/ntl.a -o aes $(LFLAGS)

example: blog_example.cc
	$(CC) $(CFLAGS) blog_example.cc $(HELIB)/src/fhe.a $(NTL)/src/ntl.a -o example $(LFLAGS)

clean:
	rm -f aes
	rm -f example
	rm -f simon
	rm -f simon-naive
