Block Ciphers, Homomorphically
------------------------------
by Brent Carmer, Tom DuBuisson, and David W. Archer, PhD

Our team at Galois, Inc. is interested in making secure computation practical. A lot of our secure
computation work has focused on linear secret sharing (LSS, a form of multi-party computation) and
the platform we've built on that technology. However, we've also done a fair bit of comparison
between LSS, garbled circuit approaches, and homomorphic encryption (HE). We recently noticed that
Shai Halevi and Victor Shoup's open source homomorphic encryption library
[HElib](https://github.com/shaih/HElib) was just waiting for someone to implement some interesting
block ciphers. In this post, we talk about our experience implementing and evaluating performance of
the [SIMON block cipher](https://eprint.iacr.org/2013/404) in HElib.

In homomorphic encryption (HE), a user encrypts data and sends it to a single untrusted server. That
server, which does not hold the encryption key, computes on the encrypted data and returns an
encrypted answer to the user. Unfortunately, making HE practical is challenging. HE is very much
(many orders of magnitude) slower than computing the same result "in the clear". Typical HE
ciphertexts are far (thousands to millions of times) bigger than the plaintexts they represent. Each
step in HE computation accumulates noise that eventually makes the plaintext unrecoverable unless
extra time-consuming steps (informally called bootstrapping) are taken. When these steps are not
taken, HE cryptosystems are typically called somewhat homomorphic (SHE for short).

When bootstrapping is used, more complex computations can be performed. Such cryptosystems are
typically called fully homomorphic (FHE for short). Even with such challenges, the promise of HE is
compelling, particularly where mobile devices may have insufficient computational power, cloud-based
servers may be readily used to outsource such computation, and users are not prepared to trust those
servers with their (plaintext) data.

As of this posting, HElib as available on github falls into the SHE category. Shai and Victor have
indicated that they plan to make bootstrapping (and thus FHE) available in a few weeks. To gain
experience using HElib, we thought to implement a member of the SIMON block cipher family. SIMON is
a new family of lightweight block ciphers released by the [NSA](http://eprint.iacr.org/2013/404.pdf)
in 2013. We implemented SIMON with 64 bit block size and 128 bit key size. The SIMON specification
calls for 44 processing "rounds" in SIMON 64/128, which we were able to implement the cipher using the
current (somewhat homomorphic) version of HElib.

The most expensive operation in homomorphic encryption is ciphertext multiplication. Most of our
work on SIMON is related to reducing the number of multiplications. We have a cool optimization to
this effect (discussed below), which allows us to achieve excellent performance.

The SIMON algorithm is shown below encoded in [Cryptol](http://cryptol.net/), a domain-specific
language for expressing cryptographic algorithms developed by Galois and widely used in some
government agencies. Cryptol is designed to express cryptographic algorithms at a level of
abstraction very close to mathematical specification, to minimize the likelihood of error. The
Cryptol tool suite allows for verification that implementations match a Cryptol description, and
also allows for deriving certain implementations from Cryptol specifications. 

Using Cryptol's support for SAT solvers, the SIMON implementation has had various properties proven
about it including no weak keys, injectivity of key expansion, and the identity of decryption
composed with encryption.

>     [simon.cry](https://github.com/GaloisInc/helib-demos/blob/master/simon.cry)
>
>     -- encRound is the heart of SIMON. 
>     -- It takes a key and a 64b block, chopped up into two 32b chunks.
>     -- As with all Feistel ciphers, at each round we swap the chunks 
>     -- and only manipulate one of them.
>     encRound : [32] -> ([32], [32]) -> ([32], [32])
>     encRound k (x, y) = (y ^ f x ^ k, x)
>
>     -- f is a helper function that performs rotations and xors over 
>     -- a chunk of input.
>     f : [32] -> [32]
>     f x = ((x <<< 1) && (x <<< 8)) ^ (x <<< 2)
>
>     -- encrypt performs one encRound on the input for each of the keys
>     encrypt : [4][32] -> ([32], [32]) -> ([32], [32])
>     encrypt k0 b0 = bs ! 0
>       where
>         bs = [b0] # [ encRound k b | b <- bs | k <- ks ]
>         ks = expandKey k0

Note: find the whole specification in [simon.cry](https://github.com/GaloisInc/helib-demos/blob/master/simon.cry)

This specification is what we set out to implement, using HElib as a platform. Ciphertexts in HElib
are composed of vectors of elements of certain rings. For our implementation, we use Ring(2). Thus
each element in the ciphertext vector represents a single bit. HElib also supports the notion of
"packing" multiple plaintexts into a single ciphertext, and computing on these in parallel, in a
SIMD-like paradigm. The number of plaintexts packable into a ciphertext, which we call nSlots, is
impacted by a number of parameters, including the maximum allowable computation depth of the
circuit, which we call L. As we vary L to allow for more computation and parallelism, we also affect
the cost of the computation in the form of the size of cryptographic keys used at each level in the
Boolean circuit.

A first attempt
===============

Representing SIMON ciphertext blocks. 
-------------------------------------

Blocks for the SIMON block cipher are 64b in size, but must be manipulated as two 32b halves
(typical of Feistel ciphers). Because of this natural structure, we first implemented ciphertexts as
two vectors of 32b each. In our parameter selection, nSlots is far larger than 32b, so we padded the
ciphertexts with zeroes, essentially not taking advantage of packing at all. We represent these
vectors as follows:

[simon-blocks.cpp](https://github.com/GaloisInc/helib-demos/blob/master/src/simon-blocks.cpp)

>     // a plaintext block is simply two 32b unsigned integers
>     struct pt_block {
>         uint32_t x;
>         uint32_t y;
>     };
>
>     // we encrypt each half by itself
>     Ctxt heEncrypt(const FHEPubKey& k, uint32_t x) {
>         vector<long> vec = uint32ToBits(x); // helib needs a vector<long>, even though this
>         pad(0, vec, global_nslots);         // returns a vector of bits
>         Ctxt c(k);
>         global_ea->encrypt(c, k, vec);
>         return c;
>     }
>
>     // then, a secret block is simply two ciphertexts
>     struct heBlock {
>         Ctxt x;
>         Ctxt y;
>     };

Processing on SIMON blocks
--------------------------

The SIMON algorithm requires use of addition in GF(2) (that is, XOR), multiplication in GF(2) (AND),
negation, and both left and right rotations. HElib provides the required addition and multiplication
primitives. Then when we "add" two ciphertexts, HElib performs pairwise XOR of their respective
vector elements, homomorphically of course. However, we must create the negation function, which we
implement by bitwise XOR with a vector of all ones:

[simon-blocks.cpp](https://github.com/GaloisInc/helib-demos/blob/master/src/simon-blocks.cpp)

>    // x is the ciphertext, global_maxint is the all 1's vector
>    void negate32(Ctxt &x) { 
>        x += \*global_maxint;
>    }

Because HElib is limited to shift operations, we must also create the required rotation functions.
Because ciphertexts have nSlots elements and we care only about the first 32 of those. Because this
is a bit tricky, we use Cryptol to specify our rotation approach and prove its correctness.

[simon-blocks.cpp](https://github.com/GaloisInc/helib-demos/blob/master/src/simon-blocks.cpp)
[rotation.cry](https://github.com/GaloisInc/helib-demos/blob/master/rotations.cry)

>
>     // this algorithm has been verified in Cryptol in rotation.cry
>     void rotateLeft32(Ctxt &x, int n) {
>         Ctxt other = x;
>         global_ea->shift(x, n);
>         global_ea->shift(other, -(32-n));
>         negate32(x);                          // x |= other. must do demorgan's law manually
>         negate32(other);                      // since we do not have bitwise OR in HElib
>         x.multiplyBy(other);
>         negate32(x);
>     }

At this point, we have all the basic functions we need to build SIMON. Next, we need to implement
the three main functions on which SIMON depends: encRound and expandKey. encrypt is simply a simple
composiiton of these two functions. Implementing encRound is straightforward and follows directly
from the Cryptol:

[simon-blocks.cpp](https://github.com/GaloisInc/helib-demos/blob/master/src/simon-blocks.cpp)

>     void encRound(Ctxt key, heBlock &inp) {
>         Ctxt tmp = inp.x;
>         Ctxt x0  = inp.x;
>         Ctxt x1  = inp.x;
>         Ctxt x2  = inp.x;
>         Ctxt y   = inp.y;
>         rotateLeft32(x0, 1);
>         rotateLeft32(x1, 8);
>         rotateLeft32(x2, 2);
>         x0.multiplyBy(x1);
>         y    += x0;
>         y    += x2;
>         y    += key;
>         inp.x = y;
>         inp.y = tmp;
>     }

Note: expandKey we won't address here, because we perform key expansion in plaintext and then
homomorphically encrypt the key.

Evaluating our first approach. We evaluated this approach by testing performance and ability to
complete SIMON without exceeding the allowable circuit depth. In one experiment, the computation
took unreasonable time: 14 hours for a single block at a circuit depth (L=80) that allowed the
computation to finish all rounds correctly. In another experiment, the computation was much faster,
but we were unable to complete it correctly, completing 10 rounds of the required 44 in 500 seconds
with L=16. Thus we concluded that this first approach was unworkable in practice.

Our second approach:
--------------------

Next, we adopted an idea used by [Smart et al.](https://eprint.iacr.org/2012/099) to achieve
concurrency in leveled homomorphic AES implementations. This idea, called "bit-slicing", interleaves
individual bits of multiple plaintexts. 

First, we select the same bit from each plaintext, and form a vector of those bits. For example, a
vector is formed by selecting the first bit of each plaintext in a group of plaintexts. Next, each
vector is homomorphically encrypted, which comes naturally in the HElib paradigm. Then we form a
vector of the resulting ciphertexts. This vector thus contains the entirety of the ciphertexts for
all included plaintexts. If you select the nth ciphertext from this vector, it contains the
encrypted nth bit of each packed plaintext.

While this approach was proposed by the Smart et al. to achieve concurrency with appropriate
computing resources, we use it for a different purpose. The highest cost computational primitive in
our naive HElib SIMON implementation is rotation. Rotation is expensive because (as shown in
[simon-blocks.cpp](https://github.com/GaloisInc/helib-demos/blob/master/src/simon-blocks.cpp)) it
requires bitwise OR, which in turn requires multiplication. 

Each of the 44 rounds in SIMON requires three such rotations in addition to the other
multiplications and additions used. Because addition is inexpensive and there is only a single other
multiplication per round, rotation dominates computational cost. The bit-slicing approach allows us
to rotate "for free" by simply permuting indices in the vector of ciphertexts. Thus the
multiplication involved in rotation is eliminated, reducing the number of multiplications per round
of SIMON from 4 to 1. You can see the resulting implementation at
[simon-simd.cpp](https://github.com/GaloisInc/helib-demos/blob/master/src/simon-simd.cpp).

With this new approach and a selection of L=23, we were successful at completing all rounds of SIMON
without the need for recryption, and doing so in reasonable time. We showed our implementation
running at 126 seconds per round. Thus all 44 rounds are completed in 1 hour and 52 minutes. This
compares favorably to our naive implementation that required 14 hours. In addition, L=23 (along with
our choices for other parameters) allows us nSlots of up to 1800. Thus processing 1800 64b blocks
concurrently, we achieved performance averaging 70ms per round, or 3.1 seconds per block.

HElib provides an interesting framework for experimenting with homomorphic encryption. Block ciphers
are a popular application for such encryption. For example, AES has been often used as a benchmark
for HE, garbled circuit, and linear secret sharing platforms for several years, including the Galois
implementation of AES on our linear secret sharing ShareMonad platform (that for some time was the
fastest known implementation of secure AES-128 computation at 3ms per block [LAD12]). Our work on
implementing SIMON on HElib without recryption and focusing on performance adds to this body of work
the first known implementation of a modern block cipher using this library. We have made our code
base available at [github](https://github.com/GaloisInc/helib-demos). In the future, we aim to take
advantage of the upcoming recryption capability in HElib to implement and study AES in this
framework.

The authors greatly appreciate the time and effort of Tom DuBussion and Getty Ritter of Galois, Inc.
in helping with implementation.

[LAD12] J. Launchbury, A. Adams-Moran, and I. Diatchki, Efficient Lookup-TableProtocol in Secure
Multiparty Computation. In Proc.  International Conference on Functional Programming (ICFP), 2012.
