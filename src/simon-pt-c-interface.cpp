// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Copyright (c) 2013-2014 Galois, Inc.
// Author: Brent Carmer
//
// This file defines a C interface for simon functions

#include "simon-pt-c-interface.h"

extern "C" {

uint32_t c_pt_rotateLeft (uint32_t x, uint32_t n) {
    return pt_rotateLeft(x,n); 
}

void c_pt_expandKey (uint32_t k[4], uint32_t exp_k[44]) {
    std::vector<pt_key32> k_vec (k, k+4);
    pt_expandKey(k_vec, 44);
    std::copy(k_vec.begin(), k_vec.end(), exp_k);
}

//void pt_expandKey(vector<pt_key32> &k, size_t nrounds = T);

}
