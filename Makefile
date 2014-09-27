HELIB=${HOME}/HElib
CC=g++
CFLAGS=-std=c++11 -I$(HELIB)/src -g
LFLAGS=-L/usr/local/lib -L/usr/local/include/NTL -lntl 

all: aes example simon

simon: simon.cc
	$(CC) $(CFLAGS) simon.cc $(HELIB)/src/fhe.a -o simon $(LFLAGS)

aes: enc_aes.cc
	$(CC) $(CFLAGS) enc_aes.cc $(HELIB)/src/fhe.a -o aes $(LFLAGS)

example: blog_example.cc
	$(CC) $(CFLAGS) blog_example.cc $(HELIB)/src/fhe.a -o example $(LFLAGS)

clean:
	rm -f aes
	rm -f example
	rm -f simon
