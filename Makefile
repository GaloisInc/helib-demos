# Copyright (c) 2013-2014 Galois, Inc.
# Distributed under the terms of the GPLv3 license (see LICENSE file)
#
# Author: Brent Carmer

HELIB  = HElib
NTL    = ntl-6.2.1
CC     = clang++
CFLAGS = -std=c++11 -g --static -Wall -ferror-limit=2
SRCDIR = src
BLDDIR = build

OBJ   = $(BLDDIR)/simon-plaintext.o $(BLDDIR)/simon-util.o 
EXE   = multest simon-simd simon-blocks simon-plaintext

ifeq ($(strip $(STUB)),)
	DEPS    = deps/$(HELIB)/src/fhe.a deps/$(NTL)/src/ntl.a
	CFLAGS += -Ideps/$(HELIB)/src -Ideps/$(NTL)/include
else
	OBJ    += $(BLDDIR)/helib-stub.o
	CFLAGS += -DSTUB
endif

all: $(EXE)

multest: $(SRCDIR)/multest.cpp $(OBJ) helib
	$(CC) $(CFLAGS) $< $(OBJ) $(DEPS) -o $@

simon-simd: $(SRCDIR)/simon-simd-driver.cpp $(BLDDIR)/simon-simd.o $(OBJ) helib
	$(CC) $(CFLAGS) $< $(BLDDIR)/$@.o $(OBJ) $(DEPS) -o $@

simon-blocks: $(SRCDIR)/simon-blocks-driver.cpp $(BLDDIR)/simon-blocks.o $(OBJ) helib
	$(CC) $(CFLAGS) $< $(BLDDIR)/$@.o $(OBJ) $(DEPS) -o $@

simon-plaintext: $(SRCDIR)/simon-plaintext-driver.cpp $(OBJ)
	$(CC) $(CFLAGS) $< $(BLDDIR)/simon-util.o $(BLDDIR)/$@.o -o $@

$(BLDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BLDDIR)
	$(CC) $(CFLAGS) $< -c -o $@

$(BLDDIR)/%.bc: $(SRCDIR)/%.cpp
	clang++ -DSTUB -std=c++11 -emit-llvm -c $< -o $@

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
	rm -f $(BLDDIR)/*.o
	rm -f $(BLDDIR)/*.bc
