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
#include <NTL/ZZ.h>

NTL_CLIENT

#include "FHEContext.h"
#include "Ctxt.h"
#include "FHE.h"
#include "timing.h"


// Sanity-check: Check that prime-set is "valid", i.e. that it 
// contains either all the special primes or none of them
bool Ctxt::verifyPrimeSet() const
{
  IndexSet s = primeSet & context.specialPrimes; // special primes in primeSet
  if (!empty(s) && s!=context.specialPrimes) return false;

  s = primeSet / s;                              // ctxt primes in primeSet
  return (s.isInterval() && s.first()<=1 && !empty(s));
}


//! @brief How many levels in the "base-set" for that ciphertext
long Ctxt::findBaseLevel() const 
{
  IndexSet s;
  findBaseSet(s);
  if (context.containsSmallPrime()) {
    if (s.contains(context.ctxtPrimes.first()))
      return 2*card(s) -1; // 1st prime is half size
    else
      return 2*card(s);
  }
  else return card(s);     // one prime per level
}

bool CtxtPart::operator==(const CtxtPart& other) const
{
  if (((DoubleCRT&)*this)!=((DoubleCRT&)other)) return false;
  
  return (skHandle==other.skHandle);
}

// Checking equality between ciphertexts. This routine performs a
// "shallow" check, comparing only pointers to ciphertext parts.
bool Ctxt::equalsTo(const Ctxt& other, bool comparePkeys) const
{
  if (&context != &other.context) return false;
  if (comparePkeys && &pubKey != &other.pubKey) return false;

  if (parts.size() != other.parts.size()) return false;
  for (size_t i=0; i<parts.size(); i++)
    if (parts[i] != other.parts[i]) return false;

  if (primeSet != other.primeSet) return false;
  if (ptxtSpace != other.ptxtSpace) return false;

  // compare noiseVar, ignoring small deviations
  if (noiseVar == 0.0) return (other.noiseVar == 0.0);
  xdouble ratio = other.noiseVar / noiseVar;
  return (ratio>0.9 && ratio<1.1);
}

// Constructor
Ctxt::Ctxt(const FHEPubKey& newPubKey, long newPtxtSpace):
  context(newPubKey.getContext()), pubKey(newPubKey), ptxtSpace(newPtxtSpace),
  noiseVar(to_xdouble(0.0))
{
  if (ptxtSpace != pubKey.getPtxtSpace()) { // plaintext-space mistamtch
    ptxtSpace = GCD(ptxtSpace, pubKey.getPtxtSpace());
    if (ptxtSpace <= 1) Error("Plaintext-space mismatch Ctxt constructor");
  }
  primeSet=context.ctxtPrimes;
}

// A private assignment method that does not check equality of context or
// public key, this needed for example when we copy the pubEncrKey member
// between different public keys.
Ctxt& Ctxt::privateAssign(const Ctxt& other)
{
  if (this == &other) return *this; // both point to the same object

  parts = other.parts;
  primeSet = other.primeSet;
  ptxtSpace = other.ptxtSpace;
  noiseVar  = other.noiseVar;
  return *this;
}

// Ciphertext maintenance

// mod-switch up to add the primes in s \setminus primeSet, after this call we
// have s<=primeSet. s must contain either all special primes or none of them.
void Ctxt::modUpToSet(const IndexSet &s)
{
  FHE_TIMER_START;
  IndexSet setDiff = s/primeSet; // set minus (primes in s but not in primeSet)
  if (empty(setDiff)) return;    // nothing to do, no primes are added

  // scale up all the parts to use also the primes in setDiff
  double f = 0.0;
  for (long i=0; i<lsize(parts); i++) {
    // addPrimesAndScale returns the logarithm of the product of added primes,
    // all calls should return the same value = log(prod. of primes in setDiff)
    f = parts[i].addPrimesAndScale(setDiff);
  }

  // The variance estimate grows by a factor of exp(f)^2 = exp(2f)
  noiseVar *= xexp(2*f);

  primeSet.insert(setDiff); // add setDiff to primeSet
  assert(verifyPrimeSet()); // sanity-check: ensure primeSet is still valid
  FHE_TIMER_STOP;
}

// mod-switch down to primeSet \intersect s, after this call we have
// primeSet<=s. s must contain either all special primes or none of them.
void Ctxt::modDownToSet(const IndexSet &s)
{
  FHE_TIMER_START;

  IndexSet intersection = primeSet & s;
  assert(!empty(intersection));       // some primes must be left
  if (intersection==primeSet) return; // nothing to do, removing no primes
  IndexSet setDiff = primeSet / intersection; // set-minus

  // Scale down all the parts: use either a simple "drop down" (just removing
  // primes, i.e., reducing the ctxt modulo the samaller modulus), or a "real
  // modulus switching" with rounding, basically whichever yeilds smaller
  // noise. Recall that we keep the invariant that a ciphertext mod Q is
  // decrypted to Q*m (mod p), so if we just "drop down" we still need to
  // multiply by (Q^{-1} mod p).

  // Get an estimate for the added noise term for modulus switching
  xdouble addedNoiseVar = modSwitchAddedNoiseVar();
  if (noiseVar*ptxtSpace*ptxtSpace < addedNoiseVar) {     // just "drop down"
    long prodInv = InvMod(rem(context.productOfPrimes(setDiff),ptxtSpace), ptxtSpace);
    for (size_t i=0; i<parts.size(); i++) {
      parts[i].removePrimes(setDiff);         // remove the primes not in s
      parts[i] *= prodInv;
      // WARNING: the following line is written just so to prevent overflow
      noiseVar = noiseVar*prodInv*prodInv;
    }
    cerr << "DEGENERATE DROP\n";
  } 
  else {                                      // do real mod switching
    for (size_t i=0; i<parts.size(); i++) 
      parts[i].scaleDownToSet(intersection, ptxtSpace);

    // update the noise estimate
    double f = context.logOfProduct(setDiff);
    noiseVar /= xexp(2*f);
    noiseVar += addedNoiseVar;
  }
  primeSet.remove(setDiff); // remove the primes not in s
  assert(verifyPrimeSet()); // sanity-check: ensure primeSet is still valid
  FHE_TIMER_STOP;
}


// Modulus-switching down
void Ctxt::modDownToLevel(long lvl)
{
  long currentLvl;
  IndexSet targetSet;
  IndexSet currentSet = primeSet & context.ctxtPrimes;
  if (context.containsSmallPrime()) {
    currentLvl = 2*card(currentSet);
    if (currentSet.contains(0))
      currentLvl--;  // first prime is half the size

    if (lvl & 1) {   // odd level, includes the half-size prime
      targetSet = IndexSet(0,(lvl-1)/2);
    } else {
      targetSet = IndexSet(1,lvl/2);
    }
  }
  else {
    currentLvl = card(currentSet);
    targetSet = IndexSet(0,lvl-1);    // one prime per level
  }

  // If target is not below the current level, nothing to do
  if (lvl >= currentLvl && currentSet==primeSet) return;

  if (lvl >= currentLvl) { // just remove the special primes
    targetSet = currentSet;
  }

  // sanity-check: interval does not contain special primes
  assert(targetSet.disjointFrom(context.specialPrimes));

  // may need to mod-UP to include the smallest prime
  if (targetSet.contains(0) && !currentSet.contains(0))
    modUpToSet(targetSet); // adds the primes in targetSet / primeSet

  modDownToSet(targetSet); // removes the primes in primeSet / targetSet
}

void Ctxt::blindCtxt(const ZZX& poly)
{
  Ctxt tmp(pubKey);
  pubKey.Encrypt(tmp,poly,ptxtSpace,/*highNoise=*/true);
  *this += tmp;
  // FIXME: This implementation is not optimized, the levels in the
  //  modulus chain should be handled much better.
}

// Reduce plaintext space to a divisor of the original plaintext space
void Ctxt::reducePtxtSpace(long newPtxtSpace)
{
  long g = GCD(ptxtSpace, newPtxtSpace);
  assert (g>1);
  ptxtSpace = g;
}


// key-switch to (1,s_i), s_i is the base key with index keyID. If
// keyID<0 then re-linearize to any key for which a switching matrix exists
void Ctxt::reLinearize(long keyID)
{
  FHE_TIMER_START;
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  // To relinearize, the primeSet must be disjoint from the special primes
  if (!primeSet.disjointFrom(context.specialPrimes)) {
    modDownToSet(primeSet / context.specialPrimes);
    // cout << "<<< special primes\n";
  }

  long g = ptxtSpace;
  Ctxt tmp(pubKey, ptxtSpace); // an empty ciphertext, same plaintext space

  double logProd = context.logOfProduct(context.specialPrimes);
  tmp.noiseVar = noiseVar * xexp(2*logProd); // The noise after mod-UP

  for (size_t i=0; i<parts.size(); i++) {
    CtxtPart& part  = parts[i];

    // For a part relative to 1 or base,  only scale and add
    if (part.skHandle.isOne() || part.skHandle.isBase(keyID)) {
      part.addPrimesAndScale(context.specialPrimes);
      tmp.addPart(part, /*matchPrimeSet=*/true);
      continue;
    }
    // Look for a key-switching matrix to re-linearize this part
    const KeySwitch& W = (keyID>=0)? 
      pubKey.getKeySWmatrix(part.skHandle,keyID) :
      pubKey.getAnyKeySWmatrix(part.skHandle);

    assert(W.toKeyID>=0);       // verify that a switching matrix exists

    g = GCD(W.ptxtSpace, g);    // verify that the plaintext spaces match
    assert (g>1);
    tmp.ptxtSpace = g;

    tmp.keySwitchPart(part, W); // switch this part & update noiseVar
  }
  *this = tmp;

#if 0
  // alternative strategy: get rid of special primes
  // after each reLin...
  if (!primeSet.disjointFrom(context.specialPrimes)) {
    modDownToSet(primeSet / context.specialPrimes);
    // cout << ">>> special primes\n";
  }
#endif


  FHE_TIMER_STOP;
}

// Takes as arguments a ciphertext-part p relative to s' and a key-switching
// matrix W = W[s'->s], uses W to switch p relative to (1,s), and adds the
// result to *this.
// It is assumed that the part p does not include any of the special primes,
// and that if *this is not an empty ciphertext then its primeSet is
// p.getIndexSet() \union context.specialPrimes
void Ctxt::keySwitchPart(const CtxtPart& p, const KeySwitch& W)
{
  FHE_TIMER_START;

  // no special primes in the input part
  assert(context.specialPrimes.disjointFrom(p.getIndexSet()));

  // For parts p that point to 1 or s, only scale and add
  if (p.skHandle.isOne() || p.skHandle.isBase(W.toKeyID)) { 
    CtxtPart pp = p;
    pp.addPrimesAndScale(context.specialPrimes);
    addPart(pp, /*matchPrimeSet=*/true);
    FHE_TIMER_STOP;
    return; 
  }

  // some sanity checks
  assert(W.fromKey == p.skHandle);  // the handles must match

  // Compute the number of digits that we need and the esitmated added noise
  // from switching this ciphertext part.
  long pSpace = W.ptxtSpace;
  long nDigits = 0;
  xdouble addedNoise = to_xdouble(0.0);
  double sizeLeft = context.logOfProduct(p.getIndexSet());
  for (size_t i=0; i<context.digits.size() && sizeLeft>0.0; i++) {    
    nDigits++;

    double digitSize = context.logOfProduct(context.digits[i]);
    if (sizeLeft<digitSize) digitSize=sizeLeft; // need only part of this digit

    // Added noise due to this digit is phi(m) * sigma^2 * pSpace^2 * |Di|^2/4, 
    // where |Di| is the magnitude of the digit

    // WARNING: the following line is written just so to prevent overflow
    addedNoise += to_xdouble(context.zMStar.getPhiM()) * pSpace*pSpace
      * xexp(2*digitSize) * context.stdev*context.stdev / 4.0;

    sizeLeft -= digitSize;
  }

  // Sanity-check: make sure that the added noise is not more than the special
  // primes can handle: After dividing the added noise by the product of all
  // the special primes, it should be smaller than the added noise term due
  // to modulus switching, i.e., keyWeight * phi(m) * pSpace^2 / 12

  long keyWeight = pubKey.getSKeyWeight(p.skHandle.getSecretKeyID());
  double phim = context.zMStar.getPhiM();
  double logModSwitchNoise = log((double)keyWeight) 
    +2*log((double)pSpace) +log(phim) -log(12.0);
  double logKeySwitchNoise = log(addedNoise) 
    -2*context.logOfProduct(context.specialPrimes);
  assert(logKeySwitchNoise < logModSwitchNoise);

  // Break the ciphertext part into digits, if needed, and scale up these
  // digits using the special primes. This is the most expensive operation
  // during homormophic evaluation, so it should be thoroughly optimized.

  vector<DoubleCRT> polyDigits;
  p.breakIntoDigits(polyDigits, nDigits);

  // Finally we multiply the vector of digits by the key-switching matrix

  // An object to hold the pseudorandom ai's, note that it must be defined
  // with the maximum number of levels, else the PRG will go out of synch.
  // FIXME: This is a bug waiting to happen.
  DoubleCRT ai(context);

  // Set the first ai using the seed, subsequent ai's (if any) will
  // use the evolving RNG state (NOTE: this is not thread-safe)

  RandomState state;
  SetSeed(W.prgSeed);

  // Add the columns in, one by one
  DoubleCRT tmp(context, IndexSet::emptySet());
  
  for (unsigned long i=0; i<polyDigits.size(); i++) {
    ai.randomize();
    tmp = polyDigits[i];
  
    // The operations below all use the IndexSet of tmp
  
    // add part*a[i] with a handle pointing to base of W.toKeyID
    tmp.Mul(ai,  /*matchIndexSet=*/false);
    addPart(tmp, SKHandle(1,1,W.toKeyID), /*matchPrimeSet=*/true);
  
    // add part*b[i] with a handle pointing to one
    polyDigits[i].Mul(W.b[i], /*matchIndexSet=*/false);
    addPart(polyDigits[i], SKHandle(), /*matchPrimeSet=*/true);
  }
  noiseVar += addedNoise;
  FHE_TIMER_STOP;
} // restore random state upon destruction of the RandomState, see NumbTh.h

// Find the IndexSet such that modDown to that set of primes makes the
// additive term due to rounding into the dominant noise term 
void Ctxt::findBaseSet(IndexSet& s) const
{
  if (getNoiseVar()<=0.0) { // an empty ciphertext
    s = context.ctxtPrimes;
    return;
  }

  assert(verifyPrimeSet());
  bool halfSize = context.containsSmallPrime();
  double addedNoise = log(modSwitchAddedNoiseVar())/2;
  double curNoise = log(getNoiseVar())/2;
  double firstNoise = context.logOfPrime(0);

  // remove special primes, if they are included in this->primeSet
  s = getPrimeSet();
  if (!s.disjointFrom(context.specialPrimes)) { 
    // scale down noise
    curNoise -= context.logOfProduct(context.specialPrimes);
    s.remove(context.specialPrimes);
  }

  if (curNoise<=2*addedNoise) return; // no need to mod down

  // if the first prime in half size, begin by removing it
  if (halfSize && s.contains(0)) {
    curNoise -= firstNoise;
    s.remove(0);
  }

  // while noise is larger than added term, scale down by the next prime
  while (curNoise>addedNoise && card(s)>1) {
    curNoise -= context.logOfPrime(s.last());
    s.remove(s.last());
  }

  if (halfSize) {
    // If noise is still too big, drop last big prime and insert half-size prime
    if (curNoise>addedNoise) {
      curNoise = firstNoise;
      s = IndexSet(0);
    } 
    // Otherwise check if you can add back the half-size prime
    else if (curNoise+firstNoise <= addedNoise) {
      curNoise += firstNoise;
      s.insert(0);
    }
  }

  if (curNoise>addedNoise && log_of_ratio()>-0.5)
    cerr << "Ctxt::findBaseSet warning: already at lowest level\n";
}


/********************************************************************/
// Ciphertext arithmetic

// Add/subtract a ciphertext part to a ciphertext.
// With negative=true we subtract, otherwise we add.
void Ctxt::addPart(const DoubleCRT& part, const SKHandle& handle, 
		   bool matchPrimeSet, bool negative)
{
  FHE_TIMER_START;
  assert (&part.getContext() == &context);

  // add to the the prime-set of *this, if needed (this is expensive)
  if (matchPrimeSet && !(part.getIndexSet() <= primeSet)) {
    IndexSet setDiff = part.getIndexSet() / primeSet; // set minus
    for (size_t i=0; i<parts.size(); i++) parts[i].addPrimes(setDiff);
    primeSet.insert(setDiff);
  }

  if (parts.size()==0) { // inserting 1st part 
    primeSet = part.getIndexSet();
    parts.push_back(CtxtPart(part,handle));
    if (negative) parts.back().Negate(); // not thread-safe??
  } else {               // adding to a ciphertext with existing parts
    assert(part.getIndexSet() <= primeSet);  // Sanity check

    DoubleCRT tmp(context, IndexSet::emptySet());
    const DoubleCRT* ptr = &part;

    // mod-UP the part if needed
    IndexSet s = primeSet / part.getIndexSet();
    if (!empty(s)) { // if need to mod-UP, do it on a temporary copy
      tmp = part;
      tmp.addPrimesAndScale(s);
      ptr = &tmp;
    }
    long j = getPartIndexByHandle(handle);
    if (j>=0) { // found a matching part, add them up
      if (negative) parts[j] -= *ptr;
      else          parts[j] += *ptr;
    } else {    // no mathing part found, just append this part
      parts.push_back(CtxtPart(*ptr,handle));
      if (negative) parts.back().Negate(); // not thread-safe??
    }
  }
  FHE_TIMER_STOP;
}

// Add a constant polynomial
void Ctxt::addConstant(const DoubleCRT& dcrt, double size)
{
  FHE_TIMER_START;
  // If the size is not given, we use the default value phi(m)*(ptxtSpace/2)^2
  if (size < 0.0) {
    // WARNING: the following line is written just so to prevent overflow
    size = ((double) context.zMStar.getPhiM()) * ptxtSpace*ptxtSpace /4.0;
  }

  // Scale the constant, then add it to the part that points to one
  long f = (ptxtSpace>2)? rem(context.productOfPrimes(primeSet),ptxtSpace): 1;
  if (f!=1) {
    DoubleCRT tmp = dcrt;
    tmp *= f;
    addPart(tmp, SKHandle(0,1,0));
  }
  else addPart(dcrt, SKHandle(0,1,0));

  noiseVar += (size*f)*f;
  FHE_TIMER_STOP;
}

// Add a constant polynomial
void Ctxt::addConstant(const ZZ& c)
{
  DoubleCRT dcrt(getContext(), getPrimeSet());
  long cc = rem(c, ptxtSpace); // reduce modulo plaintext space
  dcrt = cc;

  if (cc > ptxtSpace/2) cc -= ptxtSpace;
  double size = to_double(cc);

  addConstant(dcrt, size*size);
}

void Ctxt::negate()
{
  FHE_TIMER_START;
  for (size_t i=0; i<parts.size(); i++) parts[i].Negate();
  FHE_TIMER_STOP;
}

// Add/subtract another ciphertxt (depending on the negative flag)
void Ctxt::addCtxt(const Ctxt& other, bool negative)
{
  FHE_TIMER_START;
  // Sanity check: same context and public key
  assert (&context==&other.context && &pubKey==&other.pubKey);

  // Special case: if *this is empty then just copy other
  if (this->isEmpty()) {
    *this = other;
    if (negative) negate();
    return;
  }

  // Sanity check: verify that the plaintext spaces are compatible
  long g = GCD(this->ptxtSpace, other.ptxtSpace);
  assert (g>1);
  this->ptxtSpace = g;

  // Match the prime-sets, mod-UP the arguments if needed
  IndexSet s = other.primeSet / primeSet; // set-minus
  if (!empty(s)) modUpToSet(s);

  const Ctxt* other_pt = &other;
  Ctxt tmp(pubKey, other.ptxtSpace); // a temporaty empty ciphertext

  s = primeSet / other.primeSet; // set-minus
  if (!empty(s)) { // need to mod-UP the other, use a temporary copy
    tmp = other;
    tmp.modUpToSet(s);
    other_pt = &tmp;
  }

  // Go over the parts of other, for each one check if
  // there is a matching part in *this
  for (size_t i=0; i<other_pt->parts.size(); i++) {
    const CtxtPart& part = other_pt->parts[i];
    long j = getPartIndexByHandle(part.skHandle);
    if (j>=0) { // found a matching part, add them up
      if (negative) parts[j] -= part;
      else          parts[j] += part;
    } else {    // no mathing part found, just append this part
      parts.push_back(part);
      if (negative) parts.back().Negate(); // not thread safe??
    }
  }
  noiseVar += other_pt->noiseVar;
  FHE_TIMER_STOP;
}

// Create a tensor product of c1,c2. It is assumed that *this,c1,c2
// are defined relative to the same set of primes and plaintext space,
// and that *this DOES NOT point to the same object as c1,c2
void Ctxt::tensorProduct(const Ctxt& c1, const Ctxt& c2)
{
  // c1,c2 may be scaled, so multiply by the inverse scalar if needed
  long f = 1;
  if (c1.ptxtSpace>2) 
    f = rem(context.productOfPrimes(c1.primeSet),c1.ptxtSpace);
  if (f!=1) f = InvMod(f,c1.ptxtSpace);

  clear();                // clear *this, before we start adding things to it
  primeSet = c1.primeSet; // set the correct prime-set before we begin

  // The actual tensoring
  CtxtPart tmpPart(context, IndexSet::emptySet()); // a scratch CtxtPart
  for (size_t i=0; i<c1.parts.size(); i++) {
    CtxtPart thisPart = c1.parts[i];
    if (f!=1) thisPart *= f;
    for (size_t j=0; j<c2.parts.size(); j++) {
      tmpPart = c2.parts[j];
      // What secret key will the product point to?
      if (!tmpPart.skHandle.mul(thisPart.skHandle, tmpPart.skHandle))
	Error("Ctxt::tensorProduct: cannot multiply secret-key handles");

      tmpPart *= thisPart; // The element of the tensor product

      // Check if we already have a part relative to this secret-key handle
      long k = getPartIndexByHandle(tmpPart.skHandle);
      if (k >= 0) // found a matching part
	parts[k] += tmpPart;
      else
	parts.push_back(tmpPart);
    }
  }

  /* Compute the noise estimate as c1.noiseVar * c2.noiseVar * factor
   * where the factor depends on the handles of c1,c2. Specifically,
   * if the largest powerOfS in c1,c2 are n1,n2, respectively, then we
   * have factor = ((n1+n2) choose n2).
   */
  long n1=0,  n2=0;
  for (size_t i=0; i<c1.parts.size(); i++) // get largest powerOfS in c1
    if (c1.parts[i].skHandle.getPowerOfS() > n1)
      n1 = c1.parts[i].skHandle.getPowerOfS();
  for (size_t i=0; i<c2.parts.size(); i++) // get largest powerOfS in c2
    if (c2.parts[i].skHandle.getPowerOfS() > n2)
      n2 = c2.parts[i].skHandle.getPowerOfS();

  // compute ((n1+n2) choose n2)
  long factor = 1;
  for (long i=n1+1; i<=n1+n2; i++) factor *= i;
  for (long i=n2  ; i>1     ; i--) factor /= i;

  noiseVar = c1.noiseVar * c2.noiseVar * factor;
  if (f!=1) {
    // WARNING: the following line is written just so to prevent overflow
    noiseVar = (noiseVar*f)*f; // because every product was scaled by f
  }
}

Ctxt& Ctxt::operator*=(const Ctxt& other)
{
  FHE_TIMER_START;
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return  *this;

  // Sanity check: plaintext spaces are compatible
  long g = GCD(ptxtSpace, other.ptxtSpace);
  assert (g>1);
  this->ptxtSpace = g;
  Ctxt tmpCtxt(this->pubKey, this->ptxtSpace); // a scratch ciphertext

  if (this == &other) { // a squaring operation
    modDownToLevel(findBaseLevel());      // mod-down if needed
    tmpCtxt.tensorProduct(*this, other);  // compute the actual product
    tmpCtxt.noiseVar *= 2;     // a correction factor due to dependency
  }
  else {                // standard multiplication between two ciphertexts
    // Sanity check: same context and public key
    assert (&context==&other.context && &pubKey==&other.pubKey);

    // Match the levels, mod-DOWN the arguments if needed
    long lvl = findBaseLevel();
    long otherLvl = other.findBaseLevel();
    if (lvl > otherLvl) lvl = otherLvl; // the smallest of the two

    // mod-DOWN *this, if needed (also removes special primes, if any)
    modDownToLevel(lvl);

    // mod-DOWN other, if needed
    if (primeSet!=other.primeSet){ // use temporary copy to mod-DOWN other
      Ctxt tmpCtxt1 = other;
      tmpCtxt1.modDownToLevel(lvl);
      tmpCtxt.tensorProduct(*this,tmpCtxt1); // compute the actual product
    }
    else 
      tmpCtxt.tensorProduct(*this, other);   // compute the actual product
  }
  *this = tmpCtxt; // copy the result into *this

  FHE_TIMER_STOP;
  return *this;
}

// Higher-level multiply routines that include also modulus-switching
// and re-linearization

void Ctxt::multiplyBy(const Ctxt& other)
{
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  *this *= other;  // perform the multiplication
  reLinearize();   // re-linearize
}

void Ctxt::multiplyBy2(const Ctxt& other1, const Ctxt& other2)
{
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  long lvl = findBaseLevel();
  long lvl1 = other1.findBaseLevel();
  long lvl2 = other2.findBaseLevel();

  if (lvl<lvl1 && lvl<lvl2){ // if both others at higher levels than this,
    Ctxt tmp = other1;       // multiply others by each other, then by this
    if (&other1 == &other2) tmp *= tmp; // squaring rather than multiplication
    else                    tmp *= other2;

    *this *= tmp;
  }
  else if (lvl<lvl2) { // lvl1<=lvl<lvl2, multiply by other2, then by other1
    *this *= other2;
    *this *= other1;    
  }
  else { // multiply first by other1, then by other2
    *this *= other1;
    *this *= other2;
  }
  reLinearize(); // re-linearize after all the multiplications
}

// Multiply-by-constant
void Ctxt::multByConstant(const ZZ& c)
{
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  long cc = rem(c, ptxtSpace); // reduce modulo plaintext space
  ZZ tmp = to_ZZ(cc);

  // multiply all the parts by this constant
  for (size_t i=0; i<parts.size(); i++) parts[i] *= tmp;

  if (cc > ptxtSpace/2) cc -= ptxtSpace;
  double size = to_double(cc);
  noiseVar *= size*size;
}

// Multiply-by-constant
void Ctxt::multByConstant(const DoubleCRT& dcrt, double size)
{
  FHE_TIMER_START;
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

   // If the size is not given, we use the default value phi(m)*ptxtSpace^2/2
  if (size < 0.0) {
    // WARNING: the following line is written just so to prevent overflow
    size = ((double) context.zMStar.getPhiM()) * ptxtSpace * (ptxtSpace /4.0);
  }

  // multiply all the parts by this constant
  for (size_t i=0; i<parts.size(); i++) 
    parts[i].Mul(dcrt,/*matchIndexSets=*/false);

  noiseVar *= size;

  FHE_TIMER_STOP;
}

// Divide a cipehrtext by 2. It is assumed that the ciphertext
// encrypts an even polynomial and has plaintext space 2^r for r>1.
// As a side-effect, the plaintext space is halved from 2^r to 2^{r-1}
// If these assumptions are not met then the result will not be a
// valid ciphertext anymore.
void Ctxt::divideBy2()
{
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;
  assert (ptxtSpace % 2 == 0 && ptxtSpace>2);

  // multiply all the parts by (productOfPrimes+1)/2
  ZZ twoInverse; // set to (Q+1)/2
  getContext().productOfPrimes(twoInverse, getPrimeSet());
  twoInverse += 1;
  twoInverse /= 2;
  for (size_t i=0; i<parts.size(); i++)
    parts[i] *= twoInverse;

  noiseVar /= 4;  // noise is halved by this operation
  ptxtSpace /= 2; // and so is the plaintext space
}

// This function assumes that the slots of c contains integers mod 2^r
// (i.e., that only the free terms are nonzero). It computes for each slot
// the AND (product) of all the bits in the inetegr in that slot. That is,
// the end result has 1 in some slot iff the content of that slot was 2^r-1.
void Ctxt::extractBits(vector<Ctxt>& bits, long r)
{
  assert (getContext().zMStar.getP()==2);
  if (r<=0 || r>getContext().alMod.getR()) // how many bits to extract
    r = getContext().alMod.getR();

  Ctxt tmp(getPubKey(), getPtxtSpace());
  bits.resize(r, tmp);      // allocate space
  vector<Ctxt> w(r-1, tmp);
  w[0] = *this;
  for (long i=1; i<r; i++) {
    tmp = *this;
    for (long j=0; j<i; j++) {
      w[j].square();
      tmp -= w[j];
      tmp.divideBy2();
    }
    bits[i] = tmp; // bits[i]=i'th lowest bit in the slots of c mod 2^{r-i}
    if (i<r-1) w[i] = tmp; // needed in the next round
  }
}

void Ctxt::automorph(long k) // Apply automorphism F(X)->F(X^k) (gcd(k,m)=1)
{
  FHE_TIMER_START;
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  // Sanity check: verify that k \in Zm*
  assert (context.zMStar.inZmStar(k));
  long m = context.zMStar.getM();

  // Apply this automorphism to all the parts
  for (size_t i=0; i<parts.size(); i++) { 
    parts[i].automorph(k);
    if (!parts[i].skHandle.isOne()) {
      parts[i].skHandle.powerOfX = MulMod(parts[i].skHandle.powerOfX,k,m);
    }
  }
  // no change in noise variance
  FHE_TIMER_STOP;
}


// Apply F(X)->F(X^k) followed by re-liearization. The automorphism is possibly
// evaluated via a sequence of steps, to ensure that we can re-linearize the
// result of every step.
void Ctxt::smartAutomorph(long k) 
{
  FHE_TIMER_START;
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  long m = context.zMStar.getM();

  k = mcMod(k, m);

  // Sanity check: verify that k \in Zm*
  assert (context.zMStar.inZmStar(k));

  long keyID=getKeyID();
  if (!inCanonicalForm(keyID)) {     // Re-linearize the input, if needed
    reLinearize(keyID);
    assert (inCanonicalForm(keyID)); // ensure that re-linearization succeeded
  }
  assert (pubKey.isReachable(k,keyID)); // reachable from 1

  while (k != 1) {
    const KeySwitch& matrix = pubKey.getNextKSWmatrix(k,keyID);
    long amt = matrix.fromKey.getPowerOfX();

    automorph(amt);
    reLinearize(keyID);

    k = MulMod(k, InvMod(amt,m), m);
  }
  FHE_TIMER_STOP;
}



// applies the Frobenius automorphism p^j
void Ctxt::frobeniusAutomorph(long j) 
{
  // Special case: if *this is empty then do nothing
  if (this->isEmpty()) return;

  long m = context.zMStar.getM();
  long p = context.zMStar.getP();
  long d = context.zMStar.getOrdP();

  j = mcMod(j, d);
  long val = PowerMod(p, j, m);
  smartAutomorph(val);
}


/********************************************************************/
// Utility methods

const long Ctxt::getKeyID() const
{
  for (size_t i=0; i<parts.size(); i++)
    if (!parts[i].skHandle.isOne()) return parts[i].skHandle.getSecretKeyID();

  return -1;
}

// Estimates the added noise variance from mod-switching down
xdouble Ctxt::modSwitchAddedNoiseVar() const
{
  xdouble addedNoise = to_xdouble(0.0);

  // incorporate the secret keys' Hamming-weight
  for (size_t i=0; i<parts.size(); i++) {
    if (parts[i].skHandle.isOne()) {
      addedNoise += 1.0;
    }
    else {
      long keyId = parts[i].skHandle.getSecretKeyID();
      long d = parts[i].skHandle.getPowerOfS();
      xdouble h, t;
      h = pubKey.getSKeyWeight(keyId);

      // added noise is d! h^d
      t = h;
      for (long j = 2; j <= d; j++)
        t = t * h * j;

      addedNoise += t;
    }
  }
  // WARNING: the following line is written just so to prevent overflow
  addedNoise = (addedNoise * context.zMStar.getPhiM()) 
               * ptxtSpace * (ptxtSpace/ 12.0);

  return addedNoise;
}

istream& operator>>(istream& str, SKHandle& handle)
{
  //  cerr << "SKHandle[";
  seekPastChar(str,'['); // defined in NumbTh.cpp
  str >> handle.powerOfS;
  str >> handle.powerOfX;
  str >> handle.secretKeyID;
  seekPastChar(str,']');
  //  cerr << "]";  
  return str;
}

ostream& operator<<(ostream& str, const CtxtPart& p)
{
  return str << "[" << ((const DoubleCRT&)p) << endl 
	     << p.skHandle << "]"; 
}

istream& operator>>(istream& str, CtxtPart& p)
{
  //  cerr << "CtxtPart[";
  seekPastChar(str,'['); // defined in NumbTh.cpp
  str >> (DoubleCRT&) p;
  str >> p.skHandle;
  seekPastChar(str,']');
  //  cerr << "]";
  return str;
}

ostream& operator<<(ostream& str, const Ctxt& ctxt)
{
  str << "["<<ctxt.ptxtSpace<<" "<<ctxt.noiseVar<<" "<<ctxt.primeSet
      << " "<<ctxt.parts.size() << endl;
  for (size_t i=0; i<ctxt.parts.size(); i++)
    str << ctxt.parts[i] << endl;
  return str << "]";
}

istream& operator>>(istream& str, Ctxt& ctxt)
{
  //  cerr << "Ctxt[";
  seekPastChar(str,'['); // defined in NumbTh.cpp
  str >> ctxt.ptxtSpace >> ctxt.noiseVar >> ctxt.primeSet;
  long nParts;
  str >> nParts;
  ctxt.parts.resize(nParts, CtxtPart(ctxt.context,IndexSet::emptySet()));
  for (long i=0; i<nParts; i++) {
    str >> ctxt.parts[i];
    assert (ctxt.parts[i].getIndexSet()==ctxt.primeSet); // sanity-check
  }
  seekPastChar(str,']');
  //  cerr << "]";
  return str;
}


void CheckCtxt(const Ctxt& c, const char* label)
{
  cerr << "  "<<label << ", level=" << c.findBaseLevel() << ", log(noise/modulus)~" << c.log_of_ratio() << endl;
}

// The recursive incremental-product function that does the actual work
static void recursiveIncrementalProduct(Ctxt array[], long n)
{
  if (n <= 1) return; // nothing to do

  // split the array in two, first part is the highest power of two smaller
  // than n and second part is the rest

  long ell = NTL::NumBits(n-1); // 2^{l-1} <= n-1
  long n1 = 1UL << (ell-1);     // n/2 <= n1 = 2^l < n

  // Call the recursive procedure separately on the first and second parts
  recursiveIncrementalProduct(array, n1);
  recursiveIncrementalProduct(&array[n1], n-n1);

  // Multiply the last product in the 1st part into every product in the 2nd
  for (long i=n1; i<n; i++) array[i].multiplyBy(array[n1-1]);
}

// For i=n-1...0, set v[i]=prod_{j<=i} v[j]
// This implementation uses depth log n and (nlog n)/2 products
void incrementalProduct(vector<Ctxt>& v)
{
  long n = v.size();  // how many ciphertexts do we have
  if (n > 0) recursiveIncrementalProduct(&v[0], n); // do the actual work
}

// Compute the inner product of two vectors of ciphertexts, this routine uses
// the lower-level *= operator and does only one re-linearization at the end.
void innerProduct(Ctxt& result, const vector<Ctxt>& v1, const vector<Ctxt>& v2)
{
  long n = min(v1.size(), v2.size());
  if (n<=0) {
    result.clear();
    return;
  }
  result = v1[0]; result *= v2[0];
  for (long i=1; i<n; i++) {
    Ctxt tmp = v1[i];
    tmp *= v2[i];
    result += tmp;
  }
  result.reLinearize();
}

// Compute the inner product of a ciphertext vector and a constant vector
void innerProduct(Ctxt& result,
		  const vector<Ctxt>& v1, const vector<DoubleCRT>& v2)
{
  long n = min(v1.size(), v2.size());
  if (n<=0) {
    result.clear();
    return;
  }
  result = v1[0]; result.multByConstant(v2[0]);
  for (long i=1; i<n; i++) {
    Ctxt tmp = v1[i];
    tmp.multByConstant(v2[i]);
    result += tmp;
  }
}

void innerProduct(Ctxt& result,
		  const vector<Ctxt>& v1, const vector<ZZX>& v2)
{
  long n = min(v1.size(), v2.size());
  if (n<=0) {
    result.clear();
    return;
  }
  result = v1[0]; result.multByConstant(v2[0]);
  for (long i=1; i<n; i++) {
    Ctxt tmp = v1[i];
    tmp.multByConstant(v2[i]);
    result += tmp;
  }
}


