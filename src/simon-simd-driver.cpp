// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// An implementation of the SIMON block cipher in HElib. Each Ctxt gets packed
// with a single bit, and two vectors of Ctxt represent a SIMON block. It uses
// bit slicing to parallelize SIMON by packing the corresponding bits of blocks
// into the same Ctxt.

#include "simon-simd.h"

long global_nslots;
CTvec* global_maxint;

int main(int argc, char **argv)
{
    string inp = "secrets! very secrets!";
    cout << "inp = \"" << inp << "\"" << endl;
    vector<pt_key32> k = pt_genKey();
    pt_expandKey(k);
    printKey(k);

    // initialize helib
    long m=0, p=2, r=1;
    long L=23;
    long c=3;
    long w=64;
    long d=0;
    long security = 128;
    cout << "L=" << L << endl;
    ZZX G;
    cout << "Finding m..." << endl;
    m = FindM(security,L,c,p,d,0,0);
    cout << "Generating context..." << endl;
    FHEcontext context(m, p, r);
    cout << "Building mod-chain..." << endl;
    buildModChain(context, L, c);
    cout << "Generating keys..." << endl;
    FHESecKey seckey(context);
    const FHEPubKey& pubkey = seckey;
    G = context.alMod.getFactorsOverZZ()[0];
    seckey.GenSecKey(w);
    addSome1DMatrices(seckey);
    EncryptedArray ea(context, G);
    global_nslots = ea.size();
    cout << "nslots = " << global_nslots << endl;

    // set up globals
    CTvec maxint (ea, pubkey, transpose(uint32ToBits(0xFFFFFFFF)));
    global_maxint = &maxint;

    // HEencrypt key
    timer(true);
    cout << "Encrypting SIMON key..." << flush;
    vector<CTvec> encryptedKey = heEncrypt(ea, pubkey, k);
    timer();

    // HEencrypt input
    cout << "Encrypting inp..." << flush;
    heblock ct = heEncrypt(ea, pubkey, inp);
    timer();

    cout << "Running protocol..." << endl;
    for (size_t i = 0; i < T; i++) {
        cout << "Round " << i+1 << "/" << T << "..." << flush;
        encRound(encryptedKey[i], ct);
        timer();

        // check intermediate result for noise
        cout << "decrypting..." << flush;
        vector<pt_block> bs = heblockToBlocks(seckey, ct);
        timer();

        printf("block0    : 0x%08x 0x%08x\n", bs[0].x, bs[0].y);

        vector<pt_block> pt_bs = pt_simonEnc(k, inp, i+1);
        printf("should be : 0x%08x 0x%08x\n", pt_bs[0].x, pt_bs[0].y);

        cout << "decrypted : \"" << pt_simonDec(k, bs, i+1) << "\" " << endl;
    }

    return 0;
}
