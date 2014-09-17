
#include <NTL/lzz_p.h>

#include <NTL/new.h>

NTL_START_IMPL

zz_pInfoT *Build_zz_pInfo(FFTPrimeInfo *info)
{
   return NTL_NEW_OP zz_pInfoT(INIT_FFT, info);
}


zz_pInfoT::zz_pInfoT(long NewP, long maxroot)
{
   ref_count = 1;

   if (maxroot < 0) Error("zz_pContext: maxroot may not be negative");

   if (NewP <= 1) Error("zz_pContext: p must be > 1");
   if (NumBits(NewP) > NTL_SP_NBITS) Error("zz_pContext: modulus too big");

   ZZ P, B, M, M1, MinusM;
   long n, i;
   long q, t;

   p = NewP;

   pinv = 1/double(p);

   p_info = 0;
   p_own = 0;

   conv(P, p);

   sqr(B, P);
   LeftShift(B, B, maxroot+NTL_FFTFudge);

   set(M);
   n = 0;
   while (M <= B) {
      UseFFTPrime(n);
      q = FFTPrime[n];
      n++;
      mul(M, M, q);
   }

   if (n > 4) Error("zz_pInit: too many primes");

   NumPrimes = n;
   PrimeCnt = n;
   MaxRoot = CalcMaxRoot(q);

   if (maxroot < MaxRoot)
      MaxRoot = maxroot;

   negate(MinusM, M);
   MinusMModP = rem(MinusM, p);

   if (!(CoeffModP = (long *) NTL_MALLOC(n, sizeof(long), 0)))
      Error("out of space");

   if (!(x = (double *) NTL_MALLOC(n, sizeof(double), 0)))
      Error("out of space");

   if (!(u = (long *) NTL_MALLOC(n, sizeof(long), 0)))
      Error("out of space");

   for (i = 0; i < n; i++) {
      q = FFTPrime[i];

      div(M1, M, q);
      t = rem(M1, q);
      t = InvMod(t, q);
      if (NTL_zz_p_QUICK_CRT) mul(M1, M1, t);
      CoeffModP[i] = rem(M1, p);
      x[i] = ((double) t)/((double) q);
      u[i] = t;
   }
}

zz_pInfoT::zz_pInfoT(INIT_FFT_TYPE, FFTPrimeInfo *info)
{
   ref_count = 1;

   p = info->q;
   pinv = info->qinv;

   p_info = info;
   p_own = 0;

   NumPrimes = 1;
   PrimeCnt = 0;

   MaxRoot = CalcMaxRoot(p);
}

zz_pInfoT::zz_pInfoT(INIT_USER_FFT_TYPE, long q)
{
   ref_count = 1;


   long w;
   if (!IsFFTPrime(q, w)) Error("invalid user supplied prime");

   p = q;
   pinv = 1/((double) q);

   p_info = NTL_NEW_OP FFTPrimeInfo();
   if (!p_info) Error("out of memory");

   long bigtab = 0;
#ifdef NTL_FFT_BIGTAB
   bigtab = 1;
#endif
   InitFFTPrimeInfo(*p_info, q, w, bigtab); 

   p_own = 1;

   NumPrimes = 1;
   PrimeCnt = 0;

   MaxRoot = CalcMaxRoot(p);
}



zz_pInfoT::~zz_pInfoT()
{
   if (!p_info) {
      free(CoeffModP);
      free(x);
      free(u);
   }
   else {
      if (p_own) delete p_info;
   }
}


NTL_THREAD_LOCAL zz_pInfoT *zz_pInfo = 0;


typedef zz_pInfoT *zz_pInfoPtr;

static 
void CopyPointer(zz_pInfoPtr& dst, zz_pInfoPtr src)
{
   if (src == dst) return;

   if (dst) {
      dst->ref_count--;

      if (dst->ref_count < 0) 
         Error("internal error: negative zz_pContext ref_count");

      if (dst->ref_count == 0) delete dst;
   }

   if (src) {
      if (src->ref_count == NTL_MAX_LONG) 
         Error("internal error: zz_pContext ref_count overflow");

      src->ref_count++;
   }

   dst = src;
}
   

void zz_p::init(long p, long maxroot)
{
   zz_pContext c(p, maxroot);
   c.restore();

}

void zz_p::FFTInit(long index)
{
   zz_pContext c(INIT_FFT, index);
   c.restore();
}

void zz_p::UserFFTInit(long q)
{
   zz_pContext c(INIT_USER_FFT, q);
   c.restore();
}

zz_pContext::zz_pContext(long p, long maxroot)
{
   ptr = NTL_NEW_OP zz_pInfoT(p, maxroot);
}

// FIXME: maybe store FFT contexts in a global table,
// so we don't go through the trouble of creating and destroying
// them so much

// FIXME: in a thread-safe impl: with my idea for having
// thread local copies of certain FFT tables (and NumFFTPrimes)
// I have to be careful when installing a "foreign" zz_pContext
// -- and for that matter, a "foreign" ZZ_pContext -- from
// another thread. The issue is that the foreign thread may
// have seen more FFTPrimes. So I'll need to perform
// a special check, and if necessary, refresh the local
// view of the FFT tables.

zz_pContext::zz_pContext(INIT_FFT_TYPE, long index)
{
   if (index < 0)
      Error("bad FFT prime index");

   // allows non-consecutive indices...I'm not sure why
   while (NumFFTPrimes < index)
      UseFFTPrime(NumFFTPrimes);

   UseFFTPrime(index);

   ptr = 0;
   CopyPointer(ptr, FFTTables[index]->zz_p_context);
}

zz_pContext::zz_pContext(INIT_USER_FFT_TYPE, long q)
{
   ptr = NTL_NEW_OP zz_pInfoT(INIT_USER_FFT, q);
}

zz_pContext::zz_pContext(const zz_pContext& a)
{
   ptr = 0;
   CopyPointer(ptr, a.ptr);
}


zz_pContext& zz_pContext::operator=(const zz_pContext& a)
{
   CopyPointer(ptr, a.ptr);
   return *this;
}


zz_pContext::~zz_pContext()
{
   CopyPointer(ptr, 0);
}

void zz_pContext::save()
{
   CopyPointer(ptr, zz_pInfo);
}

void zz_pContext::restore() const
{
   CopyPointer(zz_pInfo, ptr);
}



zz_pBak::~zz_pBak()
{
   if (MustRestore)
      CopyPointer(zz_pInfo, ptr);

   CopyPointer(ptr, 0);
}

void zz_pBak::save()
{
   MustRestore = 1;
   CopyPointer(ptr, zz_pInfo);
}


void zz_pBak::restore()
{
   MustRestore = 0;
   CopyPointer(zz_pInfo, ptr);
}




static inline 
long reduce(long a, long p)
{
   if (a >= 0 && a < p)
      return a;
   else {
      a = a % p;
      if (a < 0) a += p;
      return a;
   }
}


zz_p to_zz_p(long a)
{
   return zz_p(reduce(a, zz_p::modulus()), INIT_LOOP_HOLE);
}

void conv(zz_p& x, long a)
{
   x._zz_p__rep = reduce(a, zz_p::modulus());
}

zz_p to_zz_p(const ZZ& a)
{
   return zz_p(rem(a, zz_p::modulus()), INIT_LOOP_HOLE);
}

void conv(zz_p& x, const ZZ& a)
{
   x._zz_p__rep = rem(a, zz_p::modulus());
}


istream& operator>>(istream& s, zz_p& x)
{
   NTL_ZZRegister(y);
   s >> y;
   conv(x, y);

   return s;
}

ostream& operator<<(ostream& s, zz_p a)
{
   NTL_ZZRegister(y);
   y = rep(a);
   s << y;

   return s;
}

NTL_END_IMPL
