// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file defines a C interface for simon functions

#ifndef SIMONPTC_H
#define SIMONPTC_H

#include "simon-pt.h"

extern "C" {

uint32_t c_pt_rotateLeft (uint32_t x, uint32_t n);

}

#endif
