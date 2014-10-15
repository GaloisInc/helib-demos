All of the files quoted in this blog post are available at github for you to hack on.

What is Homomorphic Encryption?
-------------------------------

Homomorphic Encryption is a variety of secure computation where a single party can compute a
function on a secret without finding out what that secret is. Which is magical.
[HElib](https://github.com/shaih/HElib) is an implementation of homomorphic encryption in C++. At
present, its computation depth is limited. It doesn't support an operation called "bootstrapping"
that provides unlimited depth computation (HE with bootstrapping is known as "fully" homomorphic
encryption).

SIMON the algorithm
-------------------

Wishing to experiment in HElib, we thought we'd practice on a simpler block cipher.  SIMON and SPECK
are two new families of lightweight block ciphers released by the
[NSA](http://eprint.iacr.org/2013/404.pdf) in 2013. SIMON is designed for optimal implementation in
hardware, and SPECK is designed for optimal implementation in software.

We implemented SIMON with 64 bit block size and 128 bit key size. The specs call for 44 rounds in
SIMON 64/128. However, as we'll show below, this cannot be accomplished using the current version of
HElib.

What follows is the encryption algorithm of SIMON 64/128 implemented in
[Cryptol](http://cryptol.net/). (Cryptol is a wonderful way to think about and verify cryptographic
algorithms.)

>    [simon.cry]
>
>    f : [32] -> [32]
>    f x = ((x <<< 1) && (x <<< 8)) ^ (x <<< 2)
>
>    encRound : [32] -> ([32], [32]) -> ([32], [32])
>    encRound k (x, y) = (y ^ f x ^ k, x)
>
>    encrypt : [4][32] -> ([32], [32]) -> ([32], [32])
>    encrypt k0 b0 = bs ! 0
>      where
>        bs = [b0] # [ encRound k b | b <- bs | k <- ks ]
>        ks = expandKey k0

As you can see, SIMON isn't at all complicated. Its expandKey subroutine (not pictured here) is the
most complex part. We won't be expanding keys homomorphically, though. We'll do that in plaintext.

Using HElib - encrypting stuff
------------------------------

Ciphertexts in HElib are represented by the class "Ctxt". Ctxts are created from vectors of longs.
The number of "slots" is determined at runtime. HElib supports operations over certain Rings. We use
Ring\_2. Then, in our implementation, each long represents a bit. To pack a Ctxt, we provide HElib
with a vector<long>.

The most tweakable parameter for us is L, specifying the number levels of key switching HElib will
allow. The more it increases, the longer computation takes, and the more memory it consumes. It
also increases the depth of the computation that we can perform. If bootstrapping were implemented,
it would occur when the depth of computation reaches L. We'll come back to this.

If you're curious as to how one performs encryption in HElib, check out the following. Don't worry
to much about global\_ea: it's referring to HElib's EncryptedArray object, which encapsulates higher
level operations (like encryption and decryption). It's created when we initialize the protocol.

>    [simon-blocks.cpp]
>
>    vector<vector<long>> strToVectors(string inp, int nslots);
>
>    vector<Ctxt> heEncrypt (string s) {
>        vector<vector<long>> pt = strToVectors(s, *global_nslots);
>        vector<Ctxt> cts;
>        for (int i = 0; i < pt.size(); i++) {
>            Ctxt c(*global_pubkey);
>            global_ea->encrypt(c, *global_pubkey, pt[i]);
>            cts.push_back(c);
>        }
>        return cts;
>    }

First attempt at SIMON
----------------------

Given that blocks in SIMON 64/128 come in chunks of 32, our first implementation converted each
block into two vectors with 32 bits in each, then padded them with zeros up to nSlots.

>    [simon-blocks.cpp]
>
>    struct pt\_block {
>        uint32_t x;
>        uint32_t y;
>    };
>
>    struct heBlock {
>        Ctxt x;
>        Ctxt y;
>    };

HElib implements addition and multiplication on Ctxts. These are mutations that occur pairwise with
the argument Ctxt. Like a zip. In Ring\_2 this means that addition is equivalent to XOR and
multiplication is equivalent to AND. And shifts are shifts: multiplication by powers of two.  We can
derive NOT for bits by XORing with 1. For 32 bit numbers, we XOR each bit with 1.  Equivalently, we
XOR the whole thing by 0xFFFFFFFF.

>    [simon-blocks.cpp]
>
>    void negate32(Ctxt &x) {
>        x += *global_maxint; // where maxint is 0xFFFFFFFF
>    }
>

In order to do a rotation, we shift the Ctxt left by n, and right by 32-n, and bitwise-OR the
result. Note that Ctxts can have more than 32 slots, but we only care about the first 32, so this is
a bit tricky tricky. We can derive OR by using DeMorgan's law on AND.

>    [simon-blocks.cpp]
>
>    // this algorithm is verified in Cryptol in rotation.cry
>    void rotateLeft32(Ctxt &x, int n) {
>        Ctxt other = x;
>        global_ea->shift(x, n);
>        global_ea->shift(other, -(32-n));
>        negate32(x);                          // x |= other
>        negate32(other);
>        x.multiplyBy(other);
>        negate32(x);
>    }

Implementing encRound is straightforward and pretty much follows from the Cryptol.

>    [simon-blocks.cpp]
>
>    void encRound(Ctxt key, heBlock &inp) {
>        Ctxt tmp = inp.x;
>        Ctxt x0  = inp.x;
>        Ctxt x1  = inp.x;
>        Ctxt x2  = inp.x;
>        Ctxt y   = inp.y;
>        rotateLeft32(x0, 1);
>        rotateLeft32(x1, 8);
>        rotateLeft32(x2, 2);
>        x0.multiplyBy(x1);
>        y    += x0;
>        y    += x2;
>        y    += key;
>        inp.x = y;
>        inp.y = tmp;
>    }

We perform key expansion in plaintext before running any homomorphic encryption.

Unfortunately, this implementation suffers from an extreme drawback. It performs 4 multiplications
per round (one in encRound and one in each of the three rotations). With L=16, the first 10 rounds
perform correctly:

>    [logs/simon-blocks-L16.log]
>
>    inp = "secrets!"
>    Running protocol...
>
>    Round 1/33...51s
>    decrypting...0s
>    result    : 0xd7b9a590 0x73656372
>    should be : 0xd7b9a590 0x73656372
>    decrypted : "secrets!"
>
>    ...
>
>    Round 10/33...46s
>    decrypting...1s
>    result    : 0x34c2ec01 0x9a789380
>    should be : 0x34c2ec01 0x9a789380
>    decrypted : "secrets!"

But on the 11th round, HElib explodes:

>    [logs/simon-blocks-L16.log]
>
>    Round 11/33...Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    Ctxt::findBaseSet warning: already at lowest level
>    46s
>    decrypting...1s
>    result    : 0xffffffff 0x34c2ec01
>    should be : 0xcae7c570 0x34c2ec01
>    decrypted : [garbage]

Again, AND is equivalent to multiplication in Ring\_2, which is the noisy operation causing HElib to
bottom out. The homomorphic implementation of rotateLeft contains an AND. So we currently use four
ANDs per round.

If you think about what gets bitwise-ANDed each round, it's only half the block, or one Ctxt.
Checking out the Cryptol implementation of SIMON, we see that half the result has been affected by
AND and rotateLeft. The other half simply gets passed along as it came in.

>    [simon.cry]
>
>    f x = ((x <<< 1) && (x <<< 8)) ^ (x <<< 2)
> 
>    encRound k (x, y) = (y ^ f x ^ k, x)

We can see that it takes two rounds for the entire block to be affected by the ANDs.  With four
ANDs per round affecting only half the ciphertexts each round, we can say that the computation depth
increases roughly 2 levels per round.

Then, in order to do the full 44 rounds, we'll need to set L to somewhere around 88 (we were able to
get it to work with L=80).  Which is highly impractical. If we set L=80, each round takes around 1100
seconds, meaning that it takes almost 14 hours to do the full 44 rounds.

Bit-slicing
-----------

We were able to complete SIMON with L=22 by encrypting a single bit per Ctxt using an optimization
known as bit slicing. It reduces the noisy operations per round to an average of 1/2 (one
multiplication per block over two rounds).

We learned about bit slicing in a paper showing off a homomorphic implementation of AES
([GHS2012](https://eprint.iacr.org/2012/099)). The basic idea is to pack a single bit into each
ciphertext and treat a vector of ciphertexts as a number.  Then a block is simply two vector<Ctxt>'s
of length 32.  Where homomorphic rotation costs one multiplication, this optimization gives us
rotations for free, in plaintext, as simple vector operations.

A side effect of this is that now we can layer blocks on top of each other, and perform SIMON in
parallel over many blocks (limited by the number of slots in a Ctxt - usually around 400 for L=16).
We think this is a very cool optimization (and it has been used to great effect in a [FHE
implementation of AES](https://eprint.iacr.org/2012/099)), despite HElib's limitations.

There is much data mangling involved, but I won't bore you with it (please check out
[simon-simd.cpp](link) for the implementation). The result is that we can execute more than three
times as many rounds (which makes sense - before there were 4 multiplications per round, now there's
1). Unfortunately, we can't get rid of that last multiplication - it's the bitwise-AND of the
encRound function.

Now if we set L=23, we can do all 44 rounds. See [simon-simd.cpp](simon-simd.cpp) for details.

>    [logs/simon-simd-L23.log]
>
>    inp = "secrets! very secrets!"
>    Running protocol...
>    Round 1/44...148s
>    Round 2/44...169s
>    ...
>    Round 43/44...97s
>    Round 44/44...97s
>    decrypting...87s
>    block0    : 0x6e063402 0xb2cab069
>    should be : 0x6e063402 0xb2cab069
>    decrypted : "secrets! very secrets!" 

The total runtime was an hour and 52 minutes.

Results
-------

A plaintext C++ implementation of SIMON achieves speeds of 61ns/round.

The "blocks" HElib implementation runs at 49s/round with L=16, 80s/round with L=32, 1100s/round with
L=80, over a billion times slower. Furthermore L must be greater than 80 in order to complete the 44
rounds that SIMON calls for.

With L=23, the bit sliced HElib implementation runs at an average of 126s/round, and completes 44
rounds. If we amortize over the number of Ctxt slots (1800), it takes an average of 70ms/round!

Conclusion
----------

HElib is cool. So is bit slicing. As far as performance goes, we can play with the L parameter to
some degree, but it comes at great cost. We can tune the maximum depth to the minimum necessary for
our protocol, but a bootstrap operation would greatly extend the functionality of HElib.

Many thanks to Tom DuBuisson, Getty Ritter, and David Archer for their assistance and advice on this
project.
