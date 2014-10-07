Current high level idea:

    The very basics of HE - what is bootstrapping
    The "depth" of multiplications and shifts in HElib is limited to ~7: XOR is free in comparison 
    SIMONv1 could not do more than one round because CT shifts are as expensive as AND
    We can make shifts free using SIMD, but it requires a parallelizable block cipher mode
    Even then, we can only do 7 SIMON rounds in HElib since it contains a bitwise AND each round
    AES is good since it contains only substitution, table-lookup, and XOR (verify this)
    We can verify our implementations of SIMON and AES are correct with Cryptol


Non-writing tasks:

    Cryptol AES chapter
    Fix Getty's AES
    SIMON in Cryptol



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





Using HElib
-----------


We perform key expansion **in plaintext**. If we were constructing a two party protocol, the client
would be doing key expansion before encrypting the key and sending it to the server.




First Attempt
-------------

* noise after 1 round

SIMD
----

A note about Block Cipher Modes
-------------------------------



