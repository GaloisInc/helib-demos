CC=g++
NTL=$(PWD)/ntl-6.2.1
FHE=$(PWD)/HElib/src
CFLAGS=-std=c++11 -I$(FHE) -I$(NTL)/include -g
LFLAGS=-L/usr/local/lib -L$(NTL)/src

all: aes example simon

simon: enc_aes.cc
	$(CC) $(CFLAGS) simon.cc $(FHE)/fhe.a $(NTL)/src/ntl.a -o simon $(LFLAGS)

aes: enc_aes.cc
	$(CC) $(CFLAGS) enc_aes.cc $(FHE)/fhe.a $(NTL)/src/ntl.a -o aes $(LFLAGS)

example: blog_example.cc
	$(CC) $(CFLAGS) blog_example.cc $(FHE)/fhe.a $(NTL)/src/ntl.a -o example $(LFLAGS)

clean:
	rm -f aes
	rm -f example
	rm -f simon

ntl:
	cd ntl-6.2.1/src; ./configure; make

helib:
	cd HElib/src/; make
