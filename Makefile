# Copyright (c) 2013-2014 Galois, Inc.
# Distributed under the terms of the GPLv3 license (see LICENSE file)
#
# Author: Brent Carmer

HELIB  = HElib
NTL    = ntl-6.2.1
CC     = clang++
CFLAGS = -std=c++11 -g --static -Wall -O3 -ferror-limit=4

HEADS = simon-plaintext.h simon-util.h
OBJ   = simon-plaintext.o simon-util.o 
EXE   = multest simon-simd simon-blocks simon-plaintext

ifeq ($(strip $(STUB)),)
	OBJ += helib-instance.o
	DEPS = deps/$(HELIB)/src/fhe.a deps/$(NTL)/src/ntl.a
	CFLAGS += -Ideps/$(HELIB)/src -Ideps/$(NTL)/include
else
	OBJ += helib-stub.o
	CFLAGS += -DSTUB=1
	DEPS = ""
endif

all: $(EXE)

#aes: aes.cpp $(OBJ) helib
	#$(CC) $(CFLAGS) $< $(OBJ) $(DEPS) -o $@

multest: multest.cpp $(OBJ) helib
	$(CC) $(CFLAGS) $< $(OBJ) $(DEPS) -o $@

simon-simd: simon-simd.cpp $(OBJ) helib
	$(CC) $(CFLAGS) $< $(OBJ) $(DEPS) -o $@

simon-blocks: simon-blocks.cpp $(OBJ) helib
	$(CC) $(CFLAGS) $< $(OBJ) $(DEPS) -o $@

simon-plaintext: simon-plaintext-test.cpp $(OBJ)
	$(CC) $(CFLAGS) $< simon-util.o simon-plaintext.o -o $@

%.o: %.cpp %.h
	$(CC) $(CFLAGS) $< -c

helib: ntl
	@mkdir -p deps
	@[[ -d deps/$(HELIB) ]] || \
	(	git clone https://github.com/shaih/HElib.git deps/$(HELIB) && \
		cp helib-makefile deps/$(HELIB)/src/Makefile \
	)
	@cd deps/$(HELIB)/src; make

ntl:
	@mkdir -p deps
	@[[ -d deps/$(NTL) ]] || \
	(   cd deps && \
		wget http://www.shoup.net/ntl/$(NTL).tar.gz -O $(NTL).tgz && \
		tar xzf $(NTL).tgz && \
		rm -f $(NTL).tgz && \
		cd $(NTL)/src && \
		./configure WIZARD=off && \
		cd ../include/NTL && \
		patch < ../../../../ntl.vector.h.patch \
	)
	@cd deps/$(NTL)/src; make

clean:
	rm -f aes
	rm -f multest
	rm -f simon-simd
	rm -f simon-blocks
	rm -f simon-plaintext
	rm -f *.o
