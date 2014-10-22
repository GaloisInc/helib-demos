# Copyright (c) 2013-2014 Galois, Inc.
# Distributed under the terms of the GPLv3 license (see LICENSE file)
#
# Author: Brent Carmer

HELIB=HElib
NTL=ntl-6.2.1
CC=clang++
CFLAGS=-std=c++11 -Ideps/$(HELIB)/src -Ideps/$(NTL)/include -g --static -Wall -O3
LFLAGS=

DEPS = deps/$(HELIB)/src/fhe.a deps/$(NTL)/src/ntl.a

HEADS = helib-instance.h helib-stub.h simon-plaintext.h simon-util.h
OBJ   = helib-instance.o helib-stub.o simon-plaintext.o simon-util.o 
EXE   = aes multest simon-simd simon-blocks simon-plaintext
EXES  = $(addsuffix .cpp, $(EXE))

all : deps $(EXE)

$(EXE): $(EXES) $(OBJ)
	$(CC) $(CFLAGS) $< $(DEPS) -o $@ $(LFLAGS) $(OBJ)

.cpp.o: $(HEADS)
	$(CC) $(CFLAGS) -c $<

deps: ntl helib

helib:
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
		cd $(NTL)/include/NTL && \
		patch < ../../../../ntl.vector.h.patch \
	)
	cd deps/$(NTL)/src; ./configure WIZARD=off; make

clean:
	rm aes
	rm multest
	rm simon-simd
	rm simon-blocks
	rm simon-plaintext
	rm *.o
