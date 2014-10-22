// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file homomorphically squares 1 a number of times in an attempt to
// discover the noise limit in HElib.

#include <ctime>
#ifdef STUB
#include "helib-stub.h"
#else
#include "helib-instance.h"
#endif
#include "simon-util.h"

EncryptedArray* global_ea;
long global_nslots;

Ctxt heEncrypt(const FHEPubKey& k, uint32_t x) {
    vector<long> vec = uint32ToBits(x);
    pad(0, vec, global_nslots);
    Ctxt c(k);
    global_ea->encrypt(c, k, vec);
    return c;
}

vector<long> heDecrypt (const FHESecKey& k, Ctxt c) {
    vector<long> vec;
    global_ea->decrypt(c, k, vec);
    return vec;
}

int main(int argc, char **argv) {
    HElibInstance inst(16);
    EncryptedArray ea = inst.get_ea();
    FHEPubKey pubkey = inst.get_pubkey();
    FHESecKey seckey = inst.get_seckey();

    // set up globals
    global_nslots = inst.get_nslots();
    global_ea     = &ea;

    // how many multiplications can we do without noise?
    Ctxt one = heEncrypt(pubkey, 1);
    for (int i = 1; i <= 50; i++) {
        cout << "mul#" << i << "..." << flush;
        ///////////
        one.multiplyBy(one);
        ///////////
        vector<long> res = heDecrypt(seckey, one);
        for (int j = 32; j >= 0; j--) {
            if (j > 0 && j%4==0) cout << " ";
            cout << res[j];
        }
        cout << endl;
    }
    return 0;
}
