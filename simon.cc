#include "FHE.h"
#include <string>
#include <stdio.h>
#include <ctime>
#include "EncryptedArray.h"
#include "permutations.h"
#include <NTL/lzz_pXFactoring.h>
#include <fstream>
#include <sstream>
#include <sys/time.h>

#include <math.h>
#include <stdint.h>
#include <bitset>
#include <tuple>
#include <stdexcept>
#include <algorithm>

std::bitset<62> z0(0b11111010001001010110000111001101111101000100101011000011100110);
std::bitset<62> z1(0b10001110111110010011000010110101000111011111001001100001011010);
std::bitset<62> z2(0b10101111011100000011010010011000101000010001111110010110110011);
std::bitset<62> z3(0b11011011101011000110010111100000010010001010011100110100001111);
std::bitset<62> z4(0b11010001111001101011011000100000010111000011001010010011101111);
bitset<62> z[5] = { z0, z1, z2, z3, z4 };

const int m = 4;
const int j = 3;
const int T = 44; // SIMON specifications call for 44 rounds
//const int T = 1;

typedef uint32_t key32;
typedef vector<key32> key;

struct block {
    uint32_t x;
    uint32_t y;
};

class ctvec;

long   global_nslots;
ctvec* global_maxint;
ctvec* global_one;
ctvec* global_zero;

Ctxt heEncrypt(EncryptedArray&, const FHEPubKey&, uint32_t);

class ctvec {
    vector<Ctxt> vec;
public:
    ctvec (EncryptedArray &ea, const FHEPubKey &pubkey, vector<long> inp) {
        for (int i = 0; i < 32; i++) {
            Ctxt c = heEncrypt(ea, pubkey, inp[i]);
            vec.push_back(c);
        }
    }
    Ctxt get (int i) {
        return vec[i];
    }
    void xorWith (ctvec &other) {
        for (int i = 0; i < vec.size(); i++) {
            vec[i] += other.get(i);
        }
    }
    void andWith (ctvec &other) {
        for (int i = 0; i < vec.size(); i++) {
            vec[i] *= other.get(i);
        }
    }
    void rotateLeft (int n) {
        rotate(vec.begin(), vec.end()-n, vec.end());
    }
    vector<long> decrypt (EncryptedArray &ea, const FHESecKey &seckey) {
        vector<long> res;
        for (int i = 0; i < vec.size(); i++) {
            vector<long> bits (global_nslots);
            ea.decrypt(vec[i], seckey, bits);
            res.push_back(bits[0]);
        }
        return res;
    }
};

struct heblock {
    ctvec x;
    ctvec y;
};

uint32_t rotateLeft(uint32_t x, int n) {
    return x << n | x >> 32 - n;
}

void rotateLeft(vector<Ctxt> &x, int n) {
    std::rotate(x.begin(), x.begin() + n, x.end());
}

block decRound(key32 k, block inp) {
    block ret = { inp.y, inp.x ^ (rotateLeft(inp.y,1) & rotateLeft(inp.y,8)) ^ rotateLeft(inp.y,2) ^ k };
    return ret;
}

void expandKey(key &inp){
    inp.reserve(T);
    if (inp.size() != 4)
        throw std::invalid_argument( "Key must be of length 4" );
    for (int i = m; i < T; i++) {
        key32 tmp;
        tmp = rotateLeft(inp[i-1], 3) ^ inp[i-3];
        tmp ^= rotateLeft(tmp, 1);
        tmp ^= 3;
        tmp ^= z[j][i-m % 62];
        tmp ^= ~inp[i-m];
        inp.push_back(tmp);
    }
}

block encRound(key32 k, block inp) {
    uint32_t y = inp.y;
    uint32_t x = inp.x;
    y ^= rotateLeft(x,1) & rotateLeft(x,8);
    y ^= rotateLeft(x,2);
    y ^= k;
    return {y,x};
}

void encRound(ctvec key, heblock &inp) {
    ctvec tmp = inp.x;
    ctvec x0 = inp.x;
    ctvec x1 = inp.x;
    ctvec x2 = inp.x;
    ctvec y = inp.y;
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

block decrypt(key k, block inp) {
    block res = inp;
    std::reverse(k.begin(), k.end());
    for (int i = 0; i < k.size(); i++) {
        res = decRound(k[i], res);
    }
    return res;
}


block encrypt(key k, block inp) {
    block res;
    for (int i = 0; i < T-1; i++) {
       res = encRound(k[i], res);
    }
    return res;
}

key genKey() {
    srand(time(NULL));
    key ks;
    for (int i = 0; i < 4; i++) {
        ks.push_back(rand());
    }
    return ks;
}

uint32_t rand32() {
    srand(time(NULL));
    uint32_t x = 0;
    for (int i = 0; i < 32; i++) {
        x |= rand()%2 << i;
    }
    return x;
}

vector<block> strToBlocks (string inp) {
    vector<block> blocks;
    for (int i = 0; i < inp.size(); i += 8) {
        uint32_t x = inp[i]   << 24 | inp[i+1] << 16 | inp[i+2] << 8 | inp[i+3];
        uint32_t y = inp[i+4] << 24 | inp[i+5] << 16 | inp[i+6] << 8 | inp[i+7];
        block b = { x, y };
        blocks.push_back(b);
    }
    return blocks;
}

vector<block> simonEnc (string inp, key k) {
    vector<block> blocks = strToBlocks(inp);
    for (int i = 0; i < blocks.size(); i++) {
        block b = blocks[i];
        blocks[i] = encrypt(k,b);
    }
    return blocks;
}

string blocksToStr (vector<block> bs) {
    string ret;
    for (int i = 0; i < bs.size(); i++) {
        block b = bs[i];
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

string simonDec(vector<block> c, key k) {
    vector<block> bs;
    for (int i = 0; i < c.size(); i++) {
        block b = decrypt(k, c[i]);
        bs.push_back(b);
    }
    return blocksToStr(bs);
}

void addCharBits(char c, vector<long> *v) {
    for (int i = 0; i < 8; i++) {
        v->push_back((c & (1 << i)) >> i);
    }
}

vector<long> charToBits (char c) {
    vector<long> ret;
    for (int i = 0; i < 8; i++) {
        ret.push_back((c & (1 << i)) >> i);
    }
    return ret;
}

void addUint32Bits(uint32_t n, vector<long> *v) {
    for (int i = 0; i < 32; i++) {
        v->push_back((n & (1 << i)) >> i);
    }
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

void pad (vector<long> *inp, int nslots) {
    inp->reserve(nslots);
    for (int i = inp->size(); i < nslots; i++) {
        inp->push_back(0);
    }
}

// returns a vector of an even number of padded vectors with 32 bits of
// information in them. each long is actually just 0 or 1.
vector<vector<long>> strToVectors (string inp, int nslots) {
    vector<vector<long>> ret;
    ret.reserve(inp.size()/4);
    for (int i = 0; i < inp.size(); i+=4) {
        vector<long> c;
        addCharBits(inp[i+3], &c);
        addCharBits(inp[i+2], &c);
        addCharBits(inp[i+1], &c);
        addCharBits(inp[i+0], &c);
        pad(&c, nslots);
        ret.push_back(c);
    }
    if (ret.size()%2) {
        vector<long> c;
        pad(&c, nslots);
        ret.push_back(c);
    }
    return ret;
}

// takes a vector of vectors where each long is 0 or 1. turns it into a string.
string vectorsToStr (vector<vector<long>> inp){
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

// turns a SIMON key (vector<uint32_t>) into a vector<vector<long>> where each
// long is simply 0 or 1.
vector<vector<long>> keyToVectors (vector<uint32_t> key, int nslots) {
    vector<vector<long>> ret;
    for (int i = 0; i < key.size(); i++) {
        vector<long> k;
        addUint32Bits(key[i], &k);
        pad(&k, nslots);
        ret.push_back(k);
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

void printKey (key k) {
    cout << "key = ";
    for (int i = 0; i < k.size(); i++) {
        printf("%X ", k[i]);
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

vector<block> vectorsToBlocks (vector<vector<long>> inp) {
    vector<block> res (inp.size()/2);
    for (int i = 0; i < inp.size(); i+=2) {
        block b = { vectorTo32(inp[i]), vectorTo32(inp[i+1]) };
        res.push_back(b);
    }
    return res;
}

string blocksToHex (vector<block> bs) {
  stringstream ss;
  for (int i = 0; i < bs.size(); i++) {
    ss << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << bs[i].x << " " << bs[i].y;
  }
  return ss.str();
}

string vectorsToHex (vector<vector<long>> inp) {
    return blocksToHex(vectorsToBlocks(inp));
}

// lifts a uint32_t into the HE monad
Ctxt heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, uint32_t x) {
    vector<long> vec;
    addUint32Bits(x, &vec);
    pad(&vec, global_nslots);
    Ctxt c(pubkey);
    ea.encrypt(c, pubkey, vec);
    return c;
}

// lifts a string into the HE monad
vector<heblock> heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, string s) {
    vector<block> pt = strToBlocks(s);
    vector<heblock> cts;
    for (int i = 0; i < pt.size(); i++) {
        ctvec c0 (ea, pubkey, uint32ToBits(pt[i].x));
        ctvec c1 (ea, pubkey, uint32ToBits(pt[i].y));
        cts.push_back({c0,c1});
    }
    return cts;
}

vector<ctvec> heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, vector<uint32_t> &k) {
    vector<ctvec> encryptedKey;
    for (int i = 0; i < k.size(); i++) {
        vector<long> bits = uint32ToBits(k[i]);
        ctvec kct (ea, pubkey, bits);
        encryptedKey.push_back(kct);
    }
    return encryptedKey;
}

vector<vector<long>> heDecrypt (EncryptedArray &ea, const FHESecKey &seckey, vector<ctvec> cts) {
    vector<vector<long>> res (cts.size());
    for (int i = 0; i < cts.size(); i++) {
        res.push_back(cts[i].decrypt(ea, seckey));
    }
    return res;
}

int main(int argc, char **argv)
{
    string inp = "catsrule";
    //cout << "inp = \"" << inp << "\"" << endl;
    key k = genKey();
    expandKey(k);
    //vector<block> bs = strToBlocks(inp);
    printKey(k);
    //cout << "blocks = " << bs[0].x << " " << bs[0].y << endl;

    long m=0, p=2, r=1;
    long L=16;
    long c=3;
    long w=64;
    long d=0;
    long security = 128;
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
    long nslots = ea.size();
    global_nslots = nslots;

    // set up globals
    cout << "Encrypting constants..." << endl;
    ctvec one (ea, pubkey, uint32ToBits(1));
    ctvec zero (ea, pubkey, uint32ToBits(0));
    ctvec maxint (ea, pubkey, uint32ToBits(0xFFFFFFFF));
    global_maxint = &maxint;
    global_zero = &zero;
    global_one = &one;

    // HEencrypt key
    cout << "Encrypting SIMON key..." << endl;
    vector<ctvec> encryptedKey = heEncrypt(ea, pubkey, k);

    // HEencrypt input
    cout << "Encrypting inp..." << endl;
    vector<heblock> cts = heEncrypt(ea, pubkey, inp);

    cout << "Running protocol..." << endl;
    for (int i = 0; i < T; i++) {
        cout << "Round " << i+1 << "/" << T << "...";
        encRound(encryptedKey[i], cts[0]);

        vector<long> ptx = cts[0].x.decrypt(ea, seckey);
        vector<long> pty = cts[0].y.decrypt(ea, seckey);
        block b = { vectorTo32(ptx), vectorTo32(pty) };
        for (int j = i; j >= 0; j--) {
            uint32_t key = k[j];
            b = decRound(key, b);
        }
        cout << "decrypted result" << i+1 << ": \"" << blocksToStr({ b }) << "\"" << endl;
    }
    vector<vector<long>> res;
    res.push_back(cts[0].x.decrypt(ea, seckey));
    res.push_back(cts[0].y.decrypt(ea, seckey));
    cout << "HESimon (dec) = " << vectorTo32(res[0]) << " " << vectorTo32(res[1]) << endl;
    //cout << vectorsToStr(res) << endl;

    vector<block> bs0 = strToBlocks(inp);
    block b = bs0[0];
    for (int i = 0; i < T; i++) {
        b = encRound(k[i], b);
    }
    cout << "PTSimon (dec) = " << b.x << " " << b.y << endl;
    //cout << blocksToStr({ b }) << endl;

    return 0;
}
