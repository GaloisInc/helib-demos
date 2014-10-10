HELIB=HElib
NTL=ntl-6.2.1
CC=clang++
CFLAGS=-std=c++11 -Ideps/$(HELIB)/src -Ideps/$(NTL)/include -g --static
LFLAGS=-L/usr/local/lib

DEPS = deps/$(HELIB)/src/fhe.a deps/$(NTL)/src/ntl.a

HEADS = simon-plaintext.h simon-util.h
OBJ = simon-util.o simon-plaintext.o

all: aes multest simon-simd simon-naive

simon-naive: simon-naive.cpp $(OBJ)
	$(CC) $(CFLAGS) simon-naive.cpp $(DEPS) -o simon-naive $(LFLAGS) $(OBJ)

simon-simd: simon-simd.cpp $(OBJ)
	$(CC) $(CFLAGS) simon-simd.cpp $(DEPS) -o simon-simd $(LFLAGS) $(OBJ)

%.o: %.cpp $(HEADS)
	$(CC) $(CFLAGS) -c $<

aes: enc_aes.cpp
	$(CC) $(CFLAGS) enc_aes.cpp $(DEPS) -o aes $(LFLAGS) $(OBJ)

multest: multest.cpp
	$(CC) $(CFLAGS) multest.cpp $(DEPS) -o multest $(LFLAGS) $(OBJ)

deps: ntl helib

helib:
	mkdir -p deps
	[[ -d deps/$(HELIB) ]] || git clone https://github.com/shaih/HElib.git deps/$(HELIB)
	cp helib-makefile deps/$(HELIB)/src/Makefile
	cd deps/$(HELIB)/src; make

ntl:
	mkdir -p deps
	[[ -d deps/$(NTL) ]] || \
		cd deps && \
		wget http://www.shoup.net/ntl/$(NTL).tar.gz -O $(NTL).tgz && \
		tar xzf $(NTL).tgz && \
		rm -f $(NTL).tgz;
	cd deps/$(NTL)/src; ./configure WIZARD=off; make

clean:
	rm -f aes
	rm -f multest
	rm -f simon-simd
	rm -f simon-naive
	rm *.o
