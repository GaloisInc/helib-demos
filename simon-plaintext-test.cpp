// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// A test that the plaintext implementation of SIMON is correct.

#include <math.h>
#include "simon-plaintext.h"
#include "simon-util.h"

int main(int argc, char **argv)
{
    string inp = "secrets!";
    cout << "inp = \"" << inp << "\"" << endl;
    cout << "T = " << T << endl;
    vector<pt_block> bs = strToBlocks(inp);
    printf("as block : 0x%08x 0x%08x\n", bs[0].x, bs[0].y);
    //key k = genKey();
    vector<pt_key32> k ({0x1b1a1918, 0x13121110, 0x0b0a0908, 0x03020100});
    pt_expandKey(k);
    printKey(k);
    
    vector<clock_t> times;
    for (size_t i = 0; i < 1000; i++) {
        clock_t t = clock();
        for (size_t j = 0; j < T; j++) {
            bs[0] = pt_encRound(k[j], bs[0]);
        }
        t = clock() - t;
        times.push_back(t);
    }
    clock_t sum = 0;
    for (clock_t x : times){
        sum += x;
    }
    sum /= times.size();
    printf("%.0fns/round\n", sum / (double)CLOCKS_PER_SEC * pow(10,9) / T);
}
