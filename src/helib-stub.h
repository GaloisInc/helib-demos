// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file creates a fake HElib environment for use with symbolic simulation.

#ifndef HELIBSTUB_H
#define HELIBSTUB_H

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std;

struct ZZX {};

class PAlgebraMod {
public:
    vector<ZZX> getFactorsOverZZ () { return { ZZX() }; }
};

class FHEcontext {
public:
    PAlgebraMod alMod;
    FHEcontext (long m, long p, long r) {}
};

class FHESecKey {
public:
    FHESecKey () {}
    FHESecKey (const FHEcontext& context) {}
    void GenSecKey (long w) {}
};

typedef FHESecKey FHEPubKey;

class Ctxt {
public:
    Ctxt (const FHEPubKey& k);
    Ctxt& operator+= (const Ctxt& rhs);
    Ctxt& operator*= (const Ctxt& rhs);
    Ctxt& addCtxt (const Ctxt& rhs);
    Ctxt& multiplyBy (const Ctxt& rhs);
    friend class EncryptedArray;
private:
    std::vector<long> _vec;
};

class EncryptedArray {
    size_t _size;
public:
    EncryptedArray (const FHEcontext& context, const ZZX& G) : _size(500) {}
    size_t size () { return _size; }
    void shift (Ctxt& c, long k); 
    void encrypt (Ctxt& ctxt, const FHEPubKey& pKey, const vector<long>& ptxt);
    void decrypt (const Ctxt& ctxt, const FHESecKey& sKey, vector<long>& ptxt);
};

long FindM (long k, long L, long c, long p, long d, long s, long chosen_m, bool verbose=false);

void buildModChain (FHEcontext &context, long nPrimes, long c=3);

void addSome1DMatrices(FHESecKey& sKey, long bound = 100, long keyID = 0);

#endif
