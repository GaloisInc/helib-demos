// this is the example from Tom's blog

#include <ctime>
#include "FHE.h"
// #include "FHEContext.h"
#include "EncryptedArray.h"

void timer(bool init = false) {
    static time_t old_time;
    static time_t new_time;
    if (!init) {
        old_time = new_time;
    }
    new_time = std::time(NULL);
    if (!init) {
        cout << (new_time - old_time) << "s" << endl;
    }
}

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
    Ctxt ct(publicKey);
    ea.encrypt(ct, publicKey, v1);
    for (int i = 0; i < 100; i++) {
        cout << "shift#" << i << "..." << flush;
        timer(true);
        ea.shift(ct, 1);
        timer();
        vector<long> res;
        ea.decrypt(ct, secretKey, res);
        for (int i = 0; i < res.size(); i ++) {
            if (i > 0 && i%4==0) cout << " ";
            cout << res[i];
        }
        cout << endl;
    }
    return 0;
}
