// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// HElibInstance attempts to hide some of the boilerplate and in doing so make it easier to create a
// fake HElib environment.

#ifndef HELIBINSTANCE_H
#define HELIBINSTANCE_H

#include "FHE.h"
#include "EncryptedArray.h"

//class HElibInstance {
    //FHEcontext* _context;
    //FHEPubKey* _pubkey;
    //FHESecKey* _seckey;
    //EncryptedArray* _ea;
//public:
    //HElibInstance (long L=16, bool verbose=true);
    //EncryptedArray& get_ea () { return *_ea; }
    //size_t get_nslots () { return _ea->size(); }
    //FHEPubKey& get_pubkey () { return *_pubkey; }
    //FHESecKey& get_seckey () { return *_seckey; }
    //~HElibInstance () { delete _ea; delete _pubkey; delete _seckey; delete _context; }
//private:
    //HElibInstance (const HElibInstance&);            // no copying allowed
    //HElibInstance& operator= (const HElibInstance&); //
//};

#endif
