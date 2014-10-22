#define DEBUG_MODE 1

#ifdef STUB
#include "helib-stub.h"
#else
#include "helib-instance.h"
#endif

const int nrounds = 10;

unsigned char s_box[256] =/*{{{*/
    {
        0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
        0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
        0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
        0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
        0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
        0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
        0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
        0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
        0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
        0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
        0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
        0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
        0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
        0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
        0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
        0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
    };/*}}}*/

// a poly is a 32-bit value
typedef uint u32;
typedef unsigned char u8;

typedef vector<u8> pt_state; // [16]
typedef pt_state pt_roundkey;

void print_state(pt_state st) {
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) printf(" ");
        printf("%02x", st[i]);
    }
    printf("\n");
}  

typedef vector<Ctxt>     CtxtByte;  // [8]
typedef vector<CtxtByte> CtxtState; // [16]

EncryptedArray* global_ea;
FHESecKey* global_seckey;

// constants
CtxtByte* global_c;

u8 r_con (u32 i) {
    static u8 lookup[] = { 1, 2, 4, 8, 16, 32, 64, 128, 27,
                54, 108, 216, 171, 77, 154, 47 };
    return lookup[i];
}

void kexp_xor (u8 left[4], u8 right[4])
{
    for (int i = 0; i < 4; i++) {
        left[i] ^= right[i];
    }
}

void next_polys(u8 ps[16], u32 round) {
  for (int i = 0; i < 4; i++)
    ps[i] ^= s_box[ps[((i + 1) % 4) + 12]];
  ps[0] ^= r_con(round);

  kexp_xor(&ps[ 4], &ps[0]);
  kexp_xor(&ps[ 8], &ps[4]);
  kexp_xor(&ps[12], &ps[8]);
}

typedef u8 old_roundkey[16];

void key_expand(old_roundkey key, old_roundkey rkeys[nrounds])
{
    old_roundkey tmp_key;
    memcpy(tmp_key, key, 16);
    memcpy(rkeys[0], tmp_key, 16);
    for (int i = 0; i < nrounds-1; i++) {
        next_polys(tmp_key, i);
        memcpy(rkeys[i+1], tmp_key, 16);
    }
}

// shoehorn old expand key functions into vector representation
vector<pt_roundkey> key_expand (pt_roundkey key)
{
    old_roundkey temp;
    for (int i = 0; i < 16; i++) {
        temp[i] = key[i];
    }
    old_roundkey rkeys[nrounds];
    key_expand(temp, rkeys);
    vector<pt_roundkey> ret (nrounds);
    for (int i = 0; i < nrounds; i++) {
        pt_roundkey k (16);
        for (int j = 0; j < 16; j++) {
            k[j] = rkeys[i][j];
        }
        ret[i] = k;
    }
    return ret;
}

CtxtByte& byte_xor (CtxtByte& lhs, const CtxtByte& rhs) {
    for (int i = 0; i < 8; i++)
        lhs[i] += rhs[i];
    return lhs;
}

vector<long> pad (long firstElem, long nslots) {
    vector<long> ns;
    ns.push_back(firstElem);
    for (int i = 1; i < nslots; i++)
        ns.push_back(0);
    return ns;
}

vector<vector<long>> encode_byte (u8 inp, long nslots) {
    vector< vector<long> > new_vec;
    for (int j = 0; j < 8; j++)
        new_vec.push_back(pad((inp >> j) & 1, nslots));
    return new_vec;
}

u8 decode_byte (const vector<vector<long>>& inp) {
    u8 elem = 0;
    for (int i = 0; i < 8; i++) {
        elem |= inp[i][0] << i;
    }
    return elem;
}

vector<vector<vector<long>>> encode_state (pt_state inp, long nslots) {
    vector<vector<vector<long>>> vs;
    for (int i = 0; i < 16; i++) {
        vs.push_back(encode_byte(inp[i], nslots));
    }
    return vs;
}

void decode_state (u8 ret[16], const vector<vector<vector<long>>>& inp) {
    for (int i = 0; i < 16; i++) {
        ret[i] = decode_byte(inp[i]);
    }
}

void add_key(CtxtState key0, CtxtState& input) {
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 8; j++)
            input[i][j] += key0[i][j];
}

// C++ thinks the modulus of a negative number is negative, which is bad.
int mod (int n, int m) {
    int res = n % m;
    if (res < 0) res *= -1;
    return res;
}

void xform_byte(CtxtByte& b) {
    //static vector<NTL::ZZX> c;
    //if (c.size() == 0) {
        //// if c hasn't been filled in yet,
        //// we need to generate the constant 0x1b
        //// in a way that we can use.
        //PlaintextArray zero_p(*global_ea);
        //PlaintextArray one_p(*global_ea);
        //zero_p.replicate(0);
        //one_p.replicate(1);
        //NTL::ZZX zero, one;
        //global_ea->encode(zero, zero_p);
        //global_ea->encode(one, one_p);
        //// because 0b01100011 = 0x63
        //c.push_back(one);
        //c.push_back(one);
        //c.push_back(zero);
        //c.push_back(zero);
        //c.push_back(zero);
        //c.push_back(one);
        //c.push_back(one);
        //c.push_back(zero);
    //}
    CtxtByte bp = b;
    for (int i = 0; i < 8; i++) {
        bp[i] += b[mod(i-4,8)];
        bp[i] += b[mod(i-5,8)];
        bp[i] += b[mod(i-6,8)];
        bp[i] += b[mod(i-7,8)];
        bp[i] += (*global_c)[i];
        //bp[i].addConstant(c[i]);
        //b[i].addConstant(c[i]);
    }
    b = bp;
}

void sub_byte (CtxtByte& b) {
    // TODO need to take the inverse
    xform_byte(b);
}

void shift_rows(CtxtState& input) {/*{{{*/
    // row 2
    CtxtByte tmp = input[4];
    input[4] = input[5];
    input[5] = input[6];
    input[6] = input[7];
    input[7] = tmp;
    // row 3
    tmp = input[8];
    input[8] = input[10];
    input[10] = tmp;
    tmp = input[9];
    input[9] = input[11];
    input[11] = tmp;
    // row 4
    tmp = input[12];
    input[12] = input[15];
    input[15] = input[14];
    input[14] = input[13];
    input[13] = tmp;
}

void mix_byte_shift(CtxtByte& b) {
    static vector<NTL::ZZX> oneBee;
    if (oneBee.size() == 0) {
        // if oneBee hasn't been filled in yet,
        // we need to generate the constant 0x1b
        // in a way that we can use.
        PlaintextArray zero_p(*global_ea);
        PlaintextArray one_p(*global_ea);
        zero_p.replicate(0);
        one_p.replicate(1);
        NTL::ZZX zero, one;
        global_ea->encode(zero, zero_p);
        global_ea->encode(one, one_p);
        // because 0b00011011 == 0x1b
        oneBee.push_back(one);
        oneBee.push_back(one);
        oneBee.push_back(zero);
        oneBee.push_back(one);
        oneBee.push_back(one);
        oneBee.push_back(zero);
        oneBee.push_back(zero);
        oneBee.push_back(zero);
    }
    Ctxt tmp = b[7];
    for (int i = 6; i >= 0; i--) {
        Ctxt v = tmp;
        v.multByConstant(oneBee[i]);
        b[i+1] = b[i];
        b[i+1] += v;
    }
    b[0] = tmp;
}

void mix_columns(CtxtByte& r0,
                 CtxtByte& r1,
                 CtxtByte& r2,
                 CtxtByte& r3) {
    CtxtByte a0 = r0, a1 = r1, a2 = r2, a3 = r3;
    CtxtByte b0 = r0, b1 = r1, b2 = r2, b3 = r3;
    mix_byte_shift(b0);
    mix_byte_shift(b1);
    mix_byte_shift(b2);
    mix_byte_shift(b3);
    r0 = b0;
    r1 = b1;
    r2 = b2;
    r3 = b3;
    byte_xor(byte_xor(byte_xor(byte_xor(r0, a3), a2), b1), a1);
    byte_xor(byte_xor(byte_xor(byte_xor(r1, a0), a3), b2), a2);
    byte_xor(byte_xor(byte_xor(byte_xor(r2, a1), a0), b3), a3);
    byte_xor(byte_xor(byte_xor(byte_xor(r3, a2), a1), b0), a0);
}

void decrypt_aes_block(u8 result[16], const CtxtState& c_pt, const FHESecKey& secretKey)
{
    for (int i = 0; i < 16; i++) {
        result[i] = 0;
        for (int j = 0; j < 8; j++) {
            vector<long> v;
            global_ea->decrypt(c_pt[i][j], secretKey, v);
            result[i] |= (v[0] << j);
        }
        printf("%02x", result[i]);
    }
    printf("\n");
}

void first_round(const CtxtState& key0, CtxtState& input) {
    add_key(key0, input);
}

void middle_round(const CtxtState& key, CtxtState& input) {
    time_t old_time, new_time;
    u8 result[16];
    old_time = std::time(NULL);
    for (int i = 0; i < 16; i++)
        sub_byte(input[i]);
    printf("  sub bytes: ");
    decrypt_aes_block(result, input, *global_seckey);
    shift_rows(input);
    printf("  shift rows: ");
    decrypt_aes_block(result, input, *global_seckey);
    for (int i = 0; i < 4; i++)
        mix_columns(input[i], input[i+4], input[i+8], input[i+12]);
    printf("  mix columns: ");
    decrypt_aes_block(result, input, *global_seckey);
    add_key(key, input);
    printf("  add key: ");
    decrypt_aes_block(result, input, *global_seckey);
    new_time = std::time(NULL);
    cout << "  Round took " << (new_time - old_time) << "s" << endl;
}

void final_round(const CtxtState& keyn, CtxtState& input) {
    for (int i = 0; i < 16; i++)
        sub_byte(input[i]);
    shift_rows(input);
    add_key(keyn, input);
}

CtxtByte encrypt_byte( const EncryptedArray& ea, const FHEPubKey& pk, u8 inp )
{
    vector<vector<long>> inp_vec (encode_byte(inp, ea.size()));
    vector<Ctxt> ct_byte;
    for (int i = 0; i < 8; i++) {
        Ctxt new_ctx(pk);
        ea.encrypt(new_ctx, pk, inp_vec[i]);
        ct_byte.push_back(new_ctx);
    }
    return ct_byte;
}

u8 decrypt_byte ( const EncryptedArray& ea, const FHESecKey& sk, CtxtByte inp )
{
    vector<vector<long>> derp_vec (8);
    for (int i=0; i < 8; i++) {
        ea.decrypt( inp[i], sk, derp_vec[i] );
    }
    return decode_byte(derp_vec);
}

CtxtState encrypt_state
( 
    const EncryptedArray& ea,
    const FHEPubKey& pk,
    pt_state st
)
{
    vector<vector<vector<long>>> pt (encode_state(st,ea.size()));
    CtxtState c_st;
    for (int i = 0; i < 16; i++) {
        vector<Ctxt> vs;
        for (int j = 0; j < 8; j++) {
            Ctxt new_ctx(pk);
            ea.encrypt(new_ctx, pk, pt[i][j]);
            vs.push_back(new_ctx);
        }
        c_st.push_back(vs);
    }
    return c_st;
}

vector<CtxtState> encrypt_keys 
(
    const EncryptedArray& ea, 
    const FHEPubKey& pk, 
    const vector<pt_roundkey>& rks
) 
{
    vector<CtxtState> keys;
    for (size_t i = 0; i < rks.size(); i++) {
        CtxtState new_st (encrypt_state(ea, pk, rks[i]));
        keys.push_back(new_st);
    }
    return keys;
}

void print_result (u8 result[16]) {
    for (int i = 0; i < 16; i++) {
        printf("%02x", result[i]);
    }
}/*}}}*/


int main(int argc, char **argv) {

    long m=0;/*{{{*/
    long p=2;
    long r=1;
    long L=16;
    long c=1;
    long w=64;
    long d=0;
    long security=128;
    NTL::ZZX G;

    pt_roundkey key ({
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    });
    pt_state data ({
        0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
        0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff
    });

    cout << "Performing key expansion..." << endl;
    vector<pt_roundkey> roundkeys (key_expand(key));

    cout << "Initializing HElib values..." << endl;
    m = FindM(security,L,c,p,d,m,0);

    FHEcontext context(m, p, r);
    buildModChain(context, L, c);
    FHESecKey secretKey(context);
    const FHEPubKey& publicKey = secretKey;
    global_seckey = &secretKey;

    G = context.alMod.getFactorsOverZZ()[0];
    secretKey.GenSecKey(w);
    addSome1DMatrices(secretKey);

    EncryptedArray ea(context, G);
    global_ea = &ea;
    long nslots = ea.size();
    cout << "nslots=" << nslots << endl;
    CtxtByte const_c = encrypt_byte(ea, publicKey, 0x63);
    global_c = &const_c;
    /*}}}*/

    // test SubByte
    puts("");
    u8 inp = 0xAB;
    CtxtByte test = encrypt_byte(ea, publicKey, inp);
    sub_byte(test);
    u8 res = decrypt_byte(ea, secretKey, test);
    printf("homomorphic SubByte(0x%02x) = 0x%02x\n", inp, res);
    printf("plaintext     s_box[0x%02x] = 0x%02x\n", inp, s_box[inp]);

    return 0;

    time_t old_time, new_time;/*{{{*/
    old_time = std::time(NULL);
    cout << "Encrypting keys..." << endl;

    u8 result[16];

    vector<CtxtState> encrypted_keys (encrypt_keys(ea, publicKey, roundkeys));

    new_time = std::time(NULL);
    cout << "  " << (new_time - old_time) << "s" << endl;
    old_time = new_time;
    cout << "Encrypting cleartext..." << endl;

    CtxtState c_pt(encrypt_state(ea, publicKey, data));
    // c_pt <- data // "bit sliced"

    new_time = std::time(NULL);
    cout << "  " << (new_time - old_time) << "s" << endl;
    old_time = new_time;

    cout << "Running AES..." << endl;
    cout << "Input = " << flush;
    decrypt_aes_block(result, c_pt, secretKey);

    cout << "First round: ";
    first_round(encrypted_keys[0], c_pt);
    decrypt_aes_block(result, c_pt, secretKey);

    // checked to here

    for (int i = 1; i < nrounds-1; i++) {
        middle_round(encrypted_keys[i], c_pt);
        decrypt_aes_block(result, c_pt, secretKey);
        if (DEBUG_MODE) goto end;
    }
    final_round(encrypted_keys[nrounds-1], c_pt);

    end:


    new_time = std::time(NULL);
    cout << "  " << (new_time - old_time) << "s" << endl;
    old_time = new_time;
    cout << "Decrypting result..." << endl;

    decrypt_aes_block(result, c_pt, secretKey);

    new_time = std::time(NULL);
    cout << "  " << (new_time - old_time) << "s" << endl;
    old_time = new_time;

    cout << endl << "And that's that." << endl;

    return 0;/*}}}*/
}
