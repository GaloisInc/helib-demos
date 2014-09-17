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
/**
 * @file polyEval.cpp
 * @brief Homomorphic Polynomial Evaluation
 */
#include "polyEval.h"

#ifdef DEBUG_PRINTOUT
static FHESecKey* sk_pt = NULL;      // global pointers for debugging purposes
static EncryptedArray* ea_pt = NULL;

static void checkPolyEval(const Ctxt& out, const Ctxt& in, const ZZX& poly)
{
  long p = in.getPtxtSpace();

  // decrypt input and output
  vector< long > in_v;
  vector< long > out_v;
  ea_pt->decrypt(in, *sk_pt, in_v);
  ea_pt->decrypt(out, *sk_pt, out_v);
  for (long i=0; i<ea_pt->size(); i++)
    if (out_v[i] != polyEvalMod(poly, in_v[i], p)) {
	cerr << "Error: poly="<< poly 
	     << ", p="  << p
	     << ", in=" << in_v[i]
	     << ", out="<< out_v[i] <<endl;
	exit(0);
      }

  cerr << "Eval("<<poly<<") succeeded, out[0]="<<out_v[0]<<endl;
}

static void printSlots(const Ctxt& c)
{
  // decrypt input and output
  vector< long > v;
  ea_pt->decrypt(c, *sk_pt, v);
  cerr << "[";
  for (long i=0; i<ea_pt->size(); i++)
    cerr << v[i]<<" ";
  cerr << "]";
}
#endif // DEBUG_PRINTOUT

// Returns the e'th power, computing it as needed
Ctxt& DynamicCtxtPowers::getPower(long e)
{
  if (v.at(e-1).isEmpty()) { // Not computed yet, compute it now
    
    long k = 1L<<(NextPowerOfTwo(e)-1); // largest power of two smaller than e
    v[e-1] = getPower(e-k);             // compute X^e = X^{e-k} * X^k
    v[e-1].multiplyBy(getPower(k));

    IndexSet s;                         // mod-switch down to base set
    v[e-1].findBaseSet(s);
    v[e-1].modDownToSet(s);
  }
  return v[e-1];
}

// Simple evaluation sum f_i * X^i, assuming that babyStep has enough powers
static void 
simplePolyEval(Ctxt& ret, const ZZX& poly, DynamicCtxtPowers& babyStep)
{
  ret.clear();
  if (deg(poly)<0) return;       // the zero polynomial always returns zero

  assert (deg(poly)<=babyStep.size()); // ensure that we have enough powers

  ZZ coef;
  ZZ p = to_ZZ(babyStep[0].getPtxtSpace());
  for (long i=1; i<=deg(poly); i++) {
    rem(coef, coeff(poly,i),p);
    if (coef > p/2) coef -= p;

    Ctxt tmp = babyStep.getPower(i); // X^i
    tmp.multByConstant(coef);        // f_i X^i
    ret += tmp;
  }
  // Add the free term
  rem(coef, ConstTerm(poly), p);
  if (coef > p/2) coef -= p;
  ret.addConstant(coef);
  //  if (verbose) checkPolyEval(ret, babyStep[0], poly);
}


// The recursive procedure in the Paterson-Stockmeyer polynomial-evaluation
// algorithm from SIAM J. on Computing, 1973.
// This procedure assumes that poly is monic, deg(poly)=k*(2t-1)+delta
// with t=2^e, and that babyStep contains k+delta powers
static void
PatersonStockmeyer(Ctxt& ret, const ZZX& poly, long k, long t, long delta,
		   DynamicCtxtPowers& babyStep, DynamicCtxtPowers& giantStep)
{
  //  if (verbose) cerr << "PatersonStockmeyer("<<poly<<"), t="<<t<<endl;
  if (deg(poly)<=babyStep.size()) { // Edge condition, use simple eval
    simplePolyEval(ret, poly, babyStep);
    return;
  }
  ZZX r = trunc(poly, k*t);      // degree <= k*2^e-1
  ZZX q = RightShift(poly, k*t); // degree == k(2^e-1) +delta

  const ZZ p = to_ZZ(babyStep[0].getPtxtSpace());
  const ZZ& coef = coeff(r,deg(q));
  SetCoeff(r, deg(q), coef-1);  // r' = r - X^{deg(q)}

  ZZX c,s;
  DivRem(c,s,r,q); // r' = c*q + s
  // deg(s)<deg(q), and if c!= 0 then deg(c)<k-delta

  assert(deg(s)<deg(q));
  assert(IsZero(c) || deg(c)<k-delta);
  SetCoeff(s,deg(q)); // s' = s + X^{deg(q)}, deg(s)==deg(q)

  // reduce the coefficients modulo mod
  for (long i=0; i<=deg(c); i++) rem(c[i],c[i], p);
  c.normalize();
  for (long i=0; i<=deg(s); i++) rem(s[i],s[i], p);
  s.normalize();

  // Evaluate recursively poly = (c+X^{kt})*q + s'
  PatersonStockmeyer(ret, q, k, t/2, delta, babyStep, giantStep);

  Ctxt tmp(ret.getPubKey(), ret.getPtxtSpace());
  simplePolyEval(tmp, c, babyStep);
  tmp += giantStep.getPower(t);
  ret.multiplyBy(tmp);

  PatersonStockmeyer(tmp, s, k, t/2, delta, babyStep, giantStep);
  ret += tmp;
  //  if (verbose) checkPolyEval(ret, babyStep[0], poly);
}

// This procedure assumes that k*(2^e +1) > deg(poly) > k*(2^e -1),
// and that babyStep contains k+ (deg(poly) mod k) powers
static void
degPowerOfTwo(Ctxt& ret, const ZZX& poly, long k,
	      DynamicCtxtPowers& babyStep, DynamicCtxtPowers& giantStep)
{
  if (deg(poly)<=babyStep.size()) { // Edge condition, use simple eval
    simplePolyEval(ret, poly, babyStep);
    return;
  }
  long n = deg(poly)/k;        // We assume n=2^e or n=2^e -1
  n = 1L << NextPowerOfTwo(n); // round up to n=2^e
  ZZX r = trunc(poly, (n-1)*k);      // degree <= k(2^e-1)-1
  ZZX q = RightShift(poly, (n-1)*k); // 0 < degree < 2k
  SetCoeff(r, (n-1)*k);              // monic, degree == k(2^e-1)
  q -= 1;

  PatersonStockmeyer(ret, r, k, n/2, 0,	babyStep, giantStep);

  Ctxt tmp(ret.getPubKey(), ret.getPtxtSpace());
  simplePolyEval(tmp, q, babyStep); // evaluate q

  // multiply by X^{k(n-1)} with minimum depth
  for (long i=1; i<n; i*=2) {  
    tmp.multiplyBy(giantStep.getPower(i));
  }
  ret += tmp;
  //  if (verbose) checkPolyEval(ret, babyStep[0], poly);
}

static void 
recursivePolyEval(Ctxt& ret, const ZZX& poly, long k,
		  DynamicCtxtPowers& babyStep, DynamicCtxtPowers& giantStep)
{
  //  if (verbose) cerr << "recursivePolyEval("<<poly<<")"<<endl;
  if (deg(poly)<=babyStep.size()) { // Edge condition, use simple eval
    simplePolyEval(ret, poly, babyStep);
    return;
  }

  long delta = deg(poly) % k; // deg(poly) mod k
  long n = divc(deg(poly),k); // ceil( deg(poly)/k )
  long t = 1L<<(NextPowerOfTwo(n)); // t >= n, so t*k >= deg(poly)

  // Special case for deg(poly) = k * 2^e +delta
  if (n==t) {
    degPowerOfTwo(ret, poly, k, babyStep, giantStep);
    return;
  }

  // When deg(poly) = k*(2^e -1) we use the Paterson-Stockmeyer recursion
  if (n == t-1 && delta==0) {
    PatersonStockmeyer(ret, poly, k, t/2, delta, babyStep, giantStep);
    return;
  }

  t = t/2;

  // In any other case we have kt < deg(poly) < k(2t-1). We then set 
  // u = deg(poly) - k*(t-1) and poly = q*X^u + r with deg(r)<u
  // and recurse on poly = (q-1)*X^u + (X^u+r)

  long u = deg(poly) - k*(t-1);
  ZZX r = trunc(poly, u);      // degree <= u-1
  ZZX q = RightShift(poly, u); // degree == k*(t-1)
  q -= 1;
  SetCoeff(r, u);              // degree == u

  PatersonStockmeyer(ret, q, k, t/2, 0, babyStep, giantStep);

  Ctxt tmp = giantStep.getPower(u/k);
  if (delta!=0) { // if u is not divisible by k then compute it
    tmp.multiplyBy(babyStep.getPower(delta));
  }
  ret.multiplyBy(tmp);

  recursivePolyEval(tmp, r, k, babyStep, giantStep);
  ret += tmp;
  //  if (verbose) checkPolyEval(ret, babyStep[0], poly);
}


//! @brief Evaluate a cleartext polynomial on an encrypted input
void polyEval(Ctxt& ret, ZZX poly, const Ctxt& x, long k)
     // Note: poly is passed by value, so caller keeps the original
{
  //  if (verbose) cerr << "polyEval("<<poly<<")"<<endl;
  if (deg(poly)<=2) { // nothing to optimize here
    if (deg(poly)<1) {
      ret.clear();
      ret.addConstant(coeff(poly, 0));
    } else {
      DynamicCtxtPowers babyStep(x, deg(poly));
      simplePolyEval(ret, poly, babyStep);
    }
    return;
  }

  // How many baby steps: set k~sqrt(n/2), rounded up/down to a power of two

  // FIXME: There may be some room for optimization here: it may be possible
  // to choose k as something other than a power of two and still maintain
  // optimal depth, in principle we can try all possible values of k between
  // the two powers of two and choose the one that goves the least number
  // of multiplies, conditioned on minimum depth.

  if (k<=0) {
    long kk = (long) sqrt(deg(poly)/2.0);
    k = 1L << NextPowerOfTwo(kk);

    // heuristic: if k>>kk then use a smaler power of two
    if ((k==16 && deg(poly)>167) || (k>16 && k>(1.44*kk)))
      k /= 2;
  }
  cerr << "  k="<<k;

  long n = divc(deg(poly),k);          // deg(p) = k*n +delta
  //  if (verbose) cerr << ", n="<<n<<endl;

  DynamicCtxtPowers babyStep(x, k);
  const Ctxt& x2k = babyStep.getPower(k);

  // Special case when deg(p)>k*(2^e -1)
  if (n==(1L << NextPowerOfTwo(n))) { // n is a power of two
    DynamicCtxtPowers giantStep(x2k, n/2);
    degPowerOfTwo(ret, poly, k, babyStep, giantStep);
    return;
  }

  // If n is not a power of two, ensure that poly is monic and that
  // its degree is divisible by k, then call the recursive procedure

  const ZZ p = to_ZZ(x.getPtxtSpace());
  ZZ top = LeadCoeff(poly);
  ZZ topInv; // the inverse mod p of the top coefficient of poly (if any)
  bool divisible = (n*k == deg(poly)); // is the degree divisible by k?
  long nonInvertibe = InvModStatus(topInv, top, p);
       // 0 if invertible, 1 if not

  // FIXME: There may be some room for optimization below: instead of
  // adding a term X^{n*k} we can add X^{n'*k} for some n'>n, so long
  // as n' is smaller than the next power of two. We could save a few
  // multiplications since giantStep[n'] may be easier to compute than
  // giantStep[n] when n' has fewer 1's than n in its binary expansion.

  ZZ extra = ZZ::zero();    // extra!=0 denotes an added term extra*X^{n*k}
  if (!divisible || nonInvertibe) {  // need to add a term
    top = to_ZZ(1);  // new top coefficient is one
    topInv = top;    // also the new inverse is one
    // set extra = 1 - current-coeff-of-X^{n*k}
    extra = SubMod(top, coeff(poly,n*k), p);
    SetCoeff(poly, n*k); // set the top coefficient of X^{n*k} to one
  }

  long t = IsZero(extra)? divc(n,2) : n;
  DynamicCtxtPowers giantStep(x2k, t);

  if (!IsOne(top)) {
    // if (verbose) cerr << "multiplying polynomial by "<<topInv<<endl;
    poly *= topInv; // Multiply by topInv to make into a monic polynomial
    for (long i=0; i<=n*k; i++) rem(poly[i], poly[i], p);
    poly.normalize();
  }
  recursivePolyEval(ret, poly, k, babyStep, giantStep);

  if (!IsOne(top)) {
    // if (verbose) cerr << "multiplying result by "<<top<<endl;
    ret.multByConstant(top);
  }

  if (!IsZero(extra)) { // if we added a term, now is the time to subtract back
    Ctxt topTerm = giantStep.getPower(n);
    topTerm.multByConstant(extra);
    ret -= topTerm;
  }
}











#if 0
/**********************************************************************/
/*     FOR DEBUGGING PURPOSES, the same procedure for plaintext x     */
/**********************************************************************/

static long verbose = 0;
class DynamicPtxtPowers {
private:
  long p;  // the modulus
  vec_long v;    // A vector storing the powers themselves
  vec_long dpth; // keeping track of depth for debugging purposes

public:
  DynamicPtxtPowers(long _x, long _p, long nPowers, long _d=1) : p(_p)
  {
    assert (_x>=0 && _p>1 && nPowers>0); // Sanity check
    v.SetLength(nPowers);
    dpth.SetLength(nPowers);
    for (long i=1; i<nPowers; i++) // Initializes nPowers empty slots
      dpth[i]=v[i]=-1;

    v[0] = _x % p;         // store X itself in v[0]
    dpth[0] = _d;
    //    cerr << " initial power="<<v[0]<<", initial depth="<<dpth[0];
  }

  //! @brief Returns the e'th power, computing it as needed
  long getPower(long e); // must use e >= 1, else throws an exception

  //! dp.at(i) and dp[i] both return the i+1st power
  long at(long i) { return getPower(i+1); }
  long operator[](long i) { return getPower(i+1); }

  const vec_long& getVector() const { return v; }
  long size() const { return v.length(); }
  bool wasComputed(long i) { return (i>=0 && i<v.length() && v[i]>=0); }
  long getDepth(long e) { return (e>0)? dpth.at(e-1) : 0; }
};

// Returns the e'th power, computing it as needed
long DynamicPtxtPowers::getPower(long e)
{
  // FIXME: Do we want to allow the vector to grow? If so then begin by
  // checking e<v.length() and resizing if not. Currently throws an exception.

  if (v.at(e-1)<0) { // Not computed yet, compute it now
    
    long k = 1L<<(NextPowerOfTwo(e)-1); // largest power of two smaller than e

    v[e-1] = getPower(e-k);             // compute X^e = X^{e-k} * X^k
    v[e-1] = MulMod(v[e-1], getPower(k), p);
    dpth[e-1] = max(getDepth(k),getDepth(e-k)) +1;
    nMults++;
  }
  return v[e-1];
}

ostream& operator<< (ostream &s, const DynamicPtxtPowers &d)
{
  return s << d.getVector();
}

static long
recursivePolyEval(const ZZX& poly, long k, DynamicPtxtPowers& babyStep,
		  DynamicPtxtPowers& giantStep, long mod,
		  long& recursiveDepth);
static long 
PatersonStockmeyer(const ZZX& poly, long k, long t, long delta,
		   DynamicPtxtPowers& babyStep, DynamicPtxtPowers& giantStep,
		   long mod, long& recursiveDepth);

long simplePolyEval(const ZZX& poly, DynamicPtxtPowers& babyStep, long mod)
{
  assert (deg(poly)<=(long)babyStep.size());// ensure that we have enough powers

  long ret = rem(ConstTerm(poly), mod);
  for (long i=0; i<deg(poly); i++) {
    long coeff = rem(poly[i+1], mod); // f_{i+1}
    long tmp = babyStep[i];           // X^{i+1}
    tmp = MulMod(tmp, coeff, mod);    // f_{i+1} X^{i+1}
    ret = AddMod(ret, tmp, mod);
  }
  return ret;
}

// This procedure assumes that k*(2^e +1) > deg(poly) > k*(2^e -1),
// and that babyStep contains k+ (deg(poly) mod k) powers
static long degPowerOfTwo(const ZZX& poly, long k, DynamicPtxtPowers& babyStep,
			  DynamicPtxtPowers& giantStep, long mod,
			  long& recursiveDepth)
{
  if (deg(poly)<=babyStep.size()) { // Edge condition, use simple eval
    long ret = simplePolyEval(poly, babyStep, mod);
    recursiveDepth = babyStep.getDepth(deg(poly));
    return ret;
  }
  long subDepth1 =0, subDepth2=0;
  long n = deg(poly)/k;        // We assume n=2^e or n=2^e -1
  n = 1L << NextPowerOfTwo(n); // round up to n=2^e
  ZZX r = trunc(poly, (n-1)*k);      // degree <= k(2^e-1)-1
  ZZX q = RightShift(poly, (n-1)*k); // 0 < degree < 2k
  SetCoeff(r, (n-1)*k);              // monic, degree == k(2^e-1)
  q -= 1;
  if (verbose) cerr << ", recursing on "<<r<<" + X^"<<(n-1)*k<<"*"<<q<<endl;

  long ret = PatersonStockmeyer(r, k, n/2, 0,
				babyStep, giantStep, mod, subDepth2);
  if (verbose)
    cerr << "  PatersonStockmeyer("<<r<<") returns "<<ret
	 << ", depth="<<subDepth2<<endl;

  long tmp = simplePolyEval(q, babyStep, mod); // evaluate q
  subDepth1 = babyStep.getDepth(deg(q));
  if (verbose)
    cerr << "  simplePolyEval("<<q<<") returns "<<tmp
	 << ", depth="<<subDepth1<<endl;

  // multiply by X^{k(n-1)} with minimum depth
  for (long i=1; i<n; i*=2) {  
    tmp = MulMod(tmp, giantStep.getPower(i), mod);
    nMults++;
    subDepth1 = max(subDepth1, giantStep.getDepth(i)) +1;
    if (verbose)
      cerr << "    after mult by giantStep.getPower("<<i<< ")="
	   << giantStep.getPower(i)<<" of depth="<< giantStep.getDepth(i)
	   << ",  ret="<<tmp<<" and depth is "<<subDepth1<<endl;
  }
  totalDepth = max(subDepth1, subDepth2);
  return AddMod(ret, tmp, mod); // return q * X^{k(n-1)} + r
}

// This procedure assumes that poly is monic, deg(poly)=k*(2t-1)+delta
// with t=2^e, and that babyStep contains k+delta powers
static long 
PatersonStockmeyer(const ZZX& poly, long k, long t, long delta,
		   DynamicPtxtPowers& babyStep, DynamicPtxtPowers& giantStep,
		   long mod, long& recursiveDepth)
{
  if (verbose) cerr << "PatersonStockmeyer("<<poly<<"), k="<<k<<", t="<<t<<endl;

  if (deg(poly)<=babyStep.size()) { // Edge condition, use simple eval
    long ret = simplePolyEval(poly, babyStep, mod);
    recursiveDepth = babyStep.getDepth(deg(poly));
    return ret;
  }
  long subDepth1=0, subDepth2=0;
  long ret, tmp;

  ZZX r = trunc(poly, k*t);      // degree <= k*2^e-1
  ZZX q = RightShift(poly, k*t); // degree == k(2^e-1) +delta

  if (verbose) cerr << "  r ="<<r<< ", q="<<q;

  const ZZ& coef = coeff(r,deg(q));
  SetCoeff(r, deg(q), coef-1);  // r' = r - X^{deg(q)}

  if (verbose) cerr << ", r'="<<r;

  ZZX c,s;
  DivRem(c,s,r,q); // r' = c*q + s
  // deg(s)<deg(q), and if c!= 0 then deg(c)<k-delta

  if (verbose) cerr << ", c="<<c<< ", s ="<<s<<endl;

  assert(deg(s)<deg(q));
  assert(IsZero(c) || deg(c)<k-delta);
  SetCoeff(s,deg(q)); // s' = s + X^{deg(q)}, deg(s)==deg(q)

  // reduce the coefficients modulo mod
  for (long i=0; i<=deg(c); i++) rem(c[i],c[i], to_ZZ(mod));
  c.normalize();
  for (long i=0; i<=deg(s); i++) rem(s[i],s[i], to_ZZ(mod));
  s.normalize();

  if (verbose) cerr << " {t==n+1} recursing on "<<s<<" + (X^"<<t*k<<"+"<<c<<")*"<<q<<endl;

  // Evaluate recursively poly = (c+X^{kt})*q + s'
  tmp = simplePolyEval(c, babyStep, mod);
  tmp = AddMod(tmp, giantStep.getPower(t), mod);
  subDepth1 = max(babyStep.getDepth(deg(c)), giantStep.getDepth(t));

  ret = PatersonStockmeyer(q, k, t/2, delta,
			   babyStep, giantStep, mod, subDepth2);

  if (verbose) {
    cerr << "  PatersonStockmeyer("<<q<<") returns "<<ret
	 << ", depth="<<subDepth2<<endl;
    if (ret != polyEvalMod(q,babyStep[0], mod)) {
      cerr << "  **1st recursive call failed, q="<<q<<endl;
      exit(0);
    }
  }
  ret = MulMod(ret, tmp, mod);
  nMults++;
  subDepth1 = max(subDepth1, subDepth2)+1;

  tmp = PatersonStockmeyer(s, k, t/2, delta,
			   babyStep, giantStep, mod, subDepth2);
  if (verbose) {
    cerr << "  PatersonStockmeyer("<<s<<") returns "<<tmp
	 << ", depth="<<subDepth2<<endl;
    if (tmp != polyEvalMod(s,babyStep[0], mod)) {
      cerr << "  **2nd recursive call failed, s="<<s<<endl;
      exit(0);
    }
  }
  ret = AddMod(ret,tmp,mod);
  recursiveDepth = max(subDepth1, subDepth2);
  return ret;
}


// This procedure assumes that poly is monic and that babyStep contains
// at least k+delta powers, where delta = deg(poly) mod k
static long 
recursivePolyEval(const ZZX& poly, long k, DynamicPtxtPowers& babyStep,
		  DynamicPtxtPowers& giantStep, long mod,
		  long& recursiveDepth)
{
  if (deg(poly)<=babyStep.size()) { // Edge condition, use simple eval
    long ret = simplePolyEval(poly, babyStep, mod);
    recursiveDepth = babyStep.getDepth(deg(poly));
    return ret;
  }

  if (verbose) cerr << "recursivePolyEval("<<poly<<")\n";

  long delta = deg(poly) % k; // deg(poly) mod k
  long n = divc(deg(poly),k); // ceil( deg(poly)/k )
  long t = 1L<<(NextPowerOfTwo(n)); // t >= n, so t*k >= deg(poly)

  // Special case for deg(poly) = k * 2^e +delta
  if (n==t)
    return degPowerOfTwo(poly, k, babyStep, giantStep, mod, recursiveDepth);

  // When deg(poly) = k*(2^e -1) we use the Paterson-Stockmeyer recursion
  if (n == t-1 && delta==0)
    return PatersonStockmeyer(poly, k, t/2, delta,
			      babyStep, giantStep, mod, recursiveDepth);

  t = t/2;

  // In any other case we have kt < deg(poly) < k(2t-1). We then set 
  // u = deg(poly) - k*(t-1) and poly = q*X^u + r with deg(r)<u
  // and recurse on poly = (q-1)*X^u + (X^u+r)

  long u = deg(poly) - k*(t-1);
  ZZX r = trunc(poly, u);      // degree <= u-1
  ZZX q = RightShift(poly, u); // degree == k*(t-1)
  q -= 1;
  SetCoeff(r, u);              // degree == u

  long ret, tmp;
  long subDepth1=0, subDepth2=0;
  if (verbose) 
    cerr << " {deg(poly)="<<deg(poly)<<"<k*(2t-1)="<<k*(2*t-1)
	 << "} recursing on "<<r<<" + X^"<<u<<"*"<<q<<endl;
  ret = PatersonStockmeyer(q, k, t/2, 0, 
			   babyStep, giantStep, mod, subDepth1);

  if (verbose) {
    cerr << "  PatersonStockmeyer("<<q<<") returns "<<ret<<", depth="<<subDepth1<<endl;
    if (ret != polyEvalMod(q,babyStep[0], mod)) {
      cerr << "  @@1st recursive call failed, q="<<q
     	   << ", ret="<<ret<<"!=" << polyEvalMod(q,babyStep[0], mod)<<endl;
      exit(0);
    }
  }

  tmp = giantStep.getPower(u/k);
  subDepth2 = giantStep.getDepth(u/k);
  if (delta!=0) { // if u is not divisible by k then compute it
    if (verbose) 
      cerr <<"  multiplying by X^"<<u
	   <<"=giantStep.getPower("<<(u/k)<<")*babyStep.getPower("<<delta<<")="
	   << giantStep.getPower(u/k)<<"*"<<babyStep.getPower(delta)
	   << "="<<tmp<<endl;
    tmp = MulMod(tmp, babyStep.getPower(delta), mod);
    nMults++;
    subDepth2++;
  }
  ret = MulMod(ret, tmp, mod);
  nMults ++;
  subDepth1 = max(subDepth1, subDepth2)+1;

  if (verbose) cerr << "  after mult by X^{k*"<<u<<"+"<<delta<<"}, depth="<< subDepth1<<endl;

  tmp = recursivePolyEval(r, k, babyStep, giantStep, mod, subDepth2);
  if (verbose)
    cerr << "  recursivePolyEval("<<r<<") returns "<<tmp<<", depth="<<subDepth2<<endl;
  if (tmp != polyEvalMod(r,babyStep[0], mod)) {
    cerr << "  @@2nd recursive call failed, r="<<r
	 << ", ret="<<tmp<<"!=" << polyEvalMod(r,babyStep[0], mod)<<endl;
    exit(0);
  }
  recursiveDepth = max(subDepth1, subDepth2);
  return AddMod(ret, tmp, mod);
}

// Note: poly is passed by value, not by reference, so the calling routine
// keeps its original polynomial
long evalPolyTopLevel(ZZX poly, long x, long p, long k=0)
{
  if (verbose)
  cerr << "\n* evalPolyTopLevel: p="<<p<<", x="<<x<<", poly="<<poly;

  if (deg(poly)<=2) { // nothing to optimize here
    if (deg(poly)<1) return to_long(coeff(poly, 0));
    DynamicPtxtPowers babyStep(x, p, deg(poly));
    long ret = simplePolyEval(poly, babyStep, p);
    totalDepth = babyStep.getDepth(deg(poly));
    return ret;
  }

  // How many baby steps: set k~sqrt(n/2), rounded up/down to a power of two

  // FIXME: There may be some room for optimization here: it may be possible
  // to choose k as something other than a power of two and still maintain
  // optimal depth, in principle we can try all possible values of k between
  // the two powers of two and choose the one that goves the least number
  // of multiplies, conditioned on minimum depth.

  if (k<=0) {
    long kk = (long) sqrt(deg(poly)/2.0);
    k = 1L << NextPowerOfTwo(kk);

    // heuristic: if k>>kk then use a smaler power of two
    if ((k==16 && deg(poly)>167) || (k>16 && k>(1.44*kk)))
      k /= 2;
  }
  cerr << ", k="<<k;

  long n = divc(deg(poly),k);          // deg(p) = k*n +delta
  if (verbose) cerr << ", n="<<n<<endl;

  DynamicPtxtPowers babyStep(x, p, k);
  long x2k = babyStep.getPower(k);

  // Special case when deg(p)>k*(2^e -1)
  if (n==(1L << NextPowerOfTwo(n))) { // n is a power of two
    DynamicPtxtPowers giantStep(x2k, p, n/2, babyStep.getDepth(k));
    if (verbose)
      cerr << "babyStep="<<babyStep<<", giantStep="<<giantStep<<endl;
    long ret = degPowerOfTwo(poly, k, babyStep, giantStep, p, totalDepth);

    if (verbose) {
      cerr << "  degPowerOfTwo("<<poly<<") returns "<<ret<<", depth="<<totalDepth<<endl;
      if (ret != polyEvalMod(poly,babyStep[0], p)) {
	cerr << "  ## recursive call failed, ret="<<ret<<"!=" 
	     << polyEvalMod(poly,babyStep[0], p)<<endl;
	exit(0);
      }
      // cerr << "  babyStep depth=[";
      // for (long i=0; i<babyStep.size(); i++) 
      // 	cerr << babyStep.getDepth(i+1)<<" ";
      // cerr << "]\n";
      // cerr << "  giantStep depth=[";
      // for (long i=0; i<giantStep.size(); i++)
      // 	cerr<<giantStep.getDepth(i+1)<<" ";
      // cerr << "]\n";
    }
    return ret;
  }

  // If n is not a power of two, ensure that poly is monic and that
  // its degree is divisible by k, then call the recursive procedure

  ZZ topInv; // the inverse mod p of the top coefficient of poly (if any)
  bool divisible = (n*k == deg(poly)); // is the degree divisible by k?
  long nonInvertibe = InvModStatus(topInv, LeadCoeff(poly), to_ZZ(p));
       // 0 if invertible, 1 if not

  // FIXME: There may be some room for optimization below: instead of
  // adding a term X^{n*k} we can add X^{n'*k} for some n'>n, so long
  // as n' is smaller than the next power of two. We could save a few
  // multiplications since giantStep[n'] may be easier to compute than
  // giantStep[n] when n' has fewer 1's than n in its binary expansion.

  long extra = 0;        // extra!=0 denotes an added term extra*X^{n*k}
  if (!divisible || nonInvertibe) {  // need to add a term
    // set extra = 1 - current-coeff-of-X^{n*k}
    extra = SubMod(1, to_long(coeff(poly,n*k)), p);
    SetCoeff(poly, n*k); // set the top coefficient of X^{n*k} to one
    topInv = to_ZZ(1);   // inverse of new top coefficient is one
  }

  long t = (extra==0)? divc(n,2) : n;
  DynamicPtxtPowers giantStep(x2k, p, t, babyStep.getDepth(k));

  if (verbose)
    cerr << "babyStep="<<babyStep<<", giantStep="<<giantStep<<endl;

  long y; // the value to return
  long subDepth1 =0;
  if (!IsOne(topInv)) {
    long top = to_long(poly[n*k]); // record the current top coefficient
    //    cerr << ", top-coeff="<<top;

    // Multiply by topInv modulo p to make into a monic polynomial
    poly *= topInv;
    for (long i=0; i<=n*k; i++) rem(poly[i], poly[i], to_ZZ(p));
    poly.normalize();

    y = recursivePolyEval(poly, k, babyStep, giantStep, p, subDepth1);
    if (verbose) {
      cerr << "  recursivePolyEval("<<poly<<") returns "<<y<<", depth="<<subDepth1<<endl;
      if (y != polyEvalMod(poly,babyStep[0], p)) {
	cerr << "## recursive call failed, ret="<<y<<"!=" 
	     << polyEvalMod(poly,babyStep[0], p)<<endl;
	exit(0);
      }
    }
    y = MulMod(y, top, p); // multiply by the original top coefficient
  }
  else {
    y = recursivePolyEval(poly, k, babyStep, giantStep, p, subDepth1);
    if (verbose) {
      cerr << "  recursivePolyEval("<<poly<<") returns "<<y<<", depth="<<subDepth1<<endl;
      if (y != polyEvalMod(poly,babyStep[0], p)) {
	cerr << "## recursive call failed, ret="<<y<<"!=" 
	     << polyEvalMod(poly,babyStep[0], p)<<endl;
	exit(0);
      }
    }
  }

  if (extra != 0) { // if we added a term, now is the time to subtract back
    if (verbose) cerr << ", subtracting "<<extra<<"*X^"<<k*n;
    extra = MulMod(extra, giantStep.getPower(n), p);
    totalDepth = max(subDepth1, giantStep.getDepth(n));
    y = SubMod(y, extra, p);
  }
  else totalDepth = subDepth1;
  if (verbose) cerr << endl;
  return y;
}
#endif
