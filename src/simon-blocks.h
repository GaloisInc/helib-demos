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
#include "FHE.h"
#include "EncryptedArray.h"
#endif

#include "simon-pt.h"
#include "simon-util.h"

extern EncryptedArray* global_ea;
extern Ctxt* global_maxint;
extern size_t global_nslots;

struct heblock {
    Ctxt x;
    Ctxt y;
};

void negate32(Ctxt &x);

void rotateLeft32(Ctxt &x, int n);

void encRound(Ctxt &key, heblock& inp);

Ctxt heEncrypt(const FHEPubKey& k, uint32_t x);

uint32_t heDecrypt (const FHESecKey& k, Ctxt &c);

vector<heblock> heEncrypt (const FHEPubKey& k, string s);

vector<Ctxt> heEncrypt (const FHEPubKey& pubkey, vector<uint32_t> k);

vector<vector<long>> heDecrypt (const FHESecKey& k, vector<Ctxt> cts);

pt_block heDecrypt (const FHESecKey& k, heblock b);
