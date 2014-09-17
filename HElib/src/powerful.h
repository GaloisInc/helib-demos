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
 * @file powerful.h
 * @brief The "powerful basis" of a cyclotomic ring
 **/
#include "NumbTh.h"
#include "bluestein.h"
#include "cloned_ptr.h"
#include "hypercube.h"
#include <NTL/lzz_pX.h>

//! @class FFTHelper
//! @brief Helper class for FFT over NTL::zz_p
//!
//! The class FFTHelper is used to help perform FFT over zz_p.
//! The constructor supplies an element x in zz_p and an integer m,
//! where x has order m and x has a square root in zz-p.
//! Auxilliary data structures are constricted to support
//! evaluation and interpolation at the points x^i for i in Z_m^*
//!
//! It is assumed that the zz_p-context is set prior to all
//! constructor and method invocations.
class FFTHelper {

private:
  long m;
  zz_p m_inv;
  zz_p root, iroot;
  zz_pXModulus phimx;
  Vec<bool> coprime;
  long phim;

  mutable zz_pX powers, ipowers;
  mutable Vec<mulmod_precon_t> powers_aux, ipowers_aux;
  mutable fftRep Rb, iRb;
  mutable fftrep_aux Rb_aux, iRb_aux;
  mutable fftRep Ra; 
  mutable zz_pX tmp;

public:
  FFTHelper(long _m, zz_p x);

  void FFT(const zz_pX& f, Vec<zz_p>& v) const;
    // compute v = { f(x^i) }_{i in Z_m^*}
  void iFFT(zz_pX& f, const Vec<zz_p>& v, bool normalize = true) const;
    // computes inverse transform. If !normalize, result is scaled by m

  const zz_p& get_m_inv() const { return m_inv; }
  // useful for aggregate normalization

};


//! For x=p^e, returns phi(p^e) = (p-1) p^{e-1}
inline long computePhi(const Pair<long, long>& x)
{
  long p = x.a;
  long e = x.b;
  return power_long(p, e - 1) * (p-1);
}

//! For x = (p,e), returns p^e
inline long computePow(const Pair<long, long>& x)
{
  long p = x.a;
  long e = x.b;
  return power_long(p, e);
}


//! returns \prod_d vec[d]
long computeProd(const Vec<long>& vec);

//! For vec[d] = (a_d , b_d), returns \prod_d a_d^{b_d}
long computeProd(const Vec< Pair<long, long> >& vec);

//! For factors[d] = (p_d, e_d), computes
//! phiVec[d] = phi(p_d^{e_d}) = (p_d-1) p_i{e_d-1}
void computePhiVec(Vec<long>& phiVec,
                   const Vec< Pair<long, long> >& factors);

//! For factors[d] = (p_d, e_d), computes powVec[d] = p_d^{e_d}
void computePowVec(Vec<long>& powVec, 
                   const Vec< Pair<long, long> >& factors);

//! For powVec[d] = p_d^{e_d}, m = \prod_d p_d^{e_d}, computes
//! divVec[d] = m/p_d^{e_d}
void computeDivVec(Vec<long>& divVec, long m,
                   const Vec<long>& powVec);

//! For divVec[d] = m/p_d^{e_d}, powVec[d] = p^{e_d}, computes
//! invVec[d] = divVec[d]^{-1} mod powVec[d]
void computeInvVec(Vec<long>& invVec,
                   const Vec<long>& divVec, const Vec<long>& powVec);

//! For powVec[d] = p_d^{e_d}, cycVec[d] = Phi_{p_d^{e_d}}(X) mod p
void computeCycVec(Vec<zz_pX>& cycVec, const Vec<long>& powVec);

//! m = m_1 ... m_k, m_d = p_d^{e_d} 
//! powVec[d] = m_d
//! invVec[d] = (m/m_d)^{-1} mod m_d
//! computes polyToCubeMap[i] and cubeToPolyMap[i] 
//!   where polyToCubeMap[i] is the index of (i_1, ..., i_k)
//!   in the cube with CubeSignature longSig = (m_1, ..., m_k),
//!   and (i_1, ..., i_k) is the unique tuple satistifying
//!   i = i_1 (m/m_1) + ... + i_k (m/m_k) mod m
//! and
//!   cubeToPolyMap is the inverse map.
void computePowerToCubeMap(Vec<long>& polyToCubeMap,
                           Vec<long>& cubeToPolyMap,
                           long m,
                           const Vec<long>& powVec,
                           const Vec<long>& invVec,
                           const CubeSignature& longSig);

//! shortSig is a CubeSignature for (phi(m_1), .., phi(m_k)),
//! longSig is a CubeSignature for (m_1, ..., m_k).
//! computes shortToLongMap[i] that maps an index i 
//! with respect to shortSig to the corresponding index
//! with respect to longSig.
void computeShortToLongMap(Vec<long>& shortToLongMap, 
                           const CubeSignature& shortSig, 
                           const CubeSignature& longSig);

//! Computes the inverse of the shortToLongMap, computed above.
//! "undefined" entries are initialzed to -1.
void computeLongToShortMap(Vec<long>& longToShortMap,
                           long m,
                           const Vec<long>& shortToLongMap);

//! This routine recursively reduces each hypercolumn
//! in dimension d (viewed as a coeff vector) by Phi_{m_d}(X)
//! If one starts with a cube of dimension (m_1, ..., m_k),
//! one ends up with a cube that is effectively of dimension
//! phi(m_1, ..., m_k). Viewed as an element of the ring
//! F_p[X_1,...,X_k]/(Phi_{m_1}(X_1), ..., Phi_{m_k}(X_k)),
//! the cube remains unchanged.
void recursiveReduce(const CubeSlice<zz_p>& s, 
                     const Vec<zz_pX>& cycVec, 
                     long d,
                     zz_pX& tmp1,
                     zz_pX& tmp2);

//! This routine implements the isomorphism from
//! F_p[X]/(Phi_m(X)) to F_p[X_1, ..., X_k]/(Phi_{m_1}(X_1),...,Phi_{m_k}(X_k))
//! The input is poly, which must be of degree < m, and the
//! output is cube, which is a HyperCube of dimension (phi(m_1), ..., phi(m_k)).
//! The caller is responsible to supply "scratch space" in the
//! form of a HyperCube tmpCube of dimension (m_1, ..., m_k).
void convertPolyToPowerful(HyperCube<zz_p>& cube, 
                           HyperCube<zz_p>& tmpCube, 
                           const zz_pX& poly,
                           const Vec<zz_pX>& cycVec,
                           const Vec<long>& polyToCubeMap,
                           const Vec<long>& shortToLongMap);

//! This implements the inverse of the above isomorphism.
void convertPowerfulToPoly(zz_pX& poly,
                           const HyperCube<zz_p>& cube,
                           long m,
                           const Vec<long>& shortToLongMap,
                           const Vec<long>& cubeToPolyMap,
                           const zz_pX& phimX);

//! powVec[d] = m_d = p_d^{e_d}
//! computes multiEvalPoints[d] as a vector of length phi(m_d)
//!   containing base^{(m/m_d) j} for j in Z_{m_d}^*
void computeMultiEvalPoints(Vec< Vec<zz_p> >& multiEvalPoints,
                            const zz_p& base,
                            long m,
                            const Vec<long>& powVec,
                            const Vec<long>& phiVec);

//! powVec[d] = m_d = p_d^{e_d}
//! computes multiEvalPoints[d] as an FFTHelper for base^{m/m_d}
void computeMultiEvalPoints(Vec< copied_ptr<FFTHelper> >& multiEvalPoints,
                            const zz_p& base,
                            long m,
                            const Vec<long>& powVec,
                            const Vec<long>& phiVec);

//! computes linearEvalPoints[i] = base^i, i in Z_m^*
void computeLinearEvalPoints(Vec<zz_p>& linearEvalPoints,
                             const zz_p& base,
                             long m, long phim);

//! powVec[d] = m_d = p_d^{e_d}
//! computes compressedIndex[d] as a vector of length m_d,
//!   where compressedIndex[d][j] = -1 if GCD(j, m_d) != 1,
//!   and otherwise is set to the relative numerical position
//!   of j among Z_{m_d}^*
void computeCompressedIndex(Vec< Vec<long> >& compressedIndex,
                            const Vec<long>& powVec);

//! computes powToCompressedIndexMap[i] as -1 if GCD(i, m) != 1, 
//!   and otherwise as the index of the point (j_1, ..., j_k)
//!   relative to a a cube of dimension (phi(m_1), ..., phi(m_k)),
//!   where each j_d is the compressed index of i_d = i mod m_d.
void computePowToCompressedIndexMap(Vec<long>& powToCompressedIndexMap,
                                    long m,
                                    const Vec<long>& powVec,
                                    const Vec< Vec<long> >& compressedIndex,
                                    const CubeSignature& shortSig);

void recursiveEval(const CubeSlice<zz_p>& s,
                   const Vec< Vec<zz_p> >& multiEvalPoints,
                   long d,
                   zz_pX& tmp1,
                   Vec<zz_p>& tmp2);

inline void eval(HyperCube<zz_p>& cube,
		 const Vec< Vec<zz_p> >& multiEvalPoints)
{
   zz_pX tmp1;
   Vec<zz_p> tmp2;

   recursiveEval(CubeSlice<zz_p>(cube), multiEvalPoints, 0, tmp1, tmp2);
} 

void recursiveEval(const CubeSlice<zz_p>& s,
                   const Vec< copied_ptr<FFTHelper> >& multiEvalPoints,
                   long d,
                   zz_pX& tmp1,
                   Vec<zz_p>& tmp2);

inline void eval(HyperCube<zz_p>& cube,
          const Vec< copied_ptr<FFTHelper> >& multiEvalPoints)
{
   zz_pX tmp1;
   Vec<zz_p> tmp2;

   recursiveEval(CubeSlice<zz_p>(cube), multiEvalPoints, 0, tmp1, tmp2);
} 

void recursiveInterp(const CubeSlice<zz_p>& s,
                     const Vec< copied_ptr<FFTHelper> >& multiEvalPoints,
                     long d,
                     zz_pX& tmp1,
                     Vec<zz_p>& tmp2);

void interp(HyperCube<zz_p>& cube,
	    const Vec< copied_ptr<FFTHelper> >& multiEvalPoints);

//! this maps an index j in [phi(m)] to a vector
//! representing the powerful basis coordinates
void mapIndexToPowerful(Vec<long>& pow, long j, const Vec<long>& phiVec);

void mapPowerfulToPoly(ZZX& poly, 
                       const Vec<long>& pow, 
                       const Vec<long>& divVec,
                       long m,
                       const ZZX& phimX);
