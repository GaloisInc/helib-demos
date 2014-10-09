#include "simon-util.h"

using namespace std;

vector<pt_block> strToBlocks (string inp) {
    vector<pt_block> blocks;
    for (int i = 0; i < inp.size(); i += 8) {
        uint32_t x = inp[i]   << 24 | inp[i+1] << 16 | inp[i+2] << 8 | inp[i+3];
        uint32_t y = inp[i+4] << 24 | inp[i+5] << 16 | inp[i+6] << 8 | inp[i+7];
        pt_block b = { x, y };
        blocks.push_back(b);
    }
    return blocks;
}

void addCharBits(char c, vector<long> *v) {
    for (int i = 0; i < 8; i++) {
        v->push_back((c & (1 << i)) >> i);
    }
}

void pad (long padWith, vector<long> &inp, long length) {
    inp.reserve(length);
    for (int i = inp.size(); i < length; i++) inp.push_back(padWith);
}

vector<long> charToBits (char c) {
    vector<long> ret;
    for (int i = 0; i < 8; i++) {
        ret.push_back((c & (1 << i)) >> i);
    }
    return ret;
}

// turns a SIMON key (vector<uint32_t>) into a vector<vector<long>> where each
// long is simply 0 or 1.
vector<vector<long>> keyToVectors (vector<uint32_t> key, long nslots) {
    vector<vector<long>> ret;
    for (int i = 0; i < key.size(); i++) {
        vector<long> k = uint32ToBits(key[i]);
        pad(0, k, nslots);
        ret.push_back(k);
    }
    return ret;
}

vector<long> uint32ToBits (uint32_t n) {
    vector<long> ret;
    for (int i = 0; i < 32; i++) {
        ret.push_back((n & (1 << i)) >> i);
    }
    return ret;
}

char charFromBits(vector<long> inp) {
    char ret;
    for (int i = 0; i < 8; i++) {
        ret |= inp[i] << i;
    }
    return ret;
}

uint32_t vectorTo32 (vector<long> inp) {
    uint32_t ret = 0;
    for (int i = 0; i < inp.size(); i++) {
        ret |= inp[i] << i;
    }
    return ret;
}

vector<vector<long>> strToVectors (string inp) {
    // returns a vector of an even number of vectors with 32 bits of
    // information in them. each long is actually just 0 or 1.
    vector<vector<long>> ret;
    ret.reserve(inp.size()/4);
    for (int i = 0; i < inp.size(); i+=4) {
        vector<long> c;
        addCharBits(inp[i+3], &c);
        addCharBits(inp[i+2], &c);
        addCharBits(inp[i+1], &c);
        addCharBits(inp[i+0], &c);
        ret.push_back(c);
    }
    if (ret.size()%2) {
        vector<long> c;
        ret.push_back(c);
    }
    return ret;
}

string vectorsToStr (vector<vector<long>> inp){
    // takes a vector of vectors where each long is 0 or 1. turns it into a string.
    string ret;
    for (int i = 0; i < inp.size(); i++) {
        vector<long> v0(&inp[i][24], &inp[i][32]);
        vector<long> v1(&inp[i][16], &inp[i][24]);
        vector<long> v2(&inp[i][ 8], &inp[i][16]);
        vector<long> v3(&inp[i][ 0], &inp[i][ 8]);
        ret.push_back(charFromBits(v0));
        ret.push_back(charFromBits(v1));
        ret.push_back(charFromBits(v2));
        ret.push_back(charFromBits(v3));
    }
    return ret;
}

vector<uint32_t> vectorsTo32 (vector<vector<long>> inp) {
    vector<uint32_t> ret;
    for (int i = 0; i < inp.size(); i++) {
        ret.push_back(vectorTo32(inp[i]));
    }
    return ret;
}

void printKey (vector<pt_key32> k) {
    cout << "key = ";
    for (int i = 0; i < k.size(); i++) {
        if (!(i%5)) printf("\n");
        printf("0x%08x ", k[i]);
    }
    cout << endl;
}

void printVector (vector<long> inp) {
    for (int i = 0; i < inp.size(); i++) {
        if (i > 0 && i%4==0) cout << " ";
        cout << inp[i];
    }
    cout << endl;
}

void printVector (vector<vector<long>> inp) {
    for (int i = 0; i < inp.size(); i++) {
        cout << "[ ";
        printVector(inp[i]);
        cout << " ]" << endl;
    }
}

vector<pt_block> vectorsToBlocks (vector<vector<long>> inp) {
    vector<pt_block> res (inp.size()/2);
    for (int i = 0; i < inp.size(); i+=2) {
        pt_block b = { vectorTo32(inp[i]), vectorTo32(inp[i+1]) };
        res.push_back(b);
    }
    return res;
}

vector<vector<long>> transpose (vector<vector<long>> inp) {
    vector<vector<long>> xs;
    for (int i = 0; i < inp[0].size(); i++) {
        vector<long> nextx;
        for (int j = 0; j < inp.size(); j++) {
            nextx.push_back(inp[j][i]);
        }
        xs.push_back(nextx);
    }
    return xs;
}

vector<vector<long>> transpose (vector<long> inp) {
    vector<vector<long>> vec ({ inp });
    return transpose(vec);
}

void timer(bool init) {
    static time_t old_time;
    static time_t new_time;
    if (!init) {
        old_time = new_time;
    }
    new_time = time(NULL);
    if (!init) {
        cout << (new_time - old_time) << "s" << endl;
    }
}
