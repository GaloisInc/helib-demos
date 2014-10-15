// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file includes functions implementing the SIMON block cipher in
// plaintext (without using homomorphic encryption).

#ifndef SIMONPT_H
#define SIMONPT_H

#include <stdint.h>
#include <vector>
#include <string>

using namespace std;

typedef uint32_t pt_key32;

struct pt_block {
    uint32_t x;
    uint32_t y;
};

// SIMON parameters
const size_t m = 4;
const size_t j = 3;
const size_t T = 44;             // SIMON specifications call for 44 rounds

const vector<vector<uint32_t>> z (
    { { 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0,
        0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1,
        0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0 },
    { 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0,
        0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1,
        0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0 },
    { 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1,
        0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1 },
    { 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,
        0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0,
        0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1 },
    { 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1,
        1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1,
        0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1 } }
    );

vector<pt_key32> pt_genKey();

void pt_expandKey(vector<pt_key32> &k, size_t nrounds = T);

uint32_t pt_rotateLeft(uint32_t x, size_t n);

pt_block pt_encRound(pt_key32 k, pt_block inp);

pt_block pt_decRound(pt_key32 k, pt_block inp);

pt_block pt_encBlock(vector<pt_key32> k, pt_block inp, size_t nrounds = T);

vector<pt_block> strToBlocks (string inp);

vector<pt_block> pt_simonEnc (vector<pt_key32> k, string inp, size_t nrounds = T);

pt_block pt_decBlock(vector<pt_key32> k, pt_block inp, size_t nrounds = T);

string pt_simonDec(vector<pt_key32> k, vector<pt_block> c, size_t nrounds = T);

string blocksToStr (vector<pt_block> bs);

#endif
