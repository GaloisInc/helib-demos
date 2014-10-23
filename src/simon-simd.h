// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// An implementation of the SIMON block cipher in HElib. Each Ctxt gets packed
// with a single bit, and two vectors of Ctxt represent a SIMON block. It uses
// bit slicing to parallelize SIMON by packing the corresponding bits of blocks
// into the same Ctxt.

#include <algorithm>

#ifdef STUB
#include "helib-stub.h"
#else
#include "FHE.h"
#include "EncryptedArray.h"
#endif

#include "simon-pt.h"
#include "simon-util.h"

class CTvec {
    vector<Ctxt> cts;
    EncryptedArray* ea;
    const FHEPubKey* pubkey;
    int nelems;
public:
    CTvec (
      EncryptedArray &inp_ea,
      const FHEPubKey &inp_pubkey,
      vector<vector<long>> inp,
      bool fill = false
    );
    Ctxt get (int i);
    void xorWith (CTvec &other);
    void andWith (CTvec &other);
    void rotateLeft (int n);
    vector<vector<long>> decrypt (const FHESecKey& seckey);
};

extern size_t global_nslots;
extern CTvec* global_maxint;

struct pt_preblock {
    vector<vector<long>> xs;
    vector<vector<long>> ys;
};

struct heblock {
    CTvec x;
    CTvec y;
};

vector<pt_block> preblockToBlocks (pt_preblock b);

pt_preblock blocksToPreblock (vector<pt_block> bs);

vector<pt_block> heblockToBlocks (const FHESecKey& k, heblock ct);

heblock heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, string s);

vector<CTvec> heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, vector<uint32_t> &k);

void encRound(CTvec key, heblock &inp);
