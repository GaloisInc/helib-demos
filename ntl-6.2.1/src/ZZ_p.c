

#include <NTL/ZZ_p.h>
#include <NTL/FFT.h>

#include <NTL/new.h>


NTL_START_IMPL


ZZ_pInfoT::ZZ_pInfoT(const ZZ& NewP)
{
   if (NewP <= 1) Error("ZZ_pContext: p must be > 1");

   ref_count = 1;
   p = NewP;
   size = p.size();

   ExtendedModulusSize = 2*size + 
                 (NTL_BITS_PER_LONG + NTL_ZZ_NBITS - 1)/NTL_ZZ_NBITS;

   initialized = 0;
   x = 0;
   u = 0;
}



void ZZ_pInfoT::init()
{
   ZZ B, M, M1, M2, M3;
   long n, i;
   long q, t;

   initialized = 1;

   sqr(B, p);

   LeftShift(B, B, NTL_FFTMaxRoot+NTL_FFTFudge);

   // FIXME: the following is quadratic time...would
   // be nice to get a faster solution...
   // One could estimate the # of primes by summing logs,
   // then multiply using a tree-based multiply, then 
   // adjust up or down...

   // Assuming IEEE floating point, the worst case estimate
   // for error guarantees a correct answer +/- 1 for
   // numprimes up to 2^25...for sure we won't be
   // using that many primes...we can certainly put in 
   // a sanity check, though. 

   // If I want a more accuaruate summation (with using Kahan,
   // which has some portability issues), I could represent 
   // numbers as x = a + f, where a is integer and f is the fractional
   // part.  Summing in this representation introduces an *absolute*
   // error of 2 epsilon n, which is just as good as Kahan 
   // for this application.

   // same strategy could also be used in the ZZX HomMul routine,
   // if we ever want to make that subquadratic

   set(M);
   n = 0;
   while (M <= B) {
      UseFFTPrime(n);
      q = FFTPrime[n];
      n++;
      mul(M, M, q);
   }

   NumPrimes = n;
   MaxRoot = CalcMaxRoot(q);


   double fn = double(n);

   if (8.0*fn*(fn+32) > NTL_FDOUBLE_PRECISION)
      Error("modulus too big");


   if (8.0*fn*(fn+32) > NTL_FDOUBLE_PRECISION/double(NTL_SP_BOUND))
      QuickCRT = 0;
   else
      QuickCRT = 1;


   if (!(x = (double *) NTL_MALLOC(n, sizeof(double), 0)))
      Error("out of space");

   if (!(u = (long *) NTL_MALLOC(n,  sizeof(long), 0)))
      Error("out of space");

   ZZ_p_rem_struct_init(&rem_struct, n, p, FFTPrime);

   ZZ_p_crt_struct_init(&crt_struct, n, p, FFTPrime);

   if (ZZ_p_crt_struct_special(crt_struct)) return;

   // FIXME: thread-safe impl: there is a problem here.
   // in addition to read-only tables, which can be shared across threads,
   // the rem and crt data structures hold scartch space which cannot.
   // So a viable solution is to allocate thread-local scratch space when
   // installing a modulus (assuming the tables are already initialized,
   // otherwise delay allocation until initialization), and free
   // it when uninstalling.  This should be efficient enough...I just
   // want to avoid allocation and deallocation on *every* rem/crt

   ZZ qq, rr;

   DivRem(qq, rr, M, p);

   NegateMod(MinusMModP, rr, p);

   for (i = 0; i < n; i++) {
      q = FFTPrime[i];

      long tt = rem(qq, q);

      mul(M2, p, tt);
      add(M2, M2, rr); 
      div(M2, M2, q);  // = (M/q) rem p
      

      div(M1, M, q);
      t = rem(M1, q);
      t = InvMod(t, q);

      mul(M3, M2, t);
      rem(M3, M3, p);

      ZZ_p_crt_struct_insert(crt_struct, i, M3);


      x[i] = ((double) t)/((double) q);
      u[i] = t;
   }
}



ZZ_pInfoT::~ZZ_pInfoT()
{
   if (initialized) {
      ZZ_p_rem_struct_free(rem_struct);
      ZZ_p_crt_struct_free(crt_struct);

      free(x);
      free(u);
   }
}


NTL_THREAD_LOCAL ZZ_pInfoT *ZZ_pInfo = 0; 

typedef ZZ_pInfoT *ZZ_pInfoPtr;


static 
void CopyPointer(ZZ_pInfoPtr& dst, ZZ_pInfoPtr src)
{
   if (src == dst) return;

   if (dst) {
      dst->ref_count--;

      if (dst->ref_count < 0) 
         Error("internal error: negative ZZ_pContext ref_count");

      if (dst->ref_count == 0) delete dst;
   }

   if (src) {
      if (src->ref_count == NTL_MAX_LONG)
         Error("internal error: ZZ_pContext ref_count overflow");

      src->ref_count++;
   }

   dst = src;
}
   

// FIXME: thread-safe impl: see FIXME in lzz_p.c regarding
// foreign contexts

void ZZ_p::init(const ZZ& p)
{
   ZZ_pContext c(p);
   c.restore();
}


ZZ_pContext::ZZ_pContext(const ZZ& p)
{
   ptr = NTL_NEW_OP ZZ_pInfoT(p);
}

ZZ_pContext::ZZ_pContext(const ZZ_pContext& a)
{
   ptr = 0;
   CopyPointer(ptr, a.ptr);
}

ZZ_pContext& ZZ_pContext::operator=(const ZZ_pContext& a)
{
   CopyPointer(ptr, a.ptr);
   return *this;
}


ZZ_pContext::~ZZ_pContext()
{
   CopyPointer(ptr, 0);
}

void ZZ_pContext::save()
{
   CopyPointer(ptr, ZZ_pInfo);
}

void ZZ_pContext::restore() const
{
   CopyPointer(ZZ_pInfo, ptr);
}



ZZ_pBak::~ZZ_pBak()
{
   if (MustRestore)
      CopyPointer(ZZ_pInfo, ptr);

   CopyPointer(ptr, 0);
}

void ZZ_pBak::save()
{
   MustRestore = 1;
   CopyPointer(ptr, ZZ_pInfo);
}


void ZZ_pBak::restore()
{
   MustRestore = 0;
   CopyPointer(ZZ_pInfo, ptr);
}


const ZZ_p& ZZ_p::zero()
{
   NTL_THREAD_LOCAL static ZZ_p z(INIT_NO_ALLOC);
   return z;
}

ZZ_p::DivHandlerPtr ZZ_p::DivHandler = 0;

   

ZZ_p::ZZ_p(INIT_VAL_TYPE, const ZZ& a)  // NO_ALLOC
{
   conv(*this, a);
} 

ZZ_p::ZZ_p(INIT_VAL_TYPE, long a) // NO_ALLOC
{
   conv(*this, a);
}


void conv(ZZ_p& x, long a)
{
   if (a == 0)
      clear(x);
   else if (a == 1)
      set(x);
   else {
      NTL_ZZRegister(y);

      conv(y, a);
      conv(x, y);
   }
}

istream& operator>>(istream& s, ZZ_p& x)
{
   NTL_ZZRegister(y);

   s >> y;
   conv(x, y);

   return s;
}

void div(ZZ_p& x, const ZZ_p& a, const ZZ_p& b)
{
   NTL_ZZ_pRegister(T);

   inv(T, b);
   mul(x, a, T);
}

void inv(ZZ_p& x, const ZZ_p& a)
{
   if (InvModStatus(x._ZZ_p__rep, a._ZZ_p__rep, ZZ_p::modulus())) {
      if (IsZero(a._ZZ_p__rep))
         Error("ZZ_p: division by zero");
      else if (ZZ_p::DivHandler)
         (*ZZ_p::DivHandler)(a);
      else
         Error("ZZ_p: division by non-invertible element");
   }
}

long operator==(const ZZ_p& a, long b)
{
   if (b == 0)
      return IsZero(a);

   if (b == 1)
      return IsOne(a);

   NTL_ZZ_pRegister(T);
   conv(T, b);
   return a == T;
}



void add(ZZ_p& x, const ZZ_p& a, long b)
{
   NTL_ZZ_pRegister(T);
   conv(T, b);
   add(x, a, T);
}

void sub(ZZ_p& x, const ZZ_p& a, long b)
{
   NTL_ZZ_pRegister(T);
   conv(T, b);
   sub(x, a, T);
}

void sub(ZZ_p& x, long a, const ZZ_p& b)
{
   NTL_ZZ_pRegister(T);
   conv(T, a);
   sub(x, T, b);
}

void mul(ZZ_p& x, const ZZ_p& a, long b)
{
   NTL_ZZ_pRegister(T);
   conv(T, b);
   mul(x, a, T);
}

void div(ZZ_p& x, const ZZ_p& a, long b)
{
   NTL_ZZ_pRegister(T);
   conv(T, b);
   div(x, a, T);
}

void div(ZZ_p& x, long a, const ZZ_p& b)
{
   if (a == 1) {
      inv(x, b);
   }
   else {
      NTL_ZZ_pRegister(T);
      conv(T, a);
      div(x, T, b);
   }
}

NTL_END_IMPL
