CC=g++
NTL=$(PWD)/ntl-6.2.1/src
FHE=$(PWD)/HElib/src
CFLAGS=-I$(FHE)
LFLAGS=-L$(NTL) -lntl

all: aes try

aes: helib enc_aes.cc
	$(CC) $(CFLAGS) enc_aes.cc $(NTL)/ntl.a $(FHE)/fhe.a -o aes $(LFLAGS)

try: helib try.cc
	$(CC) $(CFLAGS) try.cc $(NTL)/ntl.a $(FHE)/fhe.a -o try $(LFLAGS)
	./try

clean:
	rm -f aes
	 
ntl:
	cd ntl-6.2.1/src; make

helib: ntl
	cd HElib/src/; make
