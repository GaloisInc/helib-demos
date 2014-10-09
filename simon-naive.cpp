#include "simon-plaintext.h"
#include "simon-util.h"

EncryptedArray* global_ea;
Ctxt* global_maxint;
Ctxt* global_one;
Ctxt* global_zero;
long* global_nslots;
const FHEPubKey* global_pubkey;

struct heBlock {
    Ctxt x;
    Ctxt y;
};

void negate32(Ctxt &x) {
    x += *global_maxint;
}

void rotateLeft(Ctxt &x, int n) {
    Ctxt other = x;
    global_ea->shift(x, n);
    global_ea->shift(other, -(32-n));
    negate32(x);
    negate32(other);
    x.multiplyBy(other);
    negate32(x);
}

void encRound(Ctxt key, heBlock &inp) {
    Ctxt tmp = inp.x;
    Ctxt x0  = inp.x;
    Ctxt x1  = inp.x;
    Ctxt x2  = inp.x;
    Ctxt y   = inp.y;
    rotateLeft(x0, 1);
    rotateLeft(x1, 8);
    rotateLeft(x2, 2);
    x0.multiplyBy(x1);
    y    += x0;
    y    += x2;
    y    += key;
    inp.x = y;
    inp.y = tmp;
}

// lifts a uint32_t into the HE monad
Ctxt heEncrypt(uint32_t x) {
    vector<long> vec = uint32ToBits(x);
    pad(0, vec, *global_nslots);
    Ctxt c(*global_pubkey);
    global_ea->encrypt(c, c.getPubKey(), vec);
    return c;
}

uint32_t heDecrypt (Ctxt c, FHESecKey k) {
    vector<long> vec;
    global_ea->decrypt(c, k, vec);
    return vectorTo32(vec);
}

// lifts a string into the HE monad
vector<Ctxt> heEncrypt (string s) {
    vector<vector<long>> pt = strToVectors(s);
    vector<Ctxt> cts;
    for (int i = 0; i < pt.size(); i++) {
        pad(0, pt[i], *global_nslots);
        Ctxt c(*global_pubkey);
        global_ea->encrypt(c, *global_pubkey, pt[i]);
        cts.push_back(c);
    }
    return cts;
}

vector<Ctxt> heEncrypt (vector<uint32_t> k) {
    vector<vector<long>> kbits = keyToVectors(k, *global_nslots);
    vector<Ctxt> encryptedKey;
    for (int i = 0; i < kbits.size(); i++) {
        pad(0, kbits[i], *global_nslots);
        Ctxt kct(*global_pubkey);
        global_ea->encrypt(kct, *global_pubkey, kbits[i]);
        encryptedKey.push_back(kct);
    }
    return encryptedKey;
}

vector<vector<long>> heDecrypt (vector<Ctxt> cts, FHESecKey k) {
    vector<vector<long>> res (cts.size());
    for (int i = 0; i < cts.size(); i++) {
        global_ea->decrypt(cts[i], k, res[i]);
    }
    return res;
}

int main(int argc, char **argv)
{
    string inp = "secrets!";
    cout << "inp = \"" << inp << "\"" << endl;
    //key k = genKey();
    vector<pt_key32> k ({0x1b1a1918, 0x13121110, 0x0b0a0908, 0x03020100});
    pt_expandKey(k);
    vector<pt_block> bs = strToBlocks(inp);
    printKey(k);
    
    long m=0, p=2, r=1;
    long L=16;
    long c=3;
    long w=64;
    long d=0;
    long security = 128;
    ZZX G;
    cout << "Finding m...";
    m = FindM(security,L,c,p,d,0,0);
    cout << m << endl;
    cout << "Generating context..." << endl;
    FHEcontext context(m, p, r);
    cout << "Building mod-chain..." << endl;
    buildModChain(context, L, c);
    cout << "Generating keys..." << endl;
    FHESecKey secretKey(context);
    const FHEPubKey& publicKey = secretKey;
    G = context.alMod.getFactorsOverZZ()[0];
    secretKey.GenSecKey(w);
    addSome1DMatrices(secretKey);

    EncryptedArray ea(context, G);
    long nslots = ea.size();

    // set up globals
    global_nslots = &nslots;
    global_ea     = &ea;
    global_pubkey = &publicKey;

    // heEncrypt uses globals
    Ctxt maxint   = heEncrypt(0xFFFFFFFF);
    Ctxt zero     = heEncrypt(0);
    Ctxt one      = heEncrypt(1);
    global_maxint = &maxint;
    global_zero   = &zero;
    global_one    = &one;


    // test rotation
    //Ctxt t0 = heEncrypt(0x12345678);
    //Ctxt t1 = t0;
    //Ctxt t2 = t0;
    //rotateLeft(t0, 1);
    //rotateLeft(t1, 2);
    //rotateLeft(t2, 8);
    //uint32_t r0 = heDecrypt(t0, secretKey);
    //uint32_t r1 = heDecrypt(t1, secretKey);
    //uint32_t r2 = heDecrypt(t2, secretKey);
    //printf("0x%08x 0x%08x 0x%08x\n", r0, r1, r2);
    //return 0;

    // HEencrypt key
    cout << "Encrypting SIMON key..." << endl;
    vector<Ctxt> encryptedKey = heEncrypt(k);

    // HEencrypt input
    cout << "Encrypting inp..." << endl;
    vector<Ctxt> cts = heEncrypt(inp);

    // test encRound
    cout << "Running protocol..." << endl;
    heBlock heb = {cts[0], cts[1]};
    for (int i = 0; i < T; i++) {
        timer(true);
        cout << "Round " << i+1 << "/" << T << "..." << flush;
        encRound(encryptedKey[i], heb);
        timer();

        vector<vector<long>> res (2);
        ea.decrypt(heb.x, secretKey, res[0]);
        ea.decrypt(heb.y, secretKey, res[1]);
        printf("result    : 0x%08x%08x\n", vectorTo32(res[0]), vectorTo32(res[1]));

        vector<pt_block> bs0 = strToBlocks(inp);
        pt_block b = bs0[0];
        for (int j = 0; j <= i; j++) {
            b = pt_encRound(k[i], b);
        }
        printf("should be : 0x%08x%08x\n", b.x, b.y);
    }
    return 0;
}
