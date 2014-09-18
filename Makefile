CC=g++
NTL=$(PWD)/ntl-6.2.1/src
FHE=$(PWD)/HElib/src
CFLAGS=-I$(FHE)
LFLAGS=-L$(NTL) -lntl

all: aes

aes: enc_aes.cc ntl helib
	$(CC) $(CFLAGS) enc_aes.cc $(NTL)/ntl.a $(FHE)/fhe.a -o aes $(LFLAGS)

clean:
	rm -f aes

ntl:
	cd $(NTL); make

helib:
	cd $(FHE); make
