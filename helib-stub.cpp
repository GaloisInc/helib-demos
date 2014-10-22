// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file creates a fake HElib environment for use with symbolic simulation.

#include "helib-stub.h"

// Ctxt

Ctxt::Ctxt (const FHEPubKey& pubkey) : _vec() {};

Ctxt& Ctxt::operator+= (const Ctxt& rhs) 
{
    return this->addCtxt(rhs);
}

Ctxt& Ctxt::addCtxt (const Ctxt& rhs) 
{
    for (size_t i = 0; i < _vec.size(); i++) {
        _vec[i] ^= rhs._vec[i];
    }
    return *this;
}

Ctxt& Ctxt::operator*= (const Ctxt& rhs) 
{
    return this->multiplyBy(rhs);
}

Ctxt& Ctxt::multiplyBy (const Ctxt& rhs) 
{
    for (size_t i = 0; i < _vec.size(); i++) {
        _vec[i] &= rhs._vec[i];
    }
    return *this;
}

// EncryptedArray

EncryptedArray::EncryptedArray (long L) : _size(L*500) {}

void EncryptedArray::encrypt (Ctxt& ctxt, const FHEPubKey& pKey, const vector<long>& ptxt)
{
    ctxt._vec = ptxt;
    for (size_t i = ptxt.size(); i <= _size; i++) ctxt._vec.push_back(0);
}

void EncryptedArray::decrypt ( const Ctxt& ctxt, const FHESecKey& sKey, vector<long>& ptxt) 
{
    ptxt = ctxt._vec;
}

void EncryptedArray::shift (Ctxt c, long k)
{
    vector<long> shifted (c._vec.begin() + k, c._vec.end());
    vector<long> zeroes;
    for (int i = 0; i < k; i++) zeroes.push_back(0);
    zeroes.insert(zeroes.end(), c._vec.begin() + k, c._vec.end());
    c._vec = zeroes;
}

HElibInstance::HElibInstance (long L, bool verbose) 
{
    cout << "WARNING: USING HELIB STUB" << endl;
    if (verbose) cout << "L=" << L << endl;
    _seckey = new FHESecKey();
    _pubkey = new FHEPubKey();
    _ea     = new EncryptedArray(L);
    size_t nslots = _ea->size();
    if (verbose) cout << "nslots = " << nslots << endl;
}
