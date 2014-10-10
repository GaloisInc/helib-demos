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
const int m = 4;
const int j = 3;
const int T = 44;             // SIMON specifications call for 44 rounds

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

void pt_expandKey(vector<pt_key32> &k, int nrounds = T);

uint32_t pt_rotateLeft(uint32_t x, int n);

pt_block pt_encRound(pt_key32 k, pt_block inp);

pt_block pt_decRound(pt_key32 k, pt_block inp);

pt_block pt_encBlock(vector<pt_key32> k, pt_block inp, int nrounds = T);

vector<pt_block> strToBlocks (string inp);

vector<pt_block> pt_simonEnc (vector<pt_key32> k, string inp, int nrounds = T);

pt_block pt_decBlock(vector<pt_key32> k, pt_block inp, int nrounds = T);

string pt_simonDec(vector<pt_key32> k, vector<pt_block> c, int nrounds = T);

string blocksToStr (vector<pt_block> bs);

#endif
