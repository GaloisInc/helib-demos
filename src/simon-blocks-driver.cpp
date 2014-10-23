// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// An implementation of the SIMON block cipher in HElib. Each Ctxt gets packed
// with 32 bits, representing half of a SIMON block.

#include "simon-blocks.h"

EncryptedArray* global_ea;
Ctxt* global_maxint;
size_t global_nslots;

int main(int argc, char **argv)
{
    string inp = "secrets!";
    cout << "inp = \"" << inp << "\"" << endl;
    vector<pt_block> bs = strToBlocks(inp);
    printf("as block : 0x%08x 0x%08x\n", bs[0].x, bs[0].y);
    //key k = genKey();
    vector<pt_key32> k ({0x1b1a1918, 0x13121110, 0x0b0a0908, 0x03020100});
    pt_expandKey(k);
    printKey(k);

    // initialize helib
    long m=0, p=2, r=1;
    long L=70;
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
    global_ea     = &ea;
    Ctxt maxint   = heEncrypt(pubkey, 0xFFFFFFFF);
    global_maxint = &maxint;

    cout << "Encrypting SIMON key..." << flush;
    timer(true);
    vector<Ctxt> encryptedKey = heEncrypt(pubkey, k);
    timer();

    cout << "Encrypting inp..." << flush;
    vector<heblock> cts = heEncrypt(pubkey, inp);
    timer();

    cout << "Running protocol..." << endl;
    heblock b = cts[0];
    for (size_t i = 0; i < T; i++) {
        timer(true);
        cout << "Round " << i+1 << "/" << T << "..." << flush;
        encRound(encryptedKey[i], b);
        timer();

        // check intermediate result for noise
        cout << "decrypting..." << flush;
        pt_block res = heDecrypt(seckey, b);
        timer();

        printf("result    : 0x%08x 0x%08x\n", res.x, res.y);

        vector<pt_block> bs = strToBlocks(inp);
        for (size_t j = 0; j <= i; j++) {
            bs[0] = pt_encRound(k[j], bs[0]);
        }
        printf("should be : 0x%08x 0x%08x\n", bs[0].x, bs[0].y);
        cout << "decrypted : \"" << pt_simonDec(k, {res}, i+1) << "\" " << endl;
    }
    return 0;
}
