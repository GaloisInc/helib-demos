// Copyright (c) 2013-2014 Galois, Inc.
// Distributed under the terms of the GPLv3 license (see LICENSE file)
//
// Author: Brent Carmer
//
// This file contains mostly data mangling functions for use in simon-blocks
// and simon-simd.

#ifndef SIMONUTIL_H
#define SIMONUTIL_H

#include <iostream>
#include <vector>
#include <time.h>
#include "FHE.h"
#include "EncryptedArray.h"
#include "simon-plaintext.h"

using namespace std;

string blocksToStr (vector<pt_block> bs);

vector<pt_block> strToBlocks (string inp);

void addCharBits(char c, vector<long> *v);

vector<pt_block> strToBlocks (string inp);

void addCharBits(char c, vector<long> *v);

vector<long> charToBits (char c);

vector<long> uint32ToBits (uint32_t n);

void pad (long padWith, vector<long> &inp, long length);

char charFromBits(vector<long> inp);

uint32_t vectorTo32 (vector<long> inp);

vector<vector<long>> strToVectors (string inp);

vector<vector<long>> keyToVectors (vector<uint32_t> key, long nslots);

string vectorsToStr (vector<vector<long>> inp);

vector<uint32_t> vectorsTo32 (vector<vector<long>> inp);

void printKey (vector<pt_key32> k);

void printVector (vector<long> inp);

void printVector (vector<vector<long>> inp);

vector<pt_block> vectorsToBlocks (vector<vector<long>> inp);

vector<vector<long>> transpose (vector<vector<long>> inp);

vector<vector<long>> transpose (vector<long> inp);

void timer(bool init = false);

#endif
