/* Copyright (C) 2012,2013 IBM Corp.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _FHE_H_
#define _FHE_H_
/**
   @file FHE.h
   @brief Public/secret keys for the BGV cryptosystem
*/
#include <vector>
#include "NTL/ZZX.h"
#include "DoubleCRT.h"
#include "FHEContext.h"
#include "Ctxt.h"

/**
 * @class KeySwitch
 * @brief Key-switching matrices 
 *
 * There are basically two approaches for how to do key-switching: either
 * decompose the mod-q ciphertext into bits (or digits) to make it low-norm,
 * or perform the key-switching operation mod Q>>q. The tradeoff is that when
 * decomposing the (coefficients of the) ciphertext into t digits, we need to
 * increase the size of the key-switching matrix by a factor of t (and the
 * running time similarly grows). On the other hand if we do not decompose at
 * all then we need to work modulo Q>q^2, which means that the bitsize of our
 * largest modulus q0 more than doubles (and hence also the parameter m more
 * than doubles). In general if we decompose into digits of size B then we
 * need to work with Q>q*B.)
 *
 * The part of the spectrum where we expect to find the sweet spot is when
 * we decompose the ciphertext into digits of size B=q0^{1/t} for some small
 * constant t (maybe t=2,3 or so). This means that our largest modulus
 * has to be Q>q0^{1+1/t}, which increases also the parameter m by a factor
 * (1+1/t). It also means that for key-switching in the top levels we would
 * break the ciphertext to t digits, hence the key-switching matrix will have
 * t columns.
 *
 * A key-switch matrix W[s'->s] converts a ciphertext-part with respect to
 * secret-key polynomial s' into a canonical cipehrtext (i.e. a two-part
 * ciphertext with respect to (1,s)). The matrix W is a 2-by-t matrix of
 * DoubleCRT objects. The bottom row are just (psudo)random elements. Then
 * for column i, if the bottom element is ai then the top element is set as
 *     bi = P*Bi*s' + p*ei - s * ai mod P*q0,
 * where p is the plaintext space (i.e. 2 or 2^r) and Bi is the product of the
 * digits-sizes corresponding to columns 0...i-1. (For example if we have
 * digit sizes 3,5,7 then B0=1, B1=3, B2=15 and B3=105.) Also, q0 is the
 * product of all the "ciphertext primes" and P is roughly the product of all
 * the special primes. (Actually, if Q is the product of all the special
 * primes then P=Q*(Q^{-1} mod p).)
 * 
 * In this implementation we save some space, by keeping only a PRG seed for
 * generating the pseudo-random elements, rather than the elements themselves.
 *
 * To convert a cipehrtext part R, we break R into digits R = sum_i Bi Ri,
 * then set (q0,q1)^T = sum_i Ri * column-i. Note that we have
 * <(1,s),(q0,q1)> = sum_i Ri*(s*ai - s*ai + p*ei +P*Bi*s')
 *       = P * sum_i Bi*Ri * s' + p sum_i Ri*ei
 *       = P * R * s' + p*a-small-element (mod P*q0)
 * where the last element is small since the ei's are small and |Ri|<B.
 * Note that if the ciphertext is encrypted relative to plaintext space p'
 * and then key-switched with matrices W relative to plaintext space p, then
 * we get a mew ciphertxt with noise p'*small+p*small, so it is valid relative
 * to plaintext space GCD(p',p).
 *
 * The matrix W is defined modulo Q>t*B*sigma*q0 (with sigma a bound on the
 * size of the ei's), and Q is the product of all the small primes in our
 * moduli chain. However, if p is much smaller than B then is is enough to
 * use W mod Qi with Qi a smaller modulus, Q>p*sigma*q0. Also note that if
 * p<Br then we will be using only first r columns of the matrix W.
 ********************************************************************/
class KeySwitch { 
public:
  SKHandle fromKey;  // A handle for the key s'
  long     toKeyID;  // Index of the key s that we are switching into
  long     ptxtSpace;  // either 2 or 2^r

  vector<DoubleCRT> b;  // The top row, consisting of the bi's
  ZZ prgSeed;        // a seed to generate the random ai's in the bottom row
                     // NOTE: THIS USE OF THE NTL PRG IS NOT THREAD-SAFE 

  explicit
  KeySwitch(long sPow=0, long xPow=0, long fromID=0, long toID=0, long p=0):
    fromKey(sPow,xPow,fromID),toKeyID(toID),ptxtSpace(p) {}
  explicit
  KeySwitch(const SKHandle& _fromKey, long fromID=0, long toID=0, long p=0):
    fromKey(_fromKey),toKeyID(toID),ptxtSpace(p) {}

  bool operator==(const KeySwitch& other) const;
  bool operator!=(const KeySwitch& other) const {return !(*this==other);}

  unsigned long NumCols() const { return b.size(); }

  //! @brief returns a dummy static matrix with toKeyId == -1
  static const KeySwitch& dummy();

  //! A debugging method
  void verify(FHESecKey& sk);

  //! @brief Read a key-switching matrix from input
  void readMatrix(istream& str, const FHEcontext& context);
};
ostream& operator<<(ostream& str, const KeySwitch& matrix);
// We DO NOT have istream& operator>>(istream& str, KeySwitch& matrix);
// instead must use the readMatrix method above, where you can specify context


/**
 * @class FHEPubKey
 * @brief The public key
 ********************************************************************/
class FHEPubKey { // The public key
  const FHEcontext& context; // The context

  //! @var Ctxt pubEncrKey
  //! The public encryption key is an encryption of 0,
  //! relative to the first secret key
  Ctxt pubEncrKey;

  vector<long> skHwts; // The Hamming weight of the secret keys
  vector<KeySwitch> keySwitching; // The key-switching matrices

  // The keySwitchMap structure contains pointers to key-switching matrices
  // for re-linearizing automorphisms. The entry keySwitchMap[i][n] contains
  // the index j such that keySwitching[j] is the first matrix one needs to
  // use when re-linearizing s_i(X^n). 
  vector< vector<long> > keySwitchMap;

public:
  FHEPubKey(): // this constructor thorws run-time error if activeContext=NULL
    context(*activeContext), pubEncrKey(*this) {}

  explicit
  FHEPubKey(const FHEcontext& _context): 
    context(_context), pubEncrKey(*this) {}

  FHEPubKey(const FHEPubKey& other): // copy constructor
    context(other.context), pubEncrKey(*this), skHwts(other.skHwts), 
    keySwitching(other.keySwitching), keySwitchMap(other.keySwitchMap) 
  { // copy the pubEncrKey w/o checking the reference to the public key
    pubEncrKey.privateAssign(other.pubEncrKey);
  }

  void clear() { // clear all public-key data
    pubEncrKey.clear(); skHwts.clear(); 
    keySwitching.clear(); keySwitchMap.clear();
  }

  bool operator==(const FHEPubKey& other) const;
  bool operator!=(const FHEPubKey& other) const {return !(*this==other);}

  // Access methods
  const FHEcontext& getContext() const {return context;}
  long getPtxtSpace() const { return pubEncrKey.ptxtSpace; }


  //! @brief The Hamming weight of the secret key
  long getSKeyWeight(long keyID=0) const {return skHwts[keyID];}


  ///@{
  //! @name Find key-switching matrices

  //! @brief Find a key-switching matrix by its indexes. 
  //! If no such matrix exists it returns a dummy matrix with toKeyID==-1.
   const KeySwitch& getKeySWmatrix(const SKHandle& from, long toID=0) const;
  const KeySwitch& getKeySWmatrix(long fromSPower, long fromXPower, long fromID=0, long toID=0) const
  { return getKeySWmatrix(SKHandle(fromSPower,fromXPower,fromID), toID); }

  bool haveKeySWmatrix(const SKHandle& from, long toID=0) const
  { return getKeySWmatrix(from,toID).toKeyID >= 0; }

  bool haveKeySWmatrix(long fromSPower, long fromXPower, long fromID=0, long toID=0) const
  { return haveKeySWmatrix(SKHandle(fromSPower,fromXPower,fromID), toID); }

  //! @brief Is there a matrix from this key to *any* base key?
  const KeySwitch& getAnyKeySWmatrix(const SKHandle& from) const;
  bool haveAnyKeySWmatrix(const SKHandle& from) const
  { return getAnyKeySWmatrix(from).toKeyID >= 0; }

  //!@brief Get the next matrix to use for multi-hop automorphism
  //! See Section 3.2.2 in the design document
  const KeySwitch& getNextKSWmatrix(long fromXPower, long fromID=0) const
  { long matIdx = keySwitchMap.at(fromID).at(fromXPower);
    return (matIdx>=0? keySwitching.at(matIdx) : KeySwitch::dummy());
  }
  ///@}

  //! @brief Is it possible to re-linearize the automorphism X -> X^k
  //! See Section 3.2.2 in the design document (KeySwitchMap)
  bool isReachable(long k, long keyID=0) const
  { return keySwitchMap.at(keyID).at(k)>=0; }

  //! @brief Compute the reachability graph of key-switching matrices
  //! See Section 3.2.2 in the design document (KeySwitchMap)
  void setKeySwitchMap(long keyId=0);  // Computes the keySwitchMap pointers

  //! @brief Encrypts plaintext, result returned in the ciphertext argument.
  //! The returned value is the plaintext-space for that ciphertext. When
  //! called with highNoise=true, returns a ciphertext with noise level~q/8.
  long Encrypt(Ctxt &ciphertxt, const ZZX& plaintxt, long ptxtSpace=0,
	       bool highNoise=false) const;

  friend class FHESecKey;
  friend ostream& operator << (ostream& str, const FHEPubKey& pk);
  friend istream& operator >> (istream& str, FHEPubKey& pk);
};

/**
 * @class FHESecKey
 * @brief The secret key
******************************************************************/
class FHESecKey: public FHEPubKey { // The secret key
public:
  vector<DoubleCRT> sKeys; // The secret key(s) themselves

public:

  // Constructors just call the ones for the base class
  FHESecKey(){}

  explicit
  FHESecKey(const FHEcontext& _context): FHEPubKey(_context) {}

  bool operator==(const FHESecKey& other) const;
  bool operator!=(const FHESecKey& other) const {return !(*this==other);}

  void clear() // clear all secret-key data
  { FHEPubKey::clear(); sKeys.clear(); }

  //! We allow the calling application to choose a secret-key polynomial by
  //! itself, then insert it into the FHESecKey object, getting the index of
  //! that secret key in the sKeys list. If this is the first secret-key for
  //! this object then the procedure below also generate a corresponding public
  //! encryption key.
  //! It is assumed that the context already contains all parameters.
  long ImportSecKey(const DoubleCRT& sKey, long hwt, long ptxtSpace=0);

  //! Key generation: This procedure generates a single secret key, pushes it
  //! onto the sKeys list using ImportSecKey from above.
  long GenSecKey(long hwt, long ptxtSpace=0)
  { DoubleCRT newSk(context); // defined relative to all primes, special or not
    newSk.sampleHWt(hwt);     // samle a Hamming-weight-hwt polynomial
    return ImportSecKey(newSk, hwt, ptxtSpace);
  }

  //! Generate a key-switching matrix and store it in the public key. The i'th
  //! column of the matrix encrypts fromKey*B1*B2*...*B{i-1}*Q under toKey,
  //! relative to the largest modulus (i.e., all primes) and plaintext space p.
  //! Q is the product of special primes, and the Bi's are the products of
  //! primes in the i'th digit. The plaintext space defaults to 2^r, as defined
  //! by context.mod2r.
  void GenKeySWmatrix(long fromSPower, long fromXPower, long fromKeyIdx=0,
		      long toKeyIdx=0, long ptxtSpace=0);

  // Decryption
  void Decrypt(ZZX& plaintxt, const Ctxt &ciphertxt) const;

  //! @brief Debugging version, returns in f the polynomial
  //! before reduction modulo the ptxtSpace
  void Decrypt(ZZX& plaintxt, const Ctxt &ciphertxt, ZZX& f) const;

  //! @brief Symmetric encryption using the secret key.
  long Encrypt(Ctxt &ctxt, const ZZX& ptxt,
	       long ptxtSpace=0, long skIdx=0) const;

  friend ostream& operator << (ostream& str, const FHESecKey& sk);
  friend istream& operator >> (istream& str, FHESecKey& sk);
};

//! @name Strategies for generating key-switching matrices
//! These functions are implemented in KeySwitching.cpp

//! @brief Maximalistic approach:
//! generate matrices s(X^e)->s(X) for all e in Zm*
void addAllMatrices(FHESecKey& sKey, long keyID=0);

//! @brief Generate matrices so every s(X^e) can be reLinearized
//! in at most two steps
void addFewMatrices(FHESecKey& sKey, long keyID=0);

//! @brief Generate all matrices s(X^{g^i})->s(X) for generators g of
//! Zm* /(p) and i<ord(g). If g has different orders in Zm* and Zm* /(p)
//! then generate also matrices of the form s(X^{g^{-i}})->s(X)
void add1DMatrices(FHESecKey& sKey, long keyID=0);

//! @brief Generate some matrices of the form s(X^{g^i})->s(X), but not all.
//! For a generator g whose order is larger than bound, generate only enough
//! matrices for the giant-step/baby-step procedures (2*sqrt(ord(g))of them).
void addSome1DMatrices(FHESecKey& sKey, long bound=100, long keyID=0);

//! Generate all Frobenius matrices of the form s(X^{2^i})->s(X)
void addFrbMatrices(FHESecKey& sKey, long keyID=0);

//! Generate all key-switching matrices for a given permutation network
class PermNetwork;
void addMatrices4Network(FHESecKey& sKey, const PermNetwork& net, long keyID=0);

//! Choose random c0,c1 such that c0+s*c1 = p*e for a short e
void RLWE(DoubleCRT& c0, DoubleCRT& c1, const DoubleCRT &s, long p,
	  ZZ* prgSeed=NULL);

//! Same as RLWE, but assumes that c1 is already chosen by the caller
void RLWE1(DoubleCRT& c0, const DoubleCRT& c1, const DoubleCRT &s, long p);

#endif // ifndef _FHE_H_
