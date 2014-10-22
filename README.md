HElib Demos - SIMON
===================

This is a demonstration and exploration of the use of the [HElib library for homomorphic
encryption](https://github.com/shaih/HElib). Currently it contains a homomorphic implementation of
the SIMON block cipher (see the [NSA whitepaper](http://eprint.iacr.org/2013/404.pdf)).

simon-block and simon-simd will be able to do the full 44 rounds called for by the specs when HElib
implements bootstrapping. Until then, they are limited to 10 and 32 rounds respectively.

Installation
------------

There are two dependencies - [HElib](https://github.com/shaih/HElib) and
[NTL](https://github.com/shaih/HElib) (required by HElib). The makefile is set up to download and
build them to the **deps** directory automatically.  Simply compile everything with

>    make

Note that you may need to tweak some of the build flags to get it to run on your system.

Description of Demos
--------------------

* multest.cpp - test how many times HElib can homomorphically square 1.

* simon-blocks.cpp - homomorphic version of the SIMON block cipher.

* simon-simd.cpp - homomorphic version of the SIMON block cipher with SIMD optimization.

* simon-plaintext-test.cpp - plaintext version of the SIMON block cipher for benchmarking.

* aes.cpp - homomorphic implementation of AES128

Supporting Files
----------------

* simon-plaintext.{h,cpp} - plaintext version of SIMON for testing

* simon-util.{h,cpp} - data transformation functions

* helib-instance.{h,cpp} - encapsulation of HElib's extensive boilerplate

* helib-stub.{b,cpp} - fake HElib functions for plaintext evaluation and verification

* aes.cry - Cryptol implementation of AES128

* simon.cry - Cryptol implemenation of SIMON64/128

* ntl.vector.h.patch - removes an unnecessary line in NTL that causes many warnings

* makefile-helib - modified HElib makefile to use the locally built NTL

Licence
-------

Copyright (c) 2013-2014 Galois, Inc.

This software is distributed under the terms of the GNU General Public Licence v3. See the LICENCE
file for details.
