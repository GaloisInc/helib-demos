All of the files quoted in this blog post are available at github for you to hack on.

What is Homomorphic Encryption
------------------------------

Homomorphic Encryption is a variety of secure computation where a single party can compute a
function on a secret without finding out what that secret is. Which is magical.
[HElib](https://github.com/shaih/HElib) is an implementation of homomorphic encryption in C++. At
present, it's computation depth is limited. It doesn't support an operation called "bootstrapping"
that provides unlimited depth computation (HE with bootstrapping is known as "fully" homomorphic
encryption).

SIMON the algorithm
-------------------

Wishing to experiment in HElib, and knowing that AES has been accomplished before, we thought we'd
practice on a simpler block cipher.  SIMON and SPECK are two new families of lightweight block
ciphers released by the [NSA](http://eprint.iacr.org/2013/404.pdf) in 2013. SIMON is designed for
optimal implementation in hardware, and SPECK is designed for optimal implementation in software.

We implemented SIMON with 64 bit block size and 128 bit key size. The specs call for 44 rounds in
SIMON 64/128. However, as we'll show below, this cannot be accomplished using the current version of
HElib.

What follows is a definition of SIMON 64/128 in [Cryptol](http://cryptol.net/). Cryptol is a
wonderful way to think about and verify cryptographic algorithms.

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

As you can see, SIMON isn't at all complicated. It's expandKey subroutine (not pictured here) is the
most complex part. We won't be expanding keys homomorphically, though (we'll do that in plaintext).

Using HElib - encrypting stuff
------------------------------

Ciphertexts in HElib are represented by the class "Ctxt". Ctxts are created from vectors of longs.
The number of "slots" is determined at runtime. HElib supports operations over certain Rings. We use
Ring\_2. Then, in our implemenation, each long represents a bit. To pack a Ctxt, we provide HElib
with a vector<long>.

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
block into two vectors with 32 bits in each, then padded them with zeroes up to n.

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
multiplication is equivalent to AND. And shifts are shifts: multiplication by powers of two. You can
see the coolness of homomorphic encryption coming through in how HElib overloads "+=" and "\*=" for
ciphertexts (although we recommend using Ctxt.multiplyBy() instead of "\*="). In Ring\_2, addition
is XOR, and multiplication is AND. 

We can derive NOT for bits by XORing with 1. For 32 bit numbers, we XOR each bit with 1.
Equivalently, we XOR the whole thing by 0xFFFFFFFF.

>    [simon-blocks.cpp]
>
>    void negate32(Ctxt &x) {
>        x += *global_maxint; // where maxint is 0xFFFFFFFF
>    }
>    

In order to do a rotation, we shift the Ctxt left by n, and right by 32-n, and bitwise-OR the
result. Note that Ctxts have more than 32 slots, but we only care about the first 32, so this was
tricky. We can derive OR by using DeMorgan's law on AND.

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

We perform key expansion **in plaintext**. If we were constructing a two party protocol, the client
would be doing key expansion before encrypting the key and sending it to the server.

Unfortunately, this implementation suffers from an extreme drawback. It performs 4 multiplications
per round (one in encRound and one in each of the three rotations). The first 10 rounds perform
correctly:

>    [logs/simon-blocks.log]
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

>    [logs/simon-blocks.log]
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

At first, this was a strange bug to encounter. We questioned whether we had implemented homomorphic
SIMON properly. Frankly, we had no idea how far SIMON would go before hitting the noise threshold.
However, it is easy to create a test to find the maximum number of multiplications.

Finding the maximum number of multiplications
---------------------------------------------

We simply encrypt 1 and square it repeatedly. Each round we decrypt and print the leftmost 32
bits.

>    [multest.cpp]
>
>    // how many multiplications can we do without noise?
>    Ctxt one = heEncrypt(pubkey, 1);
>    for (int i = 1; i <= 50; i++) {
>        cout << "mul#" << i << "..." << flush;
>        ///////////
>        one.multiplyBy(one);
>        ///////////
>        vector<long> res = heDecrypt(seckey, one);
>        for (int j = 32; j >= 0; j--) {
>            if (j > 0 && j%4==0) cout << " ";
>            cout << res[j];
>        }
>        cout << endl;
>    }

The result is striking.

>    [logs/multest.log]
>
>    mul#28... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#29... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#30... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#31...multest1: Ctxt.cpp:138: void Ctxt::modDownToSet(const IndexSet&): 
>                       Assertion `!empty(intersection)' failed.

So, we get roughly 30 multiplications before HElib blows up. We needed to reduce the number of
multiplications SIMON uses as much as possible.

Bit-slicing
-----------

We we were able to increase the number of rounds to 32 by encrypting a single bit per Ctxt. Then a
block is simply a vector<Ctxt> of length 32.  Where homomorphic rotation costs one multiplication,
this optimization gives us rotations for free in plaintext as simple vector operations.
Unfortunately, there is one remaining multiplication - the one required by the SIMON round function
as a bitwise-AND. 

A side effect of this is that now we can layer blocks on top of each other, and perform SIMON in
parallel over many blocks (limited by the number of slots in a Ctxt - usually around 400). I think
this is a cool optimization, despite HElib's limitations.

There is much data mangling involved, but I won't bore you with it (please check out
[simon-simd.cpp] for the implementation). The result is that we can execute more than three times as
many rounds (which makes sense - before there were 4 multiplications per round, now there's 1).

>    [logs/simon-simd.log]
>
>    inp = "secrets! very secrets!"

Note that inp is more than one block of ascii. Again, we run a round of SIMON, decrypt the result,
and compare it to whtat the plaintext version of SIMON produced. We then decrypt the homomorphic
result with plaintext SIMON.

>    [logs/simon-simd.log]
>
>    Round 31/33...41s
>    decrypting...34s
>    block0    : 0xbe359882 0xdb1279a4
>    should be : 0xbe359882 0xdb1279a4
>    decrypted : "secrets! very secrets!" 
>
>    Round 32/33...42s
>    decrypting...34s
>    block0    : 0x7d220d6e 0xbe359882
>    should be : 0x779de46f 0xbe359882
>    decrypted : [garbage]
>
>    Round 33/33...Ctxt::findBaseSet warning: already at lowest level

Results
-------

A plaintext C++ implementation of SIMON running only 32 rounds achieves speeds of 61ns/round.

The "blocks" HElib implementation runs at 49s/round, close to a billion times slower.

The SIMD HElib implementation runs at 61s/round. If we amortize over the number of Ctxt slots (480),
we get 127ms/round.
