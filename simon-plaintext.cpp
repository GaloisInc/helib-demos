#include "simon-plaintext.h"

uint32_t pt_rotateLeft(uint32_t x, int n) {
    return x << n | x >> (32 - n);
}

void pt_expandKey(vector<pt_key32> &k, int nrounds){
    for (int i = k.size(); i < nrounds; i++) {
        pt_key32 tmp;
        tmp = pt_rotateLeft(k[i-1], -3);
        tmp = tmp ^ k[i-3];
        tmp = tmp ^ pt_rotateLeft(tmp, -1);
        tmp = ~k[i-m] ^ tmp ^ z[j][i-m % 62] ^ 3;
        k.push_back(tmp);
    }
}

pt_block pt_encRound(pt_key32 k, pt_block inp) {
    uint32_t x = inp.x;
    uint32_t y = inp.y;
    uint32_t tmp = x;
    x = y ^ (pt_rotateLeft(x,1) & pt_rotateLeft(x,8)) ^ pt_rotateLeft(x,2) ^ k;
    y = tmp;
    return { x, y };
}

pt_block pt_decRound(pt_key32 k, pt_block inp) {
    uint32_t x = inp.x;
    uint32_t y = inp.y;
    x ^= pt_rotateLeft(y,1) & pt_rotateLeft(y,8);
    x ^= pt_rotateLeft(y,2) ^ k;
    return { y, x };
}

pt_block pt_encBlock(vector<pt_key32> k, pt_block inp, int nrounds) {
    pt_block res = inp;
    for (int i = 0; i < nrounds; i++) {
        res = pt_encRound(k[i], res);
    }
    return res;
}

vector<pt_key32> pt_genKey() {
    srand(time(NULL));
    vector<pt_key32> ks;
    for (int i = 0; i < 4; i++) {
        ks.push_back(rand());
    }
    return ks;
}

vector<pt_block> pt_simonEnc (vector<pt_key32> k, string inp, int nrounds) {
    vector<pt_block> blocks = strToBlocks(inp);
    for (int i = 0; i < blocks.size(); i++) {
        pt_block b = blocks[i];
        blocks[i] = pt_encBlock(k, b, nrounds);
    }
    return blocks;
}

pt_block pt_decBlock(vector<pt_key32> k, pt_block inp, int nrounds) {
    pt_block res = inp;
    for (int i = nrounds-1; i >= 0; i--) {
        res = pt_decRound(k[i], res);
    }
    return res;
}

string pt_simonDec(vector<pt_key32> k, vector<pt_block> c, int nrounds) {
    vector<pt_block> bs;
    for (int i = 0; i < c.size(); i++) {
        pt_block b = pt_decBlock(k, c[i], nrounds);
        bs.push_back(b);
    }
    return blocksToStr(bs);
}

string blocksToStr (vector<pt_block> bs) {
    string ret;
    for (int i = 0; i < bs.size(); i++) {
        pt_block b = bs[i];
        ret.push_back(b.x >> 24);
        ret.push_back(b.x >> 16 & 255);
        ret.push_back(b.x >> 8  & 255);
        ret.push_back(b.x & 255);
        ret.push_back(b.y >> 24);
        ret.push_back(b.y >> 16 & 255);
        ret.push_back(b.y >> 8  & 255);
        ret.push_back(b.y & 255);
    }
    return ret;
}

