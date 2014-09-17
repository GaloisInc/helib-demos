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
#include "NumbTh.h"
#include "permutations.h"

const Vec<long> SubDimension::dummyBenes; // global variable

// Apply a permutation to a vector, out[i]=in[p1[i]]
// Unfortunately we need to implement for both Vec<long> and vector<long>
template<class T>
void applyPermToVec(Vec<T>& out, const Vec<T>& in, const Permut& p1)
{
  assert(&out != &in); // NOT an in-place procedure
  out.SetLength(p1.length());
  for (long i=0; i<p1.length(); i++)
    out[i] = in.at(p1[i]);
}
template<class T>
void applyPermToVec(vector<T>& out, const vector<T>& in, const Permut& p1)
{
  out.resize(p1.length());
  for (long i=0; i<p1.length(); i++)
    out[i] = in.at(p1[i]);
}

// Apply two permutations to a vector out[i]=in[p2[p1[i]]]
template<class T>
void applyPermsToVec(Vec<T>& out, const Vec<T>& in,
		      const Permut& p2, const Permut& p1)
{
  assert(&out != &in); // NOT an in-place procedure
  assert(p1.length() == p2.length());
  out.SetLength(p1.length());
  for (long i=0; i<p1.length(); i++)
    out[i] = in.at(p2.at(p1[i]));
}
template<class T>
void applyPermsToVec(vector<T>& out, const vector<T>& in,
		      const Permut& p2, const Permut& p1)
{
  assert(p1.length() == p2.length());
  out.resize(p1.length());
  for (long i=0; i<p1.length(); i++)
    out[i] = in.at(p2.at(p1[i]));
}
// explicit instantiations
template void applyPermToVec<long>(Vec<long>& out, const Vec<long>& in,
			       const Permut& p1);
template void applyPermToVec<long>(vector<long>& out, const vector<long>& in,
			       const Permut& p1);
template void applyPermToVec<ZZX>(vector<ZZX>& out, const vector<ZZX>& in,
			       const Permut& p1);

template void applyPermsToVec<long>(Vec<long>& out, const Vec<long>& in,
				const Permut& p2, const Permut& p1);
template void applyPermsToVec<long>(vector<long>& out, const vector<long>& in,
				const Permut& p2, const Permut& p1);


// Generate a random permutation on [0..n-1]
void randomPerm(Permut& perm, long n)
{
  perm.SetLength(n);
  for (long j = 0; j < n; j++)
     perm[j] = j;
   
  // random shuffle
  for (long m = n; m > 0; m--) {
     long p = RandomBnd(m);
     // swap positions p and m-1 of perm
     long tmp = perm[p];
     perm[p] = perm[m-1];
     perm[m-1] = tmp;
  }
}


// Write a column permutation explicitly
void ColPerm::makeExplicit(Permut& out) const
{
  long sz = getSize();
  out.SetLength(sz);

  for (long k = 0; k < sz; k++) {
    long i = getCoord(k, dim);
    long pi_i = at(k);
    out.at(k) = addCoord(k, dim, pi_i-i);
  }
}




// For each position in the data vector, compute how many slots it should be
// shifted inside its small permutation.
// Return value is zero if all the shift amounts are zero, nonzero otherwise.
long ColPerm::getShiftAmounts(Vec<long>& out) const
{
  long sz = getSize();
  out.SetLength(sz);
  long nonZero = 0;

  for (long k = 0; k < sz; k++) {
    long i = getCoord(k, dim);
    long pi_i = at(k);
    if (i != pi_i) nonZero = 1;
    out.at(addCoord(k, dim, pi_i-i)) = i - pi_i;
  }

  return nonZero;
 
}

// Compute the shift amounts corresponding to collapsing 'numLvls' levels
// of the Benes network in 'net' starting from 'startLvl'. Resulting shift
// amounts are returned in 'out', and the return value indicates if the
// resulting permutation from collapsing these levels is the identity.
static bool
collapseBenesLevels(Permut& out, const GeneralBenesNetwork &net,
		    long startLvl, long numLvls)
{
  bool noChange = true;
  // Compute partial permutation computed by the next few Benes levels
  for (long i=0; i<net.getSize(); i++) { // go over all slots
    long i2 = i;
    // compute where this slot is mapped to
    for (long l=startLvl; l<startLvl+numLvls; l++) {
      // how much to shift slot i2 between this level and next
      i2 +=  net.shamt(l) * net.getLevel(l)[i2];
    }
    out[i] = i2 - i; // shift amount for all levels together
    noChange = noChange && (i == i2);
  }
  return noChange;
}

// Get multiple layers of a Benes permutation network. Returns in out[i][j]
// the shift amount to move item j in the i'th layer. Also isID[i]=true if
// the i'th layer is the identity (i.e., contains only 0 shift amounts).
void ColPerm::getBenesShiftAmounts(Vec<Permut>& out, Vec<bool>& isID,
				   const Vec<long>& benesLvls) const
{
  // Go over the columns one by one. For each column extract the columns
  // permutation, prepare a Benes network for it, and then for every layer
  // compute the shift amounts for this columns.

  long n = getDim(dim);     // the permutations are over [0,n-1]

  // Allocate space
  out.SetLength(benesLvls.length());
  isID.SetLength(benesLvls.length());
  for (long k=0; k<benesLvls.length(); k++) {
    out[k].SetLength(getSize());
    isID[k] = true;
  }

  Vec<long> col;
  col.SetLength(n);

  for (long slice_index = 0; slice_index < numSlices(dim); slice_index++) {
    ConstCubeSlice<long> slice(*this, slice_index, dim);
    for (long col_index = 0; col_index < slice.numCols(); col_index++) {
      getHyperColumn(col, slice, col_index);

      GeneralBenesNetwork net(col); // build a Benes network for this column

      // Sanity checks: width of network == n,
      //                and sum of benesLvls entries == # of levels
      assert(net.getSize()==n);
      {long sum=0;
       for (long k=0; k<benesLvls.length(); k++) sum+=benesLvls[k];
       assert(net.getNumLevels()==sum);
      }

      // Compute the layers of the collapased network for this column
      for (long lvl=0,k=0; k<benesLvls.length(); lvl += benesLvls[k], k++) {

	// Returns in col the shift amounts for this layer in the network,
	// restricted to this column. Also returns true if the returned
	// permutation is the idendity, false otherwise.
	bool id = collapseBenesLevels(col, net, lvl, benesLvls[k]);
	isID[k] = isID[k] && id;

        CubeSlice<long> oslice(out[k], getSig());
        CubeSlice<long> osubslice(oslice, slice_index, dim);
        setHyperColumn(col, osubslice, col_index);
      }  // next collapsed layer
    }  // next column
  } // next slice
}


// Break a permutation into column-row-column format. The input pi permutes
// each dimension-i subcube, and in the output rho1,rho3 permute only along
// the i'th dimension and rho2 permutes each dimension-i+1 subcube.
// This routine cannot permute in-place, it is assumed that pi and rho2 point
// to disjoint vectors.
void breakPermTo3(const HyperCube<long>& pi, long dim, 
                  ColPerm& rho1, HyperCube<long>& rho2, ColPerm& rho3)
{
  assert(&rho1.getSig()==&pi.getSig());
  assert(&rho2.getSig()==&pi.getSig());
  assert(&rho3.getSig()==&pi.getSig());

  // pi consists of separate permutations over [0,n-1], and each
  // of these is viewed as a permutation over an n1 x n2 cube

  long n1 = pi.getDim(dim); // Size of this dimension
  long n2 = pi.getProd(dim+1); 
  long n = pi.getProd(dim); // = n1*n2;

  // representing I_n as I_n1 x I_n2: i == n2*rep[i].first + rep[i].second
  vector< pair<long,long> > rep(n);
  for (long ind=0,i=0; i<n1; i++) for (long j=0; j<n2; j++,ind++) {
      rep[ind].first = i;
      rep[ind].second = j;
    }

  for (long slice_index = 0; slice_index < pi.numSlices(dim); slice_index++) {
    ConstCubeSlice<long> pi_slice(pi, slice_index, dim);
    CubeSlice<long> rho1_slice(rho1, slice_index, dim);
    CubeSlice<long> rho2_slice(rho2, slice_index, dim);
    CubeSlice<long> rho3_slice(rho3, slice_index, dim);

    // Construct a bipartite n2-by-n2 graph for pi (cf. Lemma 1 in [GHS12a]).
    // For each j=pi(i) with representations i=(i1,i2) and j=(j1,j2), we put
    // in the bipartite graph an edge i2->j2 and label it by i.
    BipartitleGraph bg;
    for (long i=0; i<n; i++) {
      long j = pi_slice[i];  // the image of i under the permutation
      // when i = (i1,i2) and j=(j1,j2), add an edge from i2 to j2 labeled i
      bg.addEdge(rep.at(i).second, rep.at(j).second, i);
    }
    // The bipartite graph is n1-regular, so we can break its edges into
    // n1 perfect matchings, which are numbered 1,2,...,n1.
    bg.partitionToMatchings();

    // The output permutations are defined by the representation i<->(i1,i2),
    // the target permutation pi, and the coloring of the bipartite graph.
    // Denote by sigma(i) the color of the edge labeled by i, sigma(i) \in
    // {1,...,n1}. Also denote by (pi^1(i),pi^2(i)) the representation of
    // pi(i). Then:
    //
    // + rho_1 is defined by           (i1, i2) -> (sigma(i), i2)
    // + rho_2 is defined by     (sigma(i), i2) -> (sigma(i),pi^2(i))
    // + rho_3 is defined by (sigma(i),pi^2(i)) -> (pi^1(i), pi^2(i))
    //
    // rho_1 is a permutation because for every value of i2 (corresponding to
    // a left node in the graph), all the edges leaving that node have different
    // colors.
    //
    // rho_2 is a permutation since the edges of each color form a perfect
    // matching, so for every left-node j2 and color c there is a single
    // c-colored edge going into j2. The label of that edge determines a unique
    // origin index i=(i1,i2), and therefore also the pre-image of (c,j2) under
    // rho_2, which is (sigma(i),i2)=(c,i2).
    //
    // rho_3 is a permutation because rho_1,rho_2,pi are permutations, and
    // so is pi = rho_3 o rho_2 o rho_1.
    //
    // Note that the edges are colored 1..n2 while our represenation above
    // has the second digits in the range 0..n2-1, so below we use sigma(i)-1
    // rather than sigma(i).

    for (long i2=0; i2<n2; i2++) // go over all edges in the bipartite graph
      for (LNeighborList::iterator it=bg.left[i2].neighbors.begin(); 
	   it!=bg.left[i2].neighbors.end(); ++it) {
	LabeledEdge& e = it->second; // An edge in the bipartite graph;
	long i = e.label; // labeled by i
	long c = e.color -1; // colored by c (after the -1 adjustment)
	// i2 = e.from = rep[i].second;
	long j = pi_slice[i];
	long j1 = rep[j].first;
	long j2 = e.to;   // = it->first = rep[j].second;

	long tmp1 = c*n2 + i2;  // the image of i under rho1 = (c,i2)
	rho3_slice[i] = c;
	long tmp2 = c*n2 + j2;  // the image of tmp1 under rho2 =(c,j2)
	rho2_slice[tmp1] = j2;
	rho1_slice[tmp2] = j1; // image of tmp2 under rho3 =(j1,j2)=pi(i)
      }
    // FIXME: The comments above do not match the code, the roles
    //        of rho1,rho3 are switched. Why is this code working??
  }
  rho1.setPermDim(dim);
  rho3.setPermDim(dim);
}



/* Takes a permutation pi over an m-dimensional cube C=Z_{n1} x ... x Z_{nm}
 * and expresses pi as a product pi = rho_{2m-1} o ... o rho_2 o rho_1 where
 * each rho_i is a column permutation along one dimension. Specifically for
 * i<m, the permutations rho_i and rho_{2(m-1)-i} permute the i'th dimension
 ************************************************************************/
void breakPermByDim(vector<ColPerm>& out, 
		    const Permut &pi, const CubeSignature& sig)
{
  assert(sig.getSize()==pi.length());

  HyperCube<long> tmp1(sig);  
  tmp1.getData() = pi;

  HyperCube<long> tmp2(sig);  

  HyperCube<long>* tp1 = &tmp1;
  HyperCube<long>* tp2 = &tmp2;

  // Allocate the output permutations
  long m = sig.getNumDims();
  ColPerm dummy(sig);
  out.assign(2*m-1, dummy); // allocate space and initialize

  if (m == 1) { // special case, no need to break
    HyperCube<long>& out0 = out[0];
    out0 = tmp1;
    out[0].setPermDim(0);
    return;
  }

  for (long i=0; i<m-2; i++) {
    breakPermTo3(*tp1, i, out[i], *tp2, out[2*m-i-2]);
    std::swap(tp1,tp2);
  }

  breakPermTo3(*tp1, m-2, out[m-2], out[m-1], out[m]);
  out[m-1].setPermDim(m-1);
}


/********************************************************************/
/**********     MAPPING BETWEEN CUBE AND LINEAR ARRAY      **********/
/********************************************************************/

// Get the "crude" cube dimensions corresponding to a vector of trees,
// the ordered vector with one dimension per tree
void GeneratorTrees::getCubeDims(Vec<long>& dims) const
{
  dims.SetLength(trees.length());

  // copy dims from the trees
  for (long i=0; i<trees.length(); i++) {
    const OneGeneratorTree& T = trees[i];
    dims[T.getAuxKey()] = T.DataOfNode(T.rootIdx()).size;
    // getAuxKey() returns the generator number associated with this tree
  }
}

// Get the "fine" cube dimensions corresponding to a vector of trees,
// the ordered vector with one dimension per leaf in any of the trees.
void GeneratorTrees::getCubeSubDims(Vec<long>& dims) const
{
  // how many dimensions do we need
  long nDims = 0;
  for (long i=0; i<trees.length(); i++)
    nDims += trees[i].getNleaves();
  dims.SetLength(nDims); // set the size

  // copy dims from the leaves in all the trees
  long idx = 0;
  for (long i=0; i<trees.length(); i++) {
    const OneGeneratorTree& T = trees[i];
    for (long leaf=T.firstLeaf(); leaf>=0; leaf=T.nextLeaf(leaf))
      dims[idx++] = T[leaf].getData().size;
  }
}

// Adds one to the little-endian representation of an integer in base digits,
// returns true if there was an overflow
static bool addOne(Vec<long>& rep, const Vec<long> digits)
{
  for (long i=rep.length()-1; i>=0; --i) {
    rep[i]++;
    if (rep[i] >= digits[i])
      rep[i] -= digits[i];
    else
      return false;
  }
  return true;
}

// Compute the mapping between linear array and a hypercube corresponding
/// to a single generator tree
void ComputeOneGenMapping(Permut& genMap, const OneGeneratorTree& T)
{
  Vec<long> dims(INIT_SIZE, T.getNleaves());
  Vec<long> coefs(INIT_SIZE,T.getNleaves());
  for (long i=T.getNleaves()-1, leaf=T.lastLeaf(); i>=0;
                                i--, leaf=T.prevLeaf(leaf)) {
    dims[i] = T[leaf].getData().size;
    coefs[i] = T[leaf].getData().e;
  }

  // A representation of an integer with digits from dims
  Vec<long> rep(INIT_SIZE, T.getNleaves());
  for (long i=0; i<rep.length(); i++) rep[i]=0; // initialize to zero

  // initialize to all zero
  long sz = T[0].getData().size;
  genMap.SetLength(sz);
  for (long i=0; i<sz; i++) genMap[i]=0;

  // compute the permutation
  for (long i=1; i<sz; i++) {
    addOne(rep, dims); // representation of i in base dims
    for (long j=0; j<coefs.length(); j++) {
      long tmp = MulMod(rep[j], coefs[j], sz);
      genMap[i] = AddMod(genMap[i], tmp, sz);
    }
  }
}

// Compute the mapping between linear array and the hypercube
// corresponding to all the trees.
void GeneratorTrees::ComputeCubeMapping()
{
  assert(trees.length()>=1);

  if (trees.length()==1) {  // A single tree
    ComputeOneGenMapping(map2array, trees[0]);
  }
  else { // more than one generator
    // Compute the sub-mapping for every generator. Also prepare two hypercube
    // signature objects for the index calculations, with the two ordering of
    // the generators: one for the generators ordered by their index 0,1,2...
    // and the other odred the generators by the order of their trees

    Vec<long> dims1(INIT_SIZE,trees.length()), dims2(INIT_SIZE,trees.length());
    Vec<Permut> genMappings(INIT_SIZE, trees.length());
    for (long i=0; i<trees.length(); i++) {
      dims1[i] = trees[i][0].getData().size;
      ComputeOneGenMapping(genMappings[i], trees[i]);
    }
    getCubeDims(dims2);
    CubeSignature sig1(dims1), sig2(dims2);


    // Allocate space for the mapping
    map2array.SetLength(sig1.getSize());

    // Combine the generator perms to a single permutation over the cube
    for (long i=0; i<map2array.length(); i++) {
      long t=0;
      for (long j1=0; j1<trees.length(); j1++) {
	long j2 = trees[j1].getAuxKey();
	long digit = sig1.getCoord(i,j1); // the j1 digit of i in base dims
	digit = genMappings[j1][digit];   // apply the j1 permutation to it
	t += digit * sig2.getProd(j2+1);  // adds the permuted digit

      }
      map2array[i] = t;
    }
  }

  // Compute the inverse permutation
  map2cube.SetLength(map2array.length());
  for (long i=0; i<map2array.length(); i++) map2cube[ map2array[i] ] = i;
}

// Prints out a column permutation
ostream& operator<< (ostream &s, const ColPerm& p)
{
  Permut pp;
  p.makeExplicit(pp);
  return s << pp;
}

// Prints out a sub-dimension
ostream& operator<< (ostream &s, const SubDimension& sd)
{
  s << (sd.good? "(g ": "(b ") << sd.size << " " << sd.e << ")";
  if (sd.frstBenes.length()>0 || sd.scndBenes.length()>0)
    s  << sd.frstBenes << sd.scndBenes;
  return s;
}

// Prints out the vector of trees
ostream& operator<< (ostream &s, const GeneratorTrees &trees)
{
  s << "[" << trees.depth << "\n";
  for (long g=0; g<trees.numTrees(); g++) {
    const OneGeneratorTree &T = trees[g];
    s << " ["; T.printout(s); s<<"]\n";
  }
  return s << "]";
}
