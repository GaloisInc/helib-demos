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
#include "NumbTh.h"

#include <fstream>
#include <cassert>
#include <cctype>

using namespace std;


// Code for parsing command line

bool parseArgs(int argc,  char *argv[], argmap_t& argmap)
{
  for (long i = 1; i < argc; i++) {
    char *x = argv[i];
    long j = 0;
    while (x[j] != '=' && x[j] != '\0') j++; 
    if (x[j] == '\0') return false;
    string arg(x, j);
    if (argmap[arg] == NULL) return false;
    argmap[arg] = x+j+1;
  }

  return true;
}

// Mathematically correct mod and div, avoids overflow
long mcMod(long a, long b) 
{
   long r = a % b;

   if (r != 0 && (b < 0) != (r < 0))
      return r + b;
   else
      return r;

}

long mcDiv(long a, long b) {

   long r = a % b;
   long q = a / b;

   if (r != 0 && (b < 0) != (r < 0))
      return q + 1;
   else
      return q;
}


// return multiplicative order of p modulo m, or 0 if GCD(p, m) != 1
long multOrd(long p, long m)
{
  if (GCD(p, m) != 1) return 0;

  p = p % m;
  long ord = 1;
  long val = p; 
  while (val != 1) {
    ord++;
    val = MulMod(val, p, m);
  }
  return ord;
}


// return a degree-d irreducible polynomial mod p
ZZX makeIrredPoly(long p, long d)
{
	assert(d >= 1);
  assert(ProbPrime(p));

  if (d == 1) return ZZX(1, 1); // the monomial X

  zz_pBak bak; bak.save();
  zz_p::init(p);
  return to_ZZX(BuildIrred_zz_pX(d));
}


// Factoring by trial division, only works for N<2^{60}.
// Only the primes are recorded, not their multiplicity
template<class zz> static void factorT(vector<zz> &factors, const zz &N)
{
  factors.resize(0); // reset the factors

  if (N<2) return;   // sanity check

  PrimeSeq s;
  zz n = N;
  while (true) {
    if (ProbPrime(n)) { // we are left with just a single prime
      factors.push_back(n);
      return;
    }
    // if n is a composite, check if the next prime divides it
    long p = s.next();
    if ((n%p)==0) {
      zz pp;
      conv(pp,p);
      factors.push_back(pp);
      do { n /= p; } while ((n%p)==0);
    }
    if (n==1) return;
  }
}
void factorize(vector<long> &factors, long N) { factorT<long>(factors, N);}
void factorize(vector<ZZ> &factors, const ZZ& N) {factorT<ZZ>(factors, N);}

void factorize(Vec< Pair<long, long> > &factors, long N)
{
  factors.SetLength(0);

  if (N < 2) return;

  PrimeSeq s;
  long n = N;
  while (n > 1) {
    if (ProbPrime(n)) {
      append(factors, cons(n, 1L));
      return;
    }

    long p = s.next();
    if ((n % p) == 0) {
      long e = 1;
      n = n/p;
      while ((n % p) == 0) {
        n = n/p;
        e++;
      }
      append(factors, cons(p, e));
    }
  }
}

template<class zz> static void phiNT(zz &phin, vector<zz> &facts, const zz &N)
{
  if (facts.size()==0) factorize(facts,N);

  zz n = N;
  conv(phin,1); // initialize phiN=1
  for (unsigned long i=0; i<facts.size(); i++) {
    zz p = facts[i];
    phin *= (p-1); // first factor of p
    for (n /= p; (n%p)==0; n /= p) phin *= p; // multiple factors of p
  } 
}
// Specific template instantiations for long and ZZ
void phiN(long &pN, vector<long> &fs, long N)  { phiNT<long>(pN,fs,N); }
void phiN(ZZ &pN, vector<ZZ> &fs, const ZZ &N) { phiNT<ZZ>(pN,fs,N);   }

/* Compute Phi(N) */
long phi_N(long N)
{
  long phiN=1,p,e;
  PrimeSeq s;
  while (N!=1)
    { p=s.next();
      e=0;
      while ((N%p)==0) { N=N/p; e++; }
      if (e!=0)
        { phiN=phiN*(p-1)*power_long(p,e-1); }
    }
  return phiN;
}

// finding e-th root of unity modulo the current modulus
// VJS: rewritten to be both faster and deterministic,
//  and assumes that current modulus is prime

template<class zp,class zz> void FindPrimRootT(zp &root, unsigned long e)
{
  zz qm1 = zp::modulus()-1;

  assert(qm1 % e == 0);
  
  vector<long> facts;
  factorize(facts,e); // factorization of e

  root = 1;

  for (unsigned long i = 0; i < facts.size(); i++) {
    long p = facts[i];
    long pp = p;
    long ee = e/p;
    while (ee % p == 0) {
      ee = ee/p;
      pp = pp*p;
    }
    // so now we have e = pp * ee, where pp is 
    // the power of p that divides e.
    // Our goal is to find an element of order pp

    PrimeSeq s;
    long q;
    zp qq, qq1;
    long iter = 0;
    do {
      iter++;
      if (iter > 1000000) 
        Error("FindPrimitiveRoot: possible infinite loop?");
      q = s.next();
      conv(qq, q);
      power(qq1, qq, qm1/p);
    } while (qq1 == 1);
    power(qq1, qq, qm1/pp); // qq1 has order pp

    mul(root, root, qq1);
  }

  // independent check that we have an e-th root of unity 
  {
    zp s;

    power(s, root, e);
    if (s != 1) Error("FindPrimitiveRoot: internal error (1)");

    // check that s^{e/p} != 1 for any prime divisor p of e
    for (unsigned long i=0; i<facts.size(); i++) {
      long e2 = e/facts[i];
      power(s, root, e2);   // s = root^{e/p}
      if (s == 1) 
        Error("FindPrimitiveRoot: internal error (2)");
    }
  }
}
// instantiations of the template
void FindPrimitiveRoot(zz_p &r, unsigned long e){FindPrimRootT<zz_p,long>(r,e);}
void FindPrimitiveRoot(ZZ_p &r, unsigned long e){FindPrimRootT<ZZ_p,ZZ>(r,e);}

/* Compute mobius function (naive method as n is small) */
long mobius(long n)
{
  long p,e,arity=0;
  PrimeSeq s;
  while (n!=1)
    { p=s.next();
      e=0;
      while ((n%p==0)) { n=n/p; e++; }
      if (e>1) { return 0; }
      if (e!=0) { arity^=1; }
    }     
  if (arity==0) { return 1; }
  return -1;
}

/* Compute cyclotomic polynomial */
ZZX Cyclotomic(long N)
{
  ZZX Num,Den,G,F;
  set(Num); set(Den);
  long m,d;
  for (d=1; d<=N; d++)
    { if ((N%d)==0)
         { clear(G);
           SetCoeff(G,N/d,1); SetCoeff(G,0,-1);
           m=mobius(d);
           if (m==1)       { Num*=G; }
           else if (m==-1) { Den*=G; }
         }
    } 
  F=Num/Den;
  return F;
}

/* Find a primitive root modulo N */
long primroot(long N,long phiN)
{
  long g=2,p;
  PrimeSeq s;
  bool flag=false;

  while (flag==false)
    { flag=true;
      s.reset(1);
      do
        { p=s.next();
          if ((phiN%p)==0)
            { if (PowerMod(g,phiN/p,N)==1)
                { flag=false; }
            }
        }
      while (p<phiN && flag);
      if (flag==false) { g++; }
    }
  return g;
}

long ord(long N,long p)
{
  long o=0;
  while ((N%p)==0)
    { o++;
      N/=p;
    }
  return o;
}

ZZX RandPoly(long n,const ZZ& p)
{ 
  ZZX F; F.SetMaxLength(n);
  ZZ p2;  p2=p>>1;
  for (long i=0; i<n; i++)
    { SetCoeff(F,i,RandomBnd(p)-p2); }
  return F;
}

/* When q=2 maintains the same sign as the input */
void PolyRed(ZZX& out, const ZZX& in, const ZZ& q, bool abs)
{
  // ensure that out has the same degree as in
  out.SetMaxLength(deg(in)+1);               // allocate space if needed
  if (deg(out)>deg(in)) trunc(out,out,deg(in)+1); // remove high degrees

  ZZ q2; q2=q>>1;
  for (long i=0; i<=deg(in); i++)
    { ZZ c=coeff(in,i);
      c %= q;
      if (abs) {
        if (c<0) c += q;
      } 
      else if (q!=2) {
        if (c>q2)  { c=c-q; }
          else if (c<-q2) { c=c+q; }
      }
      else // q=2
        { if (sign(coeff(in,i))!=sign(c))
	    { c=-c; }
        }
      SetCoeff(out,i,c);
    }
}

void PolyRed(ZZX& out, const ZZX& in, long q, bool abs)
{
  // ensure that out has the same degree as in
  out.SetMaxLength(deg(in)+1);               // allocate space if needed
  if (deg(out)>deg(in)) trunc(out,out,deg(in)+1); // remove high degrees

  long q2; q2=q>>1;
  for (long i=0; i<=deg(in); i++)
    { long c=coeff(in,i)%q;
      if (abs)
        { if (c<0) { c=c+q; } }
      else if (q==2)
        { if (coeff(in,i)<0) { c=-c; } }
      else
        { if (c>=q2)  { c=c-q; }
          else if (c<-q2) { c=c+q; }
	}
      SetCoeff(out,i,c);
    }
}

// multiply the polynomial f by the integer a modulo q
void MulMod(ZZX& out, const ZZX& f, long a, long q, bool abs/*default=true*/)
{
  // ensure that out has the same degree as f
  out.SetMaxLength(deg(f)+1);               // allocate space if needed
  if (deg(out)>deg(f)) trunc(out,out,deg(f)+1); // remove high degrees

  for (long i=0; i<=deg(f); i++) { 
    long c = rem(coeff(f,i), q);
    c = MulMod(c, a, q); // returns c \in [0,q-1]
    if (!abs && c >= q/2)
      c -= q;
    SetCoeff(out,i,c);
  }
}

long is_in(long x,int* X,long sz)
{
  for (long i=0; i<sz; i++)
    { if (x==X[i]) { return i; } }
  return -1;
}

/* Incremental integer CRT for vectors. Expects co-primes p>0,q>0 with q odd,
 * and such that all the entries in vp are in [-p/2,p/2) and all entries in
 * vq are in [0,q-1). Returns in vp the CRT of vp mod p and vq mod q, as
 * integers in [-pq/2, pq/2). Uses the formula:
 *
 *   CRT(vp,p,vq,q) = vp + p*[ (vq-vp)*p^{-1} ]_q
 *
 * where [...]_q means reduction to the interval [-q/2,q/2). As q is odd then
 * this is the same as reducing to [-(q-1)/2,(q-1)/2], hence [...]_q * p is
 * in [-p(q-1)/2, p(q-1)/2], and since vp is in [-p/2,p/2) then the sum is
 * indeed in [-pq/2,pq/2).
 *
 * Returns true if both vectors are of the same length, false otherwise
 */
template <class zzvec>
bool intVecCRT(vec_ZZ& vp, const ZZ& p, const zzvec& vq, long q)
{
  long pInv = InvMod(rem(p,q), q); // p^{-1} mod q
  long n = min(vp.length(),vq.length());
  long q_over_2 = q/2;
  ZZ tmp;
  long vqi;
  for (long i=0; i<n; i++) {
    conv(vqi, vq[i]); // convert to single precision
    long vq_minus_vp_mod_q = SubMod(vqi, rem(vp[i],q), q);

    long delta_times_pInv = MulMod(vq_minus_vp_mod_q, pInv, q);
    if (delta_times_pInv > q_over_2) delta_times_pInv -= q;

    mul(tmp, delta_times_pInv, p); // tmp = [(vq_i-vp_i)*p^{-1}]_q * p
    vp[i] += tmp;
  }
  // other entries (if any) are 0 mod q
  for (long i=vq.length(); i<vp.length(); i++) {
    long minus_vp_mod_q = NegateMod(rem(vp[i],q), q);

    long delta_times_pInv = MulMod(minus_vp_mod_q, pInv, q);
    if (delta_times_pInv > q_over_2) delta_times_pInv -= q;

    mul(tmp, delta_times_pInv, p); // tmp = [(vq_i-vp_i)*p^{-1}]_q * p
    vp[i] += tmp;
  }
  return (vp.length()==vq.length());
}
// specific instantiations: vq can be vec_long or vec_ZZ
template bool intVecCRT(vec_ZZ&, const ZZ&, const vec_ZZ&, long);
template bool intVecCRT(vec_ZZ&, const ZZ&, const vec_long&, long);

// MinGW hack
#ifndef lrand48
#if defined(__MINGW32__) || defined(WIN32)
#define drand48() (((double)rand()) / RAND_MAX)
#define lrand48() rand()
#endif
#endif

void sampleHWt(ZZX &poly, long Hwt, long n)
{
  if (n<=0) n=deg(poly)+1; if (n<=0) return;
  clear(poly);          // initialize to zero
  poly.SetMaxLength(n); // allocate space for degree-(n-1) polynomial

  long b,u,i=0;
  if (Hwt>n) Hwt=n;
  while (i<Hwt) {  // continue until exactly Hwt nonzero coefficients
    u=lrand48()%n; // The next coefficient to choose
    if (IsZero(coeff(poly,u))) { // if we didn't choose it already
      b = lrand48()&2; // b random in {0,2}
      b--;             //   random in {-1,1}
      SetCoeff(poly,u,b);

      i++; // count another nonzero coefficient
    }
  }
  poly.normalize(); // need to call this after we work on the coeffs
}

void sampleSmall(ZZX &poly, long n)
{
  if (n<=0) n=deg(poly)+1; if (n<=0) return;
  poly.SetMaxLength(n); // allocate space for degree-(n-1) polynomial

  for (long i=0; i<n; i++) {    // Chosse coefficients, one by one
    long u = lrand48();
    if (u&1) {                 // with prob. 1/2 choose between -1 and +1
      u = (u & 2) -1;
      SetCoeff(poly, i, u);
    }
    else SetCoeff(poly, i, 0); // with ptob. 1/2 set to 0
  }
  poly.normalize(); // need to call this after we work on the coeffs
}

void sampleGaussian(ZZX &poly, long n, double stdev)
{
  static double const Pi=4.0*atan(1.0); // Pi=3.1415..
  static long const bignum = 0xfffffff;

  if (n<=0) n=deg(poly)+1; if (n<=0) return;
  poly.SetMaxLength(n); // allocate space for degree-(n-1) polynomial
  for (long i=0; i<n; i++) SetCoeff(poly, i, ZZ::zero());

  // Uses the Box-Muller method to get two Normal(0,stdev^2) variables
  for (long i=0; i<n; i+=2) {
    double r1 = (1+RandomBnd(bignum))/((double)bignum+1);
    double r2 = (1+RandomBnd(bignum))/((double)bignum+1);
    double theta=2*Pi*r1;
    double rr= sqrt(-2.0*log(r2))*stdev;

    assert(rr < 8*stdev); // sanity-check, no more than 8 standard deviations

    // Generate two Gaussians RV's, rounded to integers
    long x = (long) floor(rr*cos(theta) +0.5);
    SetCoeff(poly, i, x);
    if (i+1 < n) {
      x = (long) floor(rr*sin(theta) +0.5);
      SetCoeff(poly, i+1, x);
    }
  }
  poly.normalize(); // need to call this after we work on the coeffs
}

void sampleUniform(ZZX& poly, const ZZ& B, long n)
{
  if (n<=0) n=deg(poly)+1; if (n<=0) return;
  if (B <= 0) {
    clear(poly);
    return;
  }

  poly.SetMaxLength(n); // allocate space for degree-(n-1) polynomial

  ZZ UB, tmp;

  UB =  2*B + 1;
  for (long i = 0; i < n; i++) {
    RandomBnd(tmp, UB);
    tmp -= B; 
    poly.rep[i] = tmp;
  }

  poly.normalize();
}



// ModComp: a pretty lame implementation

void ModComp(ZZX& res, const ZZX& g, const ZZX& h, const ZZX& f)
{
  assert(LeadCoeff(f) == 1);

  ZZX hh = h % f;
  ZZX r = to_ZZX(0);

  for (long i = deg(g); i >= 0; i--) 
    r = (r*hh + coeff(g, i)) % f; 

  res = r;
}

long polyEvalMod(const ZZX& poly, long x, long p)
{
  long ret = 0;
  for (long i=deg(poly); i>=0; i--) {
    long coeff = rem(poly[i], p);
    ret = AddMod(ret, coeff, p);      // Add the coefficient of x^i
    if (i>0) ret = MulMod(ret, x, p); // then mult by x
  }
  return ret;
}

// Interpolate the integer polynomial such that poly(x[i] mod p)=y[i] (mod p^e)
// It is assumed that the points x[i] are all distinct modulo p
void interpolateMod(ZZX& poly, const vec_long& x, const vec_long& y,
		    long p, long e)
{
  poly = ZZX::zero();       // initialize to zero
  long p2e = power_long(p,e); // p^e

  vec_long ytmp(INIT_SIZE, y.length()); // A temporary writable copy
  for (long j=0; j<y.length(); j++) ytmp[j] = y[j];

  zz_pBak bak; bak.save();    // Set the current modulus to p
  zz_p::init(p);

  vec_zz_p xmod(INIT_SIZE, x.length()); // convert to zz_p
  for (long j=0; j<x.length(); j++) xmod[j] = to_zz_p(x[j] % p);

  long p2i = 1; // p^i
  for (long i=0; i<e; i++, p2i*=p) {// gradually lift the result to mod p^i
    vec_zz_p ymod(INIT_SIZE, y.length());

    // convert to zz_p
    for (long j=0; j<y.length(); j++) ymod[j] = to_zz_p(ytmp[j] % p);

    // a polynomial p_i s.t. p_i(x[j]) = i'th p-base digit of poly(x[j])
    zz_pX polyMod;
    interpolate(polyMod, xmod, ymod);    // interpolation modulo p

    // cerr <<"digit "<<i<<"="<<ymod<<", poly="<<polyMod <<endl<<std::flush;

    ZZX polyTmp; conv(polyTmp, polyMod); // convert to ZZX

    // update ytmp by subtracting the new digit, then dividing by p
    for (long j=0; j<y.length(); j++) {
      ytmp[j] -= polyEvalMod(polyTmp,rep(xmod[j]),p2e/p2i); // mod p^{e-i}
      if (ytmp[j]<0) ytmp[j] += p2e/p2i;
      ytmp[j] /= p;
    } // maybe it's worth optimizing above by using multi-point evaluation

    // add the new digit to the result
    polyTmp *= p2i;
    poly += polyTmp;
  }
}

ZZ largestCoeff(const ZZX& f)
{
  ZZ mx = ZZ::zero();
  for (long i=0; i<=deg(f); i++) {
    if (mx < abs(coeff(f,i)))
      mx = abs(coeff(f,i));
  }
  return mx;
}

ZZ sumOfCoeffs(const ZZX& f) // = f(1)
{
  ZZ sum = ZZ::zero();
  for (long i=0; i<=deg(f); i++) sum += coeff(f,i);
  return sum;
}

xdouble coeffsL2Norm(const ZZX& f) // l_2 norm
{
  xdouble s = to_xdouble(0.0);
  for (long i=0; i<=deg(f); i++) {
    xdouble coef = to_xdouble(coeff(f,i));
    s += coef * coef;
  }
  return sqrt(s);
}

// advance the input stream beyond white spaces and a single instance of cc
void seekPastChar(istream& str, int cc)
{
   int c = str.get();
   while (isspace(c)) c = str.get();
   if (c != cc) {
     std::cerr << "Searching for cc='"<<(char)cc<<"' (ascii "<<cc<<")"
	       << ", found c='"<<(char)c<<"' (ascii "<<c<<")\n";
     exit(1);
   }
}

// stuff added relating to linearized polynomials and support routines

// Builds the matrix defining the linearized polynomial transformation.
//
// NTL's current smallint modulus, zz_p::modulus(), is assumed to be p^r,
// for p prime, r >= 1 integer.
//
// After calling this function, one can call ppsolve(C, L, M, p, r) to get
// the coeffecients C for the linearized polynomial represented the linear
// map defined by its action on the standard basis for zz_pE over zz_p:
// for i = 0..zz_pE::degree()-1: x^i -> L[i], where x = (X mod zz_pE::modulus())

void buildLinPolyMatrix(mat_zz_pE& M, long p)
{
   long d = zz_pE::degree();

   M.SetDims(d, d);

   for (long j = 0; j < d; j++) 
      conv(M[0][j], zz_pX(j, 1));

   for (long i = 1; i < d; i++)
      for (long j = 0; j < d; j++)
         M[i][j] = power(M[i-1][j], p);
}

void buildLinPolyMatrix(mat_GF2E& M, long p)
{
   assert(p == 2);

   long d = GF2E::degree();

   M.SetDims(d, d);

   for (long j = 0; j < d; j++) 
      conv(M[0][j], GF2X(j, 1));

   for (long i = 1; i < d; i++)
      for (long j = 0; j < d; j++)
         M[i][j] = power(M[i-1][j], p);
}

// some auxilliary conversion routines

void convert(vec_zz_pE& X, const vector<ZZX>& A)
{
   long n = A.size();
   zz_pX tmp;
   X.SetLength(n);
   for (long i = 0; i < n; i++) {
      conv(tmp, A[i]);
      conv(X[i], tmp); 
   }
} 

void convert(mat_zz_pE& X, const vector< vector<ZZX> >& A)
{
   long n = A.size();

   if (n == 0) {
      long m = X.NumCols();
      X.SetDims(0, m);
      return;
   }

   long m = A[0].size();
   X.SetDims(n, m);

   for (long i = 0; i < n; i++)
      convert(X[i], A[i]);
}

void convert(vector<ZZX>& X, const vec_zz_pE& A)
{
   long n = A.length();
   X.resize(n);
   for (long i = 0; i < n; i++)
      conv(X[i], rep(A[i]));
}

void convert(vector< vector<ZZX> >& X, const mat_zz_pE& A)
{
   long n = A.NumRows();
   X.resize(n);
   for (long i = 0; i < n; i++)
      convert(X[i], A[i]);
}

void mul(vector<ZZX>& x, const vector<ZZX>& a, long b)
{
   long n = a.size();
   x.resize(n);
   for (long i = 0; i < n; i++) 
      mul(x[i], a[i], b);
}

void div(vector<ZZX>& x, const vector<ZZX>& a, long b)
{
   long n = a.size();
   x.resize(n);
   for (long i = 0; i < n; i++) 
      div(x[i], a[i], b);
}

void add(vector<ZZX>& x, const vector<ZZX>& a, const vector<ZZX>& b)
{
   long n = a.size();
   if (n != (long) b.size()) Error("add: dimension mismatch");
   for (long i = 0; i < n; i++)
      add(x[i], a[i], b[i]);
}

// prime power solver
// zz_p::modulus() is assumed to be p^r, for p prime, r >= 1
// A is an n x n matrix, b is a length n (row) vector,
// and a solution for the matrix-vector equation x A = b is found.
// If A is not inverible mod p, then error is raised.
void ppsolve(vec_zz_pE& x, const mat_zz_pE& A, const vec_zz_pE& b,
             long p, long r) 
{

   if (r == 1) {
      zz_pE det;
      solve(det, x, A, b);
      if (det == 0) Error("ppsolve: matrix not invertible");
      return;
   }

   long n = A.NumRows();
   if (n != A.NumCols()) 
      Error("ppsolve: matrix not square");
   if (n == 0)
      Error("ppsolve: matrix of dimension 0");

   zz_pContext pr_context;
   pr_context.save();

   zz_pEContext prE_context;
   prE_context.save();

   zz_pX G = zz_pE::modulus();

   ZZX GG = to_ZZX(G);

   vector< vector<ZZX> > AA;
   convert(AA, A);

   vector<ZZX> bb;
   convert(bb, b);

   zz_pContext p_context(p);
   p_context.restore();

   zz_pX G1 = to_zz_pX(GG);
   zz_pEContext pE_context(G1);
   pE_context.restore();

   // we are now working mod p...

   // invert A mod p

   mat_zz_pE A1;
   convert(A1, AA);

   mat_zz_pE I1;
   zz_pE det;

   inv(det, I1, A1);
   if (det == 0) {
      Error("ppsolve: matrix not invertible");
   }

   vec_zz_pE b1;
   convert(b1, bb);

   vec_zz_pE y1;
   y1 = b1 * I1;

   vector<ZZX> yy;
   convert(yy, y1);

   // yy is a solution mod p

   for (long k = 1; k < r; k++) {
      // lift solution yy mod p^k to a solution mod p^{k+1}

      pr_context.restore();
      prE_context.restore();
      // we are now working mod p^r

      vec_zz_pE d, y;
      convert(y, yy);

      d = b - y * A;

      vector<ZZX> dd;
      convert(dd, d);

      long pk = power_long(p, k);
      vector<ZZX> ee;
      div(ee, dd, pk);

      p_context.restore();
      pE_context.restore();

      // we are now working mod p

      vec_zz_pE e1;
      convert(e1, ee);
      vec_zz_pE z1;
      z1 = e1 * I1;

      vector<ZZX> zz, ww;
      convert(zz, z1);

      mul(ww, zz, pk);
      add(yy, yy, ww);
   }

   pr_context.restore();
   prE_context.restore();

   convert(x, yy);

   assert(x*A == b);
}

void ppsolve(vec_GF2E& x, const mat_GF2E& A, const vec_GF2E& b,
             long p, long r) 
{
   assert(p == 2 && r == 1);

   GF2E det;
   solve(det, x, A, b);
   if (det == 0) Error("ppsolve: matrix not invertible");
}

void buildLinPolyCoeffs(vec_zz_pE& C_out, const vec_zz_pE& L, long p, long r)
{
   mat_zz_pE M;
   buildLinPolyMatrix(M, p);

   vec_zz_pE C;
   ppsolve(C, M, L, p, r);

   C_out = C;
}

void buildLinPolyCoeffs(vec_GF2E& C_out, const vec_GF2E& L, long p, long r)
{
   assert(p == 2 && r == 1);

   mat_GF2E M;
   buildLinPolyMatrix(M, p);

   vec_GF2E C;
   ppsolve(C, M, L, p, r);

   C_out = C;
}

void applyLinPoly(zz_pE& beta, const vec_zz_pE& C, const zz_pE& alpha, long p)
{
   long d = zz_pE::degree();
   assert(d == C.length());

   zz_pE gamma, res;

   gamma = to_zz_pE(zz_pX(1, 1));
   res = C[0]*alpha;
   for (long i = 1; i < d; i++) {
      gamma = power(gamma, p);
      res += C[i]*to_zz_pE(CompMod(rep(alpha), rep(gamma), zz_pE::modulus()));
   }

   beta = res;
}

void applyLinPoly(GF2E& beta, const vec_GF2E& C, const GF2E& alpha, long p)
{
   long d = GF2E::degree();
   assert(d == C.length());

   GF2E gamma, res;

   gamma = to_GF2E(GF2X(1, 1));
   res = C[0]*alpha;
   for (long i = 1; i < d; i++) {
      gamma = power(gamma, p);
      res += C[i]*to_GF2E(CompMod(rep(alpha), rep(gamma), GF2E::modulus()));
   }

   beta = res;
}

// Auxilliary classes to facillitiate faster reduction mod Phi_m(X)
// when the input has degree less than m


static
void LocalCopyReverse(zz_pX& x, const zz_pX& a, long lo, long hi)

   // x[0..hi-lo] = reverse(a[lo..hi]), with zero fill
   // input may not alias output

{
   long i, j, n, m;

   n = hi-lo+1;
   m = a.rep.length();

   x.rep.SetLength(n);

   const zz_p* ap = a.rep.elts();
   zz_p* xp = x.rep.elts();

   for (i = 0; i < n; i++) {
      j = hi-i;
      if (j < 0 || j >= m)
         clear(xp[i]);
      else
         xp[i] = ap[j];
   }

   x.normalize();
} 

static
void LocalCyclicReduce(zz_pX& x, const zz_pX& a, long m)

// computes x = a mod X^m-1

{
   long n = deg(a);
   long i, j;
   zz_p accum;

   if (n < m) {
      x = a;
      return;
   }

   if (&x != &a)
      x.rep.SetLength(m);

   for (i = 0; i < m; i++) {
      accum = a.rep[i];
      for (j = i + m; j <= n; j += m)
         add(accum, accum, a.rep[j]);
      x.rep[i] = accum;
   }

   if (&x == &a)
      x.rep.SetLength(m);

   x.normalize();
}

zz_pXModulus1::zz_pXModulus1(long _m, const zz_pX& _f) 
: m(_m), f(_f), n(deg(f))
{
   assert(m > n);

   specialLogic = (m - n > 10 && m < 2*n);
   build(fm, f);
   
   if (specialLogic) {
      zz_pX P1, P2, P3;

      LocalCopyReverse(P3, f, 0, n);
      InvTrunc(P2, P3, m-n);
      LocalCopyReverse(P1, P2, 0, m-n-1);

      k = NextPowerOfTwo(2*(m-1-n)+1);
      k1 = NextPowerOfTwo(n);

      TofftRep(R0, P1, k); 
      TofftRep(R1, f, k1);
   }
}


void rem(zz_pX& r, const zz_pX& a, const zz_pXModulus1& ff)
{
   if (!ff.specialLogic) {
      rem(r, a, ff.fm);
      return;
   }

   long m = ff.m;
   long n = ff.n;
   long k = ff.k;
   long k1 = ff.k1;
   const fftRep& R0 = ff.R0;
   const fftRep& R1 = ff.R1;

   if (deg(a) < n) {
      r = a;
      return;
   }

   zz_pX P2, P3;

   fftRep R2, R3;

   TofftRep(R2, a, k, n, m-1);
   mul(R2, R2, R0);
   FromfftRep(P3, R2, m-1-n, 2*(m-1-n));
   
   long l = 1L << k1;

   TofftRep(R3, P3, k1);
   mul(R3, R3, R1);
   FromfftRep(P3, R3, 0, n-1);
   LocalCyclicReduce(P2, a, l);
   trunc(P2, P2, n);
   sub(P2, P2, P3);
   r = P2;
}



