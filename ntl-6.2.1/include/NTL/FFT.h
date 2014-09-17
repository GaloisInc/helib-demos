

#ifndef NTL_FFT__H
#define NTL_FFT__H

#include <NTL/ZZ.h>
#include <NTL/vector.h>
#include <NTL/vec_long.h>

NTL_OPEN_NNS

#define NTL_FFTFudge (4)
// This constant is used in selecting the correct
// number of FFT primes for polynomial multiplication
// in ZZ_pX and zz_pX.  Set at 4, this allows for
// two FFT reps to be added or subtracted once,
// before performing CRT, and leaves a reasonable margin for error.
// Don't change this!

#define NTL_FFTMaxRootBnd (NTL_SP_NBITS-2)
// Absolute maximum root bound for FFT primes.
// Don't change this!

#if (25 <= NTL_FFTMaxRootBnd)
#define NTL_FFTMaxRoot (25)
#else
#define NTL_FFTMaxRoot  NTL_FFTMaxRootBnd
#endif
// Root bound for FFT primes.  Held to a maximum
// of 25 to avoid large tables and excess precomputation,
// and to keep the number of FFT primes needed small.
// This means we can multiply polynomials of degree less than 2^24.  
// This can be increased, with a slight performance penalty.


// New interface

class FFTMultipliers {
public:
   long MaxK;
   Vec< Vec<long> > wtab_precomp;
   Vec< Vec<mulmod_precon_t> > wqinvtab_precomp;

   FFTMultipliers() : MaxK(-1) { }
};

#ifndef NTL_WIZARD_HACK
class zz_pInfoT; // forward reference, defined in lzz_p.h
#else
typedef long zz_pInfoT;
#endif


struct FFTPrimeInfo {
   long q;   // the prime itself
   double qinv;   // 1/((double) q)

   zz_pInfoT *zz_p_context; 
   // pointer to corresponding zz_p context, which points back to this 
   // object in the case of a non-user FFT prime

   Vec<long> RootTable;
   //   RootTable[j] = w^{2^{MaxRoot-j}},
   //                  where w is a primitive 2^MaxRoot root of unity
   //                  for q

   Vec<long> RootInvTable;
   // RootInvTable[j] = 1/RootTable[j] mod q

   Vec<long> TwoInvTable;
   // TwoInvTable[j] = 1/2^j mod q

   Vec<mulmod_precon_t> TwoInvPreconTable;
   // mulmod preconditioning data

   long bigtab; // flag indicating if we use big tables for this prime
   FFTMultipliers MulTab;
   FFTMultipliers InvMulTab;

};

void InitFFTPrimeInfo(FFTPrimeInfo& info, long q, long w, long bigtab);

#define NTL_FFT_BIGTAB_LIMIT (256)
// big tables are only used for the first NTL_FFT_BIGTAB_LIMIT primes
// TODO: maybe we should have a similar limit for the degree of
// the convolution as well.


NTL_THREAD_LOCAL
extern FFTPrimeInfo **FFTTables;


// legacy interface

NTL_THREAD_LOCAL
extern long NumFFTPrimes;

NTL_THREAD_LOCAL
extern long *FFTPrime;

NTL_THREAD_LOCAL
extern double *FFTPrimeInv;



long CalcMaxRoot(long p);
// calculates max power of two supported by this FFT prime.

void UseFFTPrime(long index);
// allocates and initializes information for FFT prime


void FFT(long* A, const long* a, long k, long q, const long* root);
// the low-level FFT routine.
// computes a 2^k point FFT modulo q, using the table root for the roots.

void FFT(long* A, const long* a, long k, long q, const long* root, FFTMultipliers& tab);


inline
void FFTFwd(long* A, const long *a, long k, FFTPrimeInfo& info)
// Slightly higher level interface...using the ith FFT prime
{
#ifdef NTL_FFT_BIGTAB
   if (info.bigtab)
      FFT(A, a, k, info.q, &info.RootTable[0], info.MulTab);
   else
      FFT(A, a, k, info.q, &info.RootTable[0]);
#else
   FFT(A, a, k, info.q, &info.RootTable[0]);
#endif
}

inline 
void FFTFwd(long* A, const long *a, long k, long i)
{
   FFTFwd(A, a, k, *FFTTables[i]);
}

inline
void FFTRev(long* A, const long *a, long k, FFTPrimeInfo& info)
// Slightly higher level interface...using the ith FFT prime
{
#ifdef NTL_FFT_BIGTAB
   if (info.bigtab)
      FFT(A, a, k, info.q, &info.RootInvTable[0], info.InvMulTab);
   else
      FFT(A, a, k, info.q, &info.RootInvTable[0]);
#else
   FFT(A, a, k, info.q, &info.RootInvTable[0]);
#endif
}

inline
void FFTRev(long* A, const long *a, long k, long i)
{
   FFTRev(A, a, k, *FFTTables[i]);
}

inline
void FFTMulTwoInv(long* A, const long *a, long k, FFTPrimeInfo& info)
{
   VectorMulModPrecon(1L << k, A, a, info.TwoInvTable[k], info.q, 
                      info.TwoInvPreconTable[k]);
}

inline
void FFTMulTwoInv(long* A, const long *a, long k, long i)
{
   FFTMulTwoInv(A, a, k, *FFTTables[i]);
}

inline 
void FFTRev1(long* A, const long *a, long k, FFTPrimeInfo& info)
// FFTRev + FFTMulTwoInv
{
   FFTRev(A, a, k, info);
   FFTMulTwoInv(A, A, k, info);
}

inline 
void FFTRev1(long* A, const long *a, long k, long i)
{
   FFTRev1(A, a, k, *FFTTables[i]);
}


long IsFFTPrime(long n, long& w);
// tests if n is an "FFT prime" and returns corresponding root




NTL_CLOSE_NNS

#endif
