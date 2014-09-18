CC=g++
NTL=$(PWD)/ntl-6.2.1
FHE=$(PWD)/HElib/src
CFLAGS=-I$(FHE) -I$(NTL)/include
LFLAGS=-L/usr/local/lib -L$(NTL)/src

all: aes try

aes: enc_aes.cc
	$(CC) $(CFLAGS) enc_aes.cc $(FHE)/fhe.a $(NTL)/src/ntl.a -o aes $(LFLAGS)

try: try.cc
	$(CC) $(CFLAGS) try.cc $(FHE)/fhe.a $(NTL)/src/ntl.a -o try $(LFLAGS)

clean:
	rm -f aes

ntl:
	cd ntl-6.2.1/src; ./configure; make

helib: ntl
	cd HElib/src/; make
