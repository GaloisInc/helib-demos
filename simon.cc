#include "FHE.h"
#include <string>
#include <stdio.h>
#include <ctime>
#include "EncryptedArray.h"
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

typedef uint32_t uint;
typedef uint32_t key32;
typedef vector<key32> key;

typedef struct {
    uint x;
    uint y;
} block;

typedef vector<block> ciphertext;

const int m = 4;
const int j = 3;
const int T = 44;

uint rotateLeft(uint x, int n) {
    return x << n | x >> 32 - n;
}

block encRound(key32 k, block inp) {
    block ret = { inp.y ^ (rotateLeft(inp.x,1) & rotateLeft(inp.x,8)) ^ rotateLeft(inp.x,2) ^ k, inp.x };
    return ret;
}

block decRound(key32 k, block inp) {
    block ret = { inp.y, inp.x ^ (rotateLeft(inp.y,1) & rotateLeft(inp.y,8)) ^ rotateLeft(inp.y,2) ^ k };
    return ret;
}

key expandKey(key inp){
    key ks = inp;
    ks.reserve(T-1);
    if (ks.size() != 4) 
        throw std::invalid_argument( "Key must be of length 4" );
    for (int i = m; i < T-1; i++) {
        key32 tmp;
        tmp = rotateLeft(ks[i-1], 3) ^ ks[i-3];
        tmp ^= rotateLeft(tmp, 1);
        ks[i] = ~ks[i-m] ^ tmp ^ z[j][i-m % 62] ^ 3;
    }
    return ks;
}

block encrypt(key k, block inp) {
    block res = inp;
    key ks = expandKey(k);
    for (int i = 0; i < ks.size(); i++) {
        res = encRound(ks[i], res);   
    }
    return res;
}

block decrypt(key k, block inp) {
    block res = inp;
    key ks = expandKey(k);
    std::reverse(ks.begin(), ks.end());
    for (int i = 0; i < ks.size(); i++) {
        res = decRound(ks[i], res);   
    }
    return res;
}

uint64_t rand64() {
    srand(time(NULL));
    uint64_t out = 0;
    for (int i = 0; i < 64; i++) {
        bool bit = rand() % 2; 
        out += bit * pow(2,i);
    }
    return out;
}

key genKey() {
    srand(time(NULL));
    key ks;
    for (int i = 0; i < 4; i++) {
        ks.push_back(rand());
    }
    return ks;
}

ciphertext simonEnc(string inp, key k) {
    ciphertext blocks;
    for (int i = 0; i < inp.size(); i += 8) {
        uint x = inp[i]   << 24 | inp[i+1] << 16 | inp[i+2] << 8 | inp[i+3];
        uint y = inp[i+4] << 24 | inp[i+5] << 16 | inp[i+6] << 8 | inp[i+7];
        block b = { x, y };
        blocks.push_back(encrypt(k,b));
    }
    return blocks;
}

string simonDec(ciphertext c, key k) {
    string ret;
    for (int i = 0; i < c.size(); i++) {
        block b = decrypt(k, c[i]);
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

int main(int argc, char **argv) {
    key k = genKey();
    string inp = "hello world, what is up in the neighborhood?";
    cout << "inp = " << inp << endl;
    ciphertext c = simonEnc(inp, k);
    string r = simonDec(c,k);
    cout << "decrypted = " << r << endl;
    return 0;
}
