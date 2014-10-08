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

SIMON the Algorithm
-------------------

SIMON and SPECK are two new families of lightweight block ciphers released by the NSA in 2013. SIMON
is designed for optimal implementation in hardware, and SPECK is designed for optimal implementation
in software.

http://eprint.iacr.org/2013/404.pdf

We implement SIMON with 64 bit block size and 128 bit key size. The specs call for 44 rounds of the
aptly named "round" function in SIMON 64/128. 


* algorithm description - in Cryptol

Using HElib
-----------


We perform key expansion **in plaintext**. If we were constructing a two party protocol, the client
would be doing key expansion before encrypting the key and sending it to the server.


* Setting up HElib - compiling NTL, pointing to the right directories, makefiles, etc.

First Attempt
-------------

Ciphertexts in HElib are created from vectors of longs. The number of slots is determined at
runtime, hereby refered to as **n**. My first implementation uses Ring_2. Each long then represents
a bit. So, we provided HElib a vector with n bits. Given that blocks in SIMON 64/128 come in chunks
of 32, we converted each block into two vectors with 32 bits in each, then padded them with 0s up to
n. Note that Ctxt refers to a ciphertext under homomorphic encryption in HElib.

>    struct block {
>        uint32_t x;
>        uint32_t y;
>    };
>    
>    struct heBlock {
>        Ctxt x;
>        Ctxt y;
>    };

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

HElib implements addition, multiplication, and shifting on Ctxts. These are mutations that occur
pairwise with the argument Ctxt. Like a zip. In Ring_2 this means that addition is equivalent to XOR
and multiplication is equivalent to AND. And shifts are shifts: multiplication by powers of two. You
can see the coolness of homomorphic encryption coming through in how HElib overloads "+=" and "\*="
for ciphertexts.

>    void rotateLeft(Ctxt &x, int n) {
>        Ctxt other = x;
>        global_ea->shift(other, n);
>        global_ea->shift(x, -(32 - n));
>        x += other;
>    }
>    
>    void encRound(Ctxt key, heBlock &inp) {
>        Ctxt tmp = inp.x; // make copies of the input
>        Ctxt x0  = inp.x;
>        Ctxt x1  = inp.x;
>        Ctxt x2  = inp.x;
>        Ctxt y   = inp.y;
>        rotateLeft(x0, 1);
>        rotateLeft(x1, 8);
>        rotateLeft(x2, 2);
>        x0   *= x1;
>        y    += x0;
>        y    += x2;
>        y    += key;
>        inp.x = y;
>        inp.y = tmp;
>    }

Unfortunately, this representation of blocks suffers from an extreme drawback. It performs 7
multiplications (or shifts) per round. The first round performs admirably:

>    Running protocol...
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

At first, this was a strange bug to encounter. We had no idea how far SIMON would go before hitting
the noise threshold where HElib can no longer decrypt properly. HElib provided no error or warning.
However, it is easy to create a test to find the maximum depth.

Finding the maximum depth
-------------------------

In order to see how many multiplications we can do safely, we simply encrypt 1 and square it
repeatedly. Each round we decrypt and print. Note that many details about the use of HElib are
abstracted away for the sake of demonstration.

>    // how many multiplications can we do without noise?
>    Ctxt ct1 = heEncrypt(1);
>    for (int i = 0; i < 100; i++) {
>        cout << "mul#" << i << endl;
>        ct1 *= ct1; 
>        vector<long> res = heDecrypt(ct1);
>        printVectorAsBits(res, 16);
>    }

The result is striking.

>    mul#0...1s
>    1000 0000 0000 0000
>    mul#1...0s
>    1000 0000 0000 0000
>    mul#2...3s
>    1000 0000 0000 0000
>    mul#3...8s
>    1000 0000 0000 0000
>    mul#4...28s
>    1000 0000 0000 0000
>    mul#5...105s
>    1000 0000 0000 0000
>    mul#6...407s
>    1010 0010 0110 1111

We perform a similar computation for shifts.

SIMD
----

We were able to increase the number of rounds to 6 by a SIMD optimization.

A note about Block Cipher Modes
-------------------------------



