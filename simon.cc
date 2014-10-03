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
//const int T = 4;
//const int T = 1;

// Plaintext SIMON
typedef uint32_t pt_key32;

struct pt_block {
    uint32_t x;
    uint32_t y;
};

uint32_t pt_rotateLeft(uint32_t x, int n) {
    return x << n | x >> 32 - n;
}

void pt_expandKey(vector<pt_key32> &inp){
    inp.reserve(T);
    if (inp.size() != 4)
        throw std::invalid_argument( "Key must be of length 4" );
    for (int i = m; i < T; i++) {
        pt_key32 tmp;
        tmp  = pt_rotateLeft(inp[i-1], 3) ^ inp[i-3];
        tmp ^= pt_rotateLeft(tmp, 1);
        tmp ^= 3;
        tmp ^= z[j][i-m % 62];
        tmp ^= ~inp[i-m];
        inp.push_back(tmp);
    }
}

pt_block pt_encRound(pt_key32 k, pt_block inp) {
    uint32_t x = inp.x;
    uint32_t y = inp.y;
    y ^= pt_rotateLeft(x,1) & pt_rotateLeft(x,8);
    y ^= pt_rotateLeft(x,2);
    y ^= k;
    return { y, x };
}

pt_block pt_decRound(pt_key32 k, pt_block inp) {
    uint32_t x = inp.x;
    uint32_t y = inp.y;
    x ^= pt_rotateLeft(y,1) & pt_rotateLeft(y,8);
    x ^= pt_rotateLeft(y,2) ^ k;
    return { y, x };
}

pt_block pt_encBlock(vector<pt_key32> k, pt_block inp, int nrounds = T) {
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

vector<pt_block> pt_simonEnc (vector<pt_key32> k, string inp, int nrounds = T) {
    vector<pt_block> blocks = strToBlocks(inp);
    for (int i = 0; i < blocks.size(); i++) {
        pt_block b = blocks[i];
        blocks[i] = pt_encBlock(k, b, nrounds);
    }
    return blocks;
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

pt_block pt_decBlock(vector<pt_key32> k, pt_block inp, int nrounds = T) {
    pt_block res = inp;
    for (int i = nrounds-1; i >= 0; i--) {
        res = pt_decRound(k[i], res);
    }
    return res;
}

string pt_simonDec(vector<pt_key32> k, vector<pt_block> c, int nrounds = T) {
    vector<pt_block> bs;
    for (int i = 0; i < c.size(); i++) {
        pt_block b = pt_decBlock(k, c[i], nrounds);
        bs.push_back(b);
    }
    return blocksToStr(bs);
}
class ctvec;

long   global_nslots;
ctvec* global_maxint;
ctvec* global_one;
ctvec* global_zero;

class ctvec {
    vector<Ctxt> cts;
    EncryptedArray* ea;
    const FHEPubKey* pubkey;
    int nelems;

    void pad (long x, vector<long> &inp) {
        inp.reserve(global_nslots);
        for (int i = inp.size(); i < global_nslots; i++) inp.push_back(x);
    }

    void addUint32Bits(uint32_t n, vector<long> *v) {
        for (int i = 0; i < 32; i++) v->push_back((n & (1 << i)) >> i);
    }

public:

    // inp comes in already bitsliced/transposed
    ctvec (EncryptedArray &inp_ea, const FHEPubKey &inp_pubkey, vector<vector<long>> inp, bool fill=false) {
        ea = &inp_ea;
        pubkey = &inp_pubkey;
        nelems = inp[0].size();
        for (int i = 0; i < inp.size(); i++) {
            if (fill) {
                pad(inp[i][0]&1, inp[i]);
            } else {
                pad(0, inp[i]);
            }
            Ctxt c(*pubkey);
            ea->encrypt(c, *pubkey, inp[i]);
            cts.push_back(c);
        }
    }

    Ctxt get (int i) {
        return cts[i];
    }

    void xorWith (ctvec &other) {
        for (int i = 0; i < cts.size(); i++) {
            cts[i] += other.get(i);
        }
    }

    void andWith (ctvec &other) {
        for (int i = 0; i < cts.size(); i++) {
            cts[i] *= other.get(i);
        }
    }

    void rotateLeft (int n) {
        rotate(cts.begin(), cts.end()-n, cts.end());
    }

    vector<vector<long>> decrypt (const FHESecKey &seckey) {
        vector<vector<long>> res;
        for (int i = 0; i < cts.size(); i++) {
            vector<long> bits (global_nslots);
            ea->decrypt(cts[i], seckey, bits);
            res.push_back(bits);
        }
        return res;
    }
};

struct heblock {
    ctvec x;
    ctvec y;
};

struct pt_preblock {
    vector<vector<long>> xs;
    vector<vector<long>> ys;
};


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

pt_preblock blocksToPreblock (vector<pt_block> bs) {
    vector<vector<long>> xs;
    vector<vector<long>> ys;
    for (int i = 0; i < 32; i++) {
        vector<long> nextx;
        vector<long> nexty;
        for (int j = 0; j < bs.size(); j++) {
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

vector<pt_block> preblockToBlocks (pt_preblock b) {
    vector<pt_block> bs;
    for (int j = 0; j < b.xs[0].size(); j++) {
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

vector<pt_block> heblockToBlocks (const FHESecKey &k, heblock ct) {
    vector<pt_block> res;
    vector<vector<long>> xs = ct.x.decrypt(k);
    vector<vector<long>> ys = ct.y.decrypt(k);
    return preblockToBlocks({ xs, ys });
}

//string blocksToHex (vector<pt_block> bs) {
  //stringstream ss;
  //for (int i = 0; i < bs.size(); i++) {
    //ss << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << bs[i].x << " " << bs[i].y;
  //}
  //return ss.str();
//}

//string vectorsToHex (vector<vector<long>> inp) {
    //return blocksToHex(vectorsToBlocks(inp));
//}

heblock heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, string s) {
    vector<pt_block> pt = strToBlocks(s);
    pt_preblock b = blocksToPreblock(pt);
    ctvec c0 (ea, pubkey, b.xs);
    ctvec c1 (ea, pubkey, b.ys);
    return { c0, c1 };
}

vector<ctvec> heEncrypt (EncryptedArray &ea, const FHEPubKey &pubkey, vector<uint32_t> &k) {
    vector<ctvec> encryptedKey;
    for (int i = 0; i < k.size(); i++) {
        vector<long> bits = uint32ToBits(k[i]);
        vector<vector<long>> trans = transpose(bits);
        ctvec kct (ea, pubkey, trans, true);
        encryptedKey.push_back(kct);
    }
    return encryptedKey;
}

//vector<vector<long>> heDecrypt (const FHESecKey &seckey, heblock ct) {
    //vector<vector<long>> ;
    //for (int i = 0; i < cts.size(); i++) {
        //res.push_back(cts[i].x.decrypt(seckey));
        //res.push_back(cts[i].y.decrypt(seckey));
    //}
    //return res;
//}

void timer(bool init = false) {
    static time_t old_time;
    static time_t new_time;
    if (!init) {
        old_time = new_time;
    }
    new_time = std::time(NULL);
    if (!init) {
        cout << (new_time - old_time) << "s" << endl;
    }
}


int main(int argc, char **argv)
{
    string inp = "dude, cats rule. galois should have a cat room.";
    cout << "inp = \"" << inp << "\"" << endl;
    vector<pt_key32> k = pt_genKey();
    pt_expandKey(k);
    printKey(k);

    // initialize helib
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
    cout << "nslots = " << nslots << endl;
    global_nslots = nslots;
    timer(true);

    // set up globals
    cout << "Encrypting constants..." << flush;
    ctvec one (ea, pubkey, transpose(uint32ToBits(1)));
    ctvec zero (ea, pubkey, transpose(uint32ToBits(0)));
    ctvec maxint (ea, pubkey, transpose(uint32ToBits(0xFFFFFFFF)));
    global_maxint = &maxint;
    global_zero = &zero;
    global_one = &one;
    timer();


    // HEencrypt key
    cout << "Encrypting SIMON key..." << flush;
    vector<ctvec> encryptedKey = heEncrypt(ea, pubkey, k);
    timer();

    // HEencrypt input
    cout << "Encrypting inp..." << flush;
    heblock ct = heEncrypt(ea, pubkey, inp);
    timer();

    cout << "Running protocol..." << endl;
    for (int i = 0; i < T; i++) {
        cout << "Round " << i+1 << "/" << T << "..." << flush;
        encRound(encryptedKey[i], ct);
        timer();

        // check intermediate result for noise
        cout << "decrypting..." << flush;
        vector<pt_block> bs = heblockToBlocks(seckey, ct);
        timer();
        cout << "result: \"" << pt_simonDec(k, bs, i+1) << "\" " << endl;
    }
    
    timer(true);
    cout << "Decrypting result..." << flush;
    vector<pt_block> bs = heblockToBlocks(seckey, ct);
    timer();
    cout << "result: \"" << pt_simonDec(k, bs) << "\"" << endl;
    return 0;
}
