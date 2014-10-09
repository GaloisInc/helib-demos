HELIB=${HOME}/HElib
NTL=${HOME}/ntl-6.2.1
CC=clang++
CFLAGS=-std=c++11 -I$(HELIB)/src -I$(NTL)/include -g --static
LFLAGS=-L/usr/local/lib

DEPS = $(HELIB)/src/fhe.a $(NTL)/src/ntl.a

HEADS = simon-plaintext.h simon-util.h
OBJ = simon-util.o simon-plaintext.o

#all: aes shifttest multest simon-simd simon-naive
all: simon-simd simon-naive

simon-naive: simon-naive.cpp $(OBJ)
	$(CC) $(CFLAGS) simon-naive.cpp $(DEPS) -o simon-naive $(LFLAGS) $(OBJ)

simon-simd: simon-simd.cpp $(OBJ)
	$(CC) $(CFLAGS) simon-simd.cpp $(DEPS) -o simon-simd $(LFLAGS) $(OBJ)

%.o: %.cpp $(HEADS)
	$(CC) $(CFLAGS) -c $<

#simon-naive: simon-naive.cpp
	#$(CC) $(CFLAGS) simon-naive.cpp $(DEPS) -o simon-naive $(LFLAGS)

#simon-simd: simon-simd.cpp
	#$(CC) $(CFLAGS) simon-simd.cpp $(DEPS) -o simon-simd $(LFLAGS)

#helib-wrapper: simon-util helib-wrapper.cpp

#simon-util: simon-util.cpp
	#$(CC) $(CFLAGS) simon-simd.cpp $(DEPS) -o simon-simd $(LFLAGS)

#aes: enc_aes.cpp
	#$(CC) $(CFLAGS) enc_aes.cpp $(DEPS) -o aes $(LFLAGS)

#shifttest: shifttest.cpp
	#$(CC) $(CFLAGS) shifttest.cpp $(DEPS) -o shifttest $(LFLAGS)

#multest: multest.cpp
	#$(CC) $(CFLAGS) multest.cpp $(DEPS) -o multest $(LFLAGS)

clean:
	rm -f aes
	rm -f multest
	rm -f shifttest
	rm -f simon-simd
	rm -f simon-naive
	rm *.o
