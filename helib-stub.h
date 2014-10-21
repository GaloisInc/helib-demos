// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file creates a fake HElib environment for use with symbolic simulation.

#ifndef HELIBINSTANCE
#define HELIBINSTANCE

class HElibInstance {
public:
    HElibInstance (long L=16, bool verbose=true);
    const EncryptedArray& get_ea () { return *_ea; }
    size_t get_nslots () { return _ea->size(); }
    const FHEPubKey& get_pubkey () { return *_pubkey; }
    const FHESecKey& get_seckey () { return *_seckey; }
    ~HElibInstance () { delete _ea; delete _pubkey; delete _seckey; }
private:
    EncryptedArray* _ea;
    FHEPubKey* _pubkey;
    FHESecKey* _seckey;
    HElibInstance(const HElibInstance&);            // no copying allowed
    HElibInstance& operator=(const HElibInstance&); //
};

#endif
