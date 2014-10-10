Current high level idea:

    The very basics of HE - what is bootstrapping
    AES has been implemented in HE - we wanted to try a "simpler" block cipher - how about SIMON
    SIMONv1 could not do more than one round because CT shifts are as expensive as AND
    The "depth" of multiplications and shifts in HElib is limited to ~7: XOR is free in comparison 
    We can make shifts free using SIMD, but it requires a parallelizable block cipher mode
    Even then, we can only do 7 SIMON rounds in HElib since it contains a bitwise AND each round


Non-writing tasks:

    Cryptol AES chapter
    Fix Getty's AES


What is Homomorphic Encryption
==============================


HElib
-----

* homomorphic encryption, not quite FHE - no bootstrapping
* summary and links

* two party concept
* plaintext vs ciphertext computation

SIMON the algorithm
-------------------

Wishing to experiment in HElib, and knowing that AES has been acomplished before, we thought we'd
try implementing a simpler cipher.  SIMON and SPECK are two new families of lightweight block
ciphers released by the NSA in 2013. SIMON is designed for optimal implementation in hardware, and
SPECK is designed for optimal implementation in software.

http://eprint.iacr.org/2013/404.pdf

We implement SIMON with 64 bit block size and 128 bit key size. The specs call for 44 rounds in
SIMON 64/128. 

* algorithm description - in Cryptol

We perform key expansion **in plaintext**. If we were constructing a two party protocol, the client
would be doing key expansion before encrypting the key and sending it to the server.

Using HElib - encrypting stuff
------------------------------

Ciphertexts in HElib are the class "Ctxt". Ctxts are created from vectors of longs. The number of
slots is determined at runtime, hereby refered to as **n**. Our implementations use Ring_2. Each
long then represents a bit. 

So, to pack a Ctxt, one provides HElib a vector<long>, representing a vector of bits.

If you're curious as to how one performs encryption in HElib, check out the following. Don't worry
to much about global_ea: it's referring to HElib's EncryptedArray object, which encapsulates higher
level operations (like encryption and decryption). It's created when we initialize the prototcol.

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

First attempt
-------------

Given that blocks in SIMON 64/128 come in chunks of 32, our first implementation converted each
block into two vectors with 32 bits in each, then padded them with zeroes up to n.

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
is XOR, and multiplication is AND. We can derive NOT by XORing with 1. We can derive OR by using
DeMorgan's law on AND.

>    void negate32(Ctxt &x) {
>        x += *global_maxint; // where maxint is 0xFFFFFFFF
>    }
>    
>    // this algorithm for rotation is verified in Cryptol in rotation.cry
>    void rotateLeft32(Ctxt &x, int n) {
>        Ctxt other = x;
>        global_ea->shift(x, n);
>        global_ea->shift(other, -(32-n));
>        negate32(x);
>        negate32(other);
>        x.multiplyBy(other);
>        negate32(x);
>    }
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

Unfortunately, this representation of blocks suffers from an extreme drawback. It performs 4
multiplications per round. The first round performs admirably:

inp = "secrets!"

>    Round 1/2...21s
>    result    : 0xcd3950b563617473

What is going on here is that we're encrypting the input and the key, running a single round of
SIMON on it homomorphically, then decrypting. We can compare the result to simply running a single
round of plaintext SIMON on the input:

>    should be : 0xcd3950b563617473

So far so good, but on the second round, we see junk:

>    Round 2/2...30s
>    result    : 0x74125b2bcd3950b5
>    should be : 0x497ff99384761008

At first, this was a strange bug to encounter. We questioned whether we had implemented homomorphic
SIMON properly. Frankly, we had no idea how far SIMON would go before hitting the noise threshold
where HElib can no longer decrypt properly (something having to do with lattices and noise). HElib
provided no error or warning.  However, it is easy to create a test to find the maximum
multiplication depth.

Finding the maximum depth
-------------------------

In order to find out how many multiplications we can do, we can simply encrypt 1 and square it
repeatedly. Each round we decrypt and print the leftmost 32 bits.

>    // how many multiplications can we do without noise?
>    Ctxt one = heEncrypt(pubkey, 1);
>    for (int i = 1; i <= 10; i++) {
>        cout << "mul#" << i << "..." << flush;
>        ///////////
>        one *= one;
>        ///////////
>        vector<long> res = heDecrypt(seckey, one);
>        for (int j = 32; j >= 0; j--) {
>            if (j > 0 && j%4==0) cout << " ";
>            cout << res[j];
>        }
>        cout << endl;
>    }

The result is striking.

>    mul#1... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#2... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#3... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#4... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#5... 0000 0000 0000 0000 0000 0000 0000 00001
>    mul#6... 1010 1010 1001 0111 1011 1000 1000 11111

Bit-slicing
-----------

Despite knowing that we would not be able to perform the full 44 rounds that SIMON calls for, we we
were able to increase the number of rounds to 6 by encrypting a single bit per Ctxt. This means that
we can perform rotations in plaintext as simple vector operations. Since each rotation costs one
multiplication (in order to do bitwise-OR), the number of multiplications per round is reduced to
one. Unfortunately, this remaining multiplication is required by the SIMON round function as a
bitwise-AND.

We think this is a cool optimization, despite HElib's limitations.

Layering blocks on top of each other.
