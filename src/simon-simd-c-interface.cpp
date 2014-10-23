// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Copyright (c) 2013-2014 Galois, Inc.
// Author: Brent Carmer
//
// This file defines a C interface for simon functions

#include "simon-blocks-c-interface.h"

extern "C" {

uint32_t c_blocks_rotateLeft (uint32_t x, uint32_t n) {
    Ctxt c = heEncrypt(FHEPubKey(), x);
    rotateLeft32(c, n);
    return heDecrypt(FHESecKey(), c);
}
    
}
