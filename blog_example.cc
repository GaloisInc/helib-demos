// this is the example from Tom's blog

#include "FHE.h"
// #include "FHEContext.h"
#include "EncryptedArray.h"
#include <NTL/lzz_pXFactoring.h>
#include <fstream>
#include <sstream>
#include <sys/time.h>

int main(int argc, char **argv)
{
  cout << "starting ..." << endl;
  long m=0, p=2, r=1;
  long L=16;
  long c=3;
  long w=64;
  long d=0;
  long security = 128;
  ZZX G;
  m = FindM(security,L,c,p,d,0,0);
  cout << "generated m ..." << endl;
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
  long nslots = ea.size();

  vector<long> v1;
  for (int i = 0; i < nslots; i++) {
    v1.push_back(i*2);
  }
  Ctxt ct1(publicKey);
  ea.encrypt(ct1, publicKey, v1);

  vector<long> v2;
  Ctxt ct2(publicKey);
  for (int i = 0; i < nslots; i++) {
    v2.push_back(i*3);
  }
  ea.encrypt(ct2, publicKey, v2);

  Ctxt ctSum  = ct1;
  Ctxt ctProd = ct2;

  ctSum += ct2;
  ctProd *= ct2;

  vector<long> res;
  ea.decrypt(ctSum, secretKey, res);

  cout << "All computations are modulo " << p << "." << endl;
  for (int i = 0; i < res.size(); i ++) {
    cout << v1[i] << " + " << v2[i] << " = " << res[i] << endl;
  }

  ea.decrypt(ctProd, secretKey, res);
  for (int i = 0; i < res.size(); i++) {
    cout << v1[i] << " * " << v2[i] << " = " << res[i] << endl;
  }

  return 0;
}
