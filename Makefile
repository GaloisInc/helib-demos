# Copyright (c) 2013-2014 Galois, Inc.
# Distributed under the terms of the GPLv3 license (see LICENSE file)
#
# Author: Brent Carmer

HELIB  = HElib
NTL    = ntl-7.0.0
CC     = g++
CFLAGS = -std=c++11 -g --static -Wall
SRCDIR = src
BLDDIR = build

OBJ    = $(BLDDIR)/simon-pt.o $(BLDDIR)/simon-util.o 
BC     = $(BLDDIR)/simon-pt.bc $(BLDDIR)/simon-util.bc
BLOCKSBC = $(BLDDIR)/simon-blocks.bc $(BLDDIR)/simon-blocks-c-interface.bc \
		   $(BLDDIR)/helib-stub.bc $(BC)
SIMDBC   = $(BLDDIR)/simon-simd.bc $(BLDDIR)/simon-simd-c-interface.bc \
		   $(BLDDIR)/helib-stub.bc $(BC)
EXE    = multest simon-simd simon-blocks simon-pt

ifeq ($(strip $(STUB)),)
	DEPS    = deps/$(HELIB)/src/fhe.a deps/$(NTL)/src/ntl.a
	CFLAGS += -Ideps/$(HELIB)/src -Ideps/$(NTL)/include
else
	OBJ    += $(BLDDIR)/helib-stub.o
	CFLAGS += -DSTUB
endif

all: $(EXE)

simon-simd: $(SRCDIR)/simon-simd-driver.cpp $(BLDDIR)/simon-simd.o $(OBJ) helib
	$(CC) $(CFLAGS) $(LFLAGS) $< $(BLDDIR)/$@.o $(OBJ) $(DEPS) -o $@

simon-blocks: $(SRCDIR)/simon-blocks-driver.cpp $(BLDDIR)/simon-blocks.o $(OBJ) helib
	$(CC) $(CFLAGS) $(LFLAGS) $< $(BLDDIR)/$@.o $(OBJ) $(DEPS) -o $@

simon-pt: $(SRCDIR)/simon-pt-driver.cpp $(OBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $< $(BLDDIR)/simon-util.o $(BLDDIR)/$@.o -o $@

bitcode: pt-bitcode blocks-bitcode simd-bitcode

pt-bitcode: $(BLDDIR)/simon-pt-c-interface.bc $(BC)
	llvm-link -o simon-pt.bc $(BLDDIR)/simon-pt-c-interface.bc $(BC)

blocks-bitcode: $(BLOCKSBC)
	llvm-link -o simon-blocks.bc $(BLOCKSBC)

simd-bitcode: $(SIMDBC)
	llvm-link -o simon-simd.bc $(SIMDBC)

multest: $(SRCDIR)/multest.cpp $(OBJ) helib
	$(CC) $(CFLAGS) $(LFLAGS) $< $(OBJ) $(DEPS) -o $@

$(BLDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BLDDIR)
	$(CC) $(CFLAGS) $(LFLAGS) $< -c -o $@

$(BLDDIR)/%.bc: $(SRCDIR)/%.cpp
	@mkdir -p $(BLDDIR)
	clang++ $(CFLAGS) -DSTUB -emit-llvm -c $< -o $@

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
		cd ../include/NTL
	)
	@cd deps/$(NTL)/src; make

clean:
	rm -f aes
	rm -f multest
	rm -f simon-simd
	rm -f simon-blocks
	rm -f simon-pt
	rm -f $(BLDDIR)/*.o
	rm -f *.bc
	rm -f $(BLDDIR)/*.bc
