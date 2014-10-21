// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// HElibInstance attempts to hide some of the boilerplate and in doing so make it easier to create a
// fake HElib environment.

#include "helib-instance.h"

HElibInstance::HElibInstance (long L, bool verbose) 
{
    long m=0;
    long p=2;
    long r=1;
    long c=3;
    long w=64;
    long d=0;
    long security = 128;
    cout << "L=" << L << endl;
    ZZX G;

    if (verbose) cout << "Finding m..." << endl;
    m = FindM(security,L,c,p,d,0,0);

    if (verbose) cout << "Generating context..." << endl;
    FHEcontext context(m, p, r);

    if (verbose) cout << "Building mod-chain..." << endl;
    buildModChain(context, L, c);

    if (verbose) cout << "Generating keys..." << endl;
    _seckey = new FHESecKey(context);
    _pubkey = new FHEPubKey(*_seckey);

    G = context.alMod.getFactorsOverZZ()[0];
    _seckey->GenSecKey(w);
    addSome1DMatrices(*_seckey);

    _ea = new EncryptedArray(context, G);

    size_t nslots = _ea->size();
    if (verbose) cout << "nslots = " << nslots << endl;
}
