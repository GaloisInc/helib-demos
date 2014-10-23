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

CTvec::CTvec
(
    EncryptedArray &inp_ea,
    const FHEPubKey &inp_pubkey,
    vector<vector<long>> inp,
    bool fill
)
{
    ea = &inp_ea;
    pubkey = &inp_pubkey;
    nelems = inp[0].size();
    for (uint32_t i = 0; i < inp.size(); i++) {
        if (fill) {
            pad(inp[i][0]&1, inp[i], global_nslots);
        } else {
            pad(0, inp[i], global_nslots);
        }
        Ctxt c(*pubkey);
        ea->encrypt(c, *pubkey, inp[i]);
        cts.push_back(c);
    }
}

Ctxt CTvec::get (int i) { return cts[i]; }

void CTvec::xorWith (CTvec &other) {
    for (uint32_t i = 0; i < cts.size(); i++) {
        cts[i].addCtxt(other.get(i));
    }
}

void CTvec::andWith (CTvec &other) {
    for (uint32_t i = 0; i < cts.size(); i++) {
        cts[i].multiplyBy(other.get(i));
    }
}

void CTvec::rotateLeft (int n) {
    rotate(cts.begin(), cts.end()-n, cts.end());
}

vector<vector<long>> CTvec::decrypt (const FHESecKey& seckey) {
    vector<vector<long>> res;
    for (uint32_t i = 0; i < cts.size(); i++) {
        vector<long> decrypted (global_nslots);
        ea->decrypt(cts[i], seckey, decrypted);
        vector<long> bits (decrypted.begin(), decrypted.begin() + nelems);
        res.push_back(bits);
    }
    return res;
}

vector<pt_block> preblockToBlocks (pt_preblock b) {
    vector<pt_block> bs;
    for (size_t j = 0; j < b.xs[0].size(); j++) {
        vector<long> xbits;
        vector<long> ybits;
        for (int i = 0; i < 32; i++) {
            xbits.push_back(b.xs[i][j]);
            ybits.push_back(b.ys[i][j]);
        }
        uint32_t x = vectorTo32(xbits);
        uint32_t y = vectorTo32(ybits);
        bs.push_back({ x, y });
    }
    return bs;
}

pt_preblock blocksToPreblock (vector<pt_block> bs) {
    vector<vector<long>> xs;
    vector<vector<long>> ys;
    for (int i = 0; i < 32; i++) {
        vector<long> nextx;
        vector<long> nexty;
        for (size_t j = 0; j < bs.size(); j++) {
            vector<long> x = uint32ToBits(bs[j].x);
            vector<long> y = uint32ToBits(bs[j].y);
            nextx.push_back(x[i]);
            nexty.push_back(y[i]);
        }
        xs.push_back(nextx);
        ys.push_back(nexty);
    }
    return { xs, ys };
}

vector<pt_block> heblockToBlocks (const FHESecKey& k, heblock ct) {
    vector<pt_block> res;
    vector<vector<long>> xs = ct.x.decrypt(k);
    vector<vector<long>> ys = ct.y.decrypt(k);
    return preblockToBlocks({ xs, ys });
}

heblock heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, string s) {
    vector<pt_block> pt = strToBlocks(s);
    pt_preblock b = blocksToPreblock(pt);
    CTvec c0 (ea, pubkey, b.xs);
    CTvec c1 (ea, pubkey, b.ys);
    return { c0, c1 };
}

vector<CTvec> heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, vector<uint32_t> &k) {
    vector<CTvec> encryptedKey;
    for (size_t i = 0; i < k.size(); i++) {
        vector<long> bits = uint32ToBits(k[i]);
        vector<vector<long>> trans = transpose(bits);
        CTvec kct (ea, pubkey, trans, true);
        encryptedKey.push_back(kct);
    }
    return encryptedKey;
}

void encRound(CTvec key, heblock &inp) {
    CTvec tmp = inp.x;
    CTvec x0 = inp.x;
    CTvec x1 = inp.x;
    CTvec x2 = inp.x;
    CTvec y = inp.y;
    x0.rotateLeft(1);
    x1.rotateLeft(8);
    x2.rotateLeft(2);
    x0.andWith(x1);
    y.xorWith(x0);
    y.xorWith(x2);
    y.xorWith(key);
    inp.x = y;
    inp.y = tmp;
}
