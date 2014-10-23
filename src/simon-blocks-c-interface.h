// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file defines a C interface for simon functions

#ifndef SIMONBLOCKSC_H
#define SIMONBLOCKSC_H

#include "simon-blocks.h"

extern "C" {

uint32_t c_blocks_rotateLeft (uint32_t x, uint32_t n);

}

#endif
