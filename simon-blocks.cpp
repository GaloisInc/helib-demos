// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// An implementation of the SIMON block cipher in HElib. Each Ctxt gets packed
// with 32 bits, representing half of a SIMON block.

#ifdef STUB
#include "helib-stub.h"
#else
#include "helib-instance.h"
#endif

#include "simon-plaintext.h"
#include "simon-util.h"

EncryptedArray* global_ea;
Ctxt* global_maxint;
size_t global_nslots;

struct heblock {
    Ctxt x;
    Ctxt y;
};

void negate32(Ctxt &x) {
    x += *global_maxint;
}

// this algorithm for rotation is verified in Cryptol in rotation.cry
void rotateLeft32(Ctxt &x, int n) {
    Ctxt other = x;
    global_ea->shift(x, n);
    global_ea->shift(other, -(32-n));
    negate32(x);                        // bitwise OR
    negate32(other);
    x.multiplyBy(other);
    negate32(x);
}

void encRound(Ctxt &key, heblock& inp) {
    Ctxt tmp = inp.x;
    Ctxt x0  = inp.x;
    Ctxt x1  = inp.x;
    Ctxt x2  = inp.x;
    Ctxt y   = inp.y;
    rotateLeft32(x0, 1);
    rotateLeft32(x1, 8);
    rotateLeft32(x2, 2);
    x0.multiplyBy(x1);
    y    += x0;
    y    += x2;
    y    += key;
    inp.x = y;
    inp.y = tmp;
}

Ctxt heEncrypt(const FHEPubKey& k, uint32_t x) {
    vector<long> vec = uint32ToBits(x);
    pad(0, vec, global_nslots);
    Ctxt c(k);
    global_ea->encrypt(c, k, vec);
    return c;
}

uint32_t heDecrypt (const FHESecKey& k, Ctxt &c) {
    vector<long> vec;
    global_ea->decrypt(c, k, vec);
    return vectorTo32(vec);
}

vector<heblock> heEncrypt (const FHEPubKey& k, string s) {
    vector<vector<long>> pt = strToVectors(s);
    vector<Ctxt> cts;
    vector<heblock> blocks;
    for (size_t i = 0; i < pt.size(); i++) {
        pad(0, pt[i], global_nslots);
        Ctxt c(k);
        global_ea->encrypt(c, k, pt[i]);
        cts.push_back(c);
    }
    for (size_t i = 0; i < cts.size()-1; i++) {
        blocks.push_back({ cts[i], cts[i+1] });
    }
    return blocks;
}

vector<Ctxt> heEncrypt (const FHEPubKey& pubkey, vector<uint32_t> k) {
    vector<vector<long>> kbits = keyToVectors(k, global_nslots);
    vector<Ctxt> encryptedKey;
    for (size_t i = 0; i < kbits.size(); i++) {
        pad(0, kbits[i], global_nslots);
        Ctxt kct(pubkey);
        global_ea->encrypt(kct, pubkey, kbits[i]);
        encryptedKey.push_back(kct);
    }
    return encryptedKey;
}

vector<vector<long>> heDecrypt (const FHESecKey& k, vector<Ctxt> cts) {
    vector<vector<long>> res (cts.size());
    for (size_t i = 0; i < cts.size(); i++) {
        global_ea->decrypt(cts[i], k, res[i]);
    }
    return res;
}

pt_block heDecrypt (const FHESecKey& k, heblock b) {
    return { heDecrypt(k,b.x), heDecrypt(k,b.y) };
}


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
    
    HElibInstance inst(70);
    EncryptedArray ea = inst.get_ea();
    FHEPubKey pubkey = inst.get_pubkey();
    FHESecKey seckey = inst.get_seckey();

    // set up globals
    global_nslots = inst.get_nslots();
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
