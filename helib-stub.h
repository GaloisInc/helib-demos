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

struct FHEPubKey {};
struct FHESecKey {};

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
    EncryptedArray (long L);
    size_t size () { return _size; }
    void shift (Ctxt c, long k); 
    void encrypt (Ctxt& ctxt, const FHEPubKey& pKey, const vector<long>& ptxt);
    void decrypt (const Ctxt& ctxt, const FHESecKey& sKey, vector<long>& ptxt);
};

class HElibInstance {
    EncryptedArray* _ea;
    FHEPubKey* _pubkey;
    FHESecKey* _seckey;
public:
    HElibInstance (long L=16, bool verbose=true);
    const EncryptedArray& get_ea () { return *_ea; }
    size_t get_nslots () { return _ea->size(); }
    const FHEPubKey& get_pubkey () { return *_pubkey; }
    const FHESecKey& get_seckey () { return *_seckey; }
    ~HElibInstance () { delete _ea; delete _pubkey; delete _seckey; }
private:
    HElibInstance (const HElibInstance&);            // no copying allowed
    HElibInstance& operator= (const HElibInstance&); //
};

#endif
