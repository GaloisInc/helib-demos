// this is the example from Tom's blog

#include "FHE.h"
// #include "FHEContext.h"
#include "EncryptedArray.h"
#include <NTL/lzz_pXFactoring.h>
#include <fstream>
#include <sstream>
#include <sys/time.h>


int main(int argc, char **argv) {
    cout << "starting ..." << endl;
    long m=0, p=2, r=1;
    long L=16;
    long c=3;
    long w=64;
    long d=0;
    long security = 128;
    ZZX G;
    cout << "finding m ...";
    m = FindM(security,L,c,p,d,0,0);
    cout << m << endl;
    FHEcontext context(m, p, r);
    cout << "created context..." << endl;
    buildModChain(context, L, c);
    cout << "buildModChain..." << endl;
    FHESecKey secretKey(context);
    const FHEPubKey& publicKey = secretKey;
    G = context.alMod.getFactorsOverZZ()[0];
    secretKey.GenSecKey(w);
    addSome1DMatrices(secretKey);
    cout << "generated key" << endl;

    EncryptedArray ea(context, G);
    PlaintextArray pa(ea);
    long nslots = ea.size();

    srand(time(NULL));

    vector<long> v1;
    for (int i = 0; i < nslots; i++) {
        if (i == 0) {
            v1.push_back(1);
        } else {
            v1.push_back(0);
        }
    }

    // how many multiplications can we do without noise?
    Ctxt ct1(publicKey);
    ea.encrypt(ct1, publicKey, v1);
    for (int i = 0; i < 100; i++) {
        cout << "mul#" << i << endl;
        ct1 *= ct1; 
        vector<long> res;
        ea.decrypt(ct1, secretKey, res);
        for (int i = 0; i < 16; i ++) {
            if (i > 0 && i%4==0) cout << " ";
            cout << res[i];
        }
        cout << endl;
    }
    return 0;
}
