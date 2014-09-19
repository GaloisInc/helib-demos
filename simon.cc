#include "FHE.h"
#include <string>
#include <stdio.h>
#include <ctime>
#include "EncryptedArray.h"
#include <NTL/lzz_pXFactoring.h>
#include <fstream>
#include <sstream>
#include <sys/time.h>

#include <stdint.h>
#include <bitset>
#include <tuple>

std::bitset<62> z0(0b11111010001001010110000111001101111101000100101011000011100110);
std::bitset<62> z1(0b10001110111110010011000010110101000111011111001001100001011010);
std::bitset<62> z2(0b10101111011100000011010010011000101000010001111110010110110011);
std::bitset<62> z3(0b11011011101011000110010111100000010010001010011100110100001111);
std::bitset<62> z4(0b11010001111001101011011000100000010111000011001010010011101111);
bitset<62> z[5] = { z0, z1, z2, z3, z4 };

// im assuming unsigned ints are 32 bits long

typedef struct {
    uint32_t fst;
    uint32_t snd;
} block64;

typedef uint32_t key32;

uint32_t rotateLeft(uint32_t x, int n) {
    return x << n | x >> 32 - n;
}

int main(int argc, char **argv) {
    // should be 3
    cout << rotateLeft(0b10000000000000000000000000000001, 1) << endl;
    return 0;
}
