// The cipher library of the «Гермес» suite: what its .NET programs did with
// Windows CNG / DPAPI / GZipStream and with their own hand-written code, made
// here byte for byte the same. Programs see it as <hcrypt.h> (names without
// the hc_ prefix: hc_internal.h turns the one into the other).
//
//   SHA-256, HMAC, PBKDF2 (as Rfc2898DeriveBytes/SHA256), CRC-32 (gzip)
//   BLAKE2b and Argon2id ported from Hermes\Ankh\...\Core\Blake2b.cs, Argon2id.cs
//     (lanes of a slice on every core: smp_parallel)
//   SHA-256 and HMAC in pieces (streams), a constant-time compare
//   AES-256-GCM (as AesGcmCng.cs), AES-256-CBC + PKCS7 (as Хетеп\Печать.cs),
//     CBC by whole blocks for streams and random access (VaultDoc's .vdoc);
//     AES-NI + PCLMULQDQ when the processor has them, tables otherwise
//   ChaCha20, Poly1305, XChaCha20-Poly1305 ported from Ua\Core\XChaCha20Poly1305.cs
//   ECDSA P-256 + SHA-256 (as ECDsaCng in SignetRing.cs), k by RFC 6979
//   for Bluetooth LE pairing: AES-128 (one block), AES-CMAC, ECDH P-256
//   gzip: our own deflate (LZ77 + fixed/dynamic Huffman) out; inflate.c in
//   random bytes: rdseed/rdrand + clocks, mixed with SHA-256
//   local_seal: secrets sealed to this installation (instead of DPAPI)
//
// Ported from DynastyOS (src\hcrypt.c). What it took from that kernel comes
// from Windows here (hc_internal.h): page_alloc/page_free (Argon2id memory, the
// deflate state), smp_parallel (threads), timer_ticks and mouse_seq (entropy),
// fs_read/fs_write (the local_seal key in %LOCALAPPDATA%\hcrypt\hcrypt.key).
#include "hc_internal.h"
#include "hc_simd.h"

#define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define ROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

static u32 rd32le(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u32 rd32be(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3]; }
static u64 rd64le(const u8 *p) { return (u64)rd32le(p) | ((u64)rd32le(p + 4) << 32); }
static u64 rd64be(const u8 *p) { return ((u64)rd32be(p) << 32) | (u64)rd32be(p + 4); }
static void wr32le(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void wr32be(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void wr64le(u8 *p, u64 v) { wr32le(p, (u32)v); wr32le(p + 4, (u32)(v >> 32)); }
static void wr64be(u8 *p, u64 v) { wr32be(p, (u32)(v >> 32)); wr32be(p + 4, (u32)v); }

// Secrets on the stack: a store the compiler may not drop.
static void hc_wipe(void *p, u64 n)
{
    volatile u8 *q = (volatile u8 *)p;
    while (n--) *q++ = 0;
}

static int ct_equal(const u8 *a, const u8 *b, int n)
{
    u32 d = 0;
    for (int i = 0; i < n; i++) d |= (u32)(a[i] ^ b[i]);
    return d == 0;
}

// ---- what the processor can do ----

enum { F_READY = 1, F_AES = 2, F_CLMUL = 4, F_SSSE3 = 8, F_RDRAND = 16, F_RDSEED = 32 };
static volatile int g_feat;
static int g_soft;                       // the host test: table paths only

static int features(void)
{
    int f = g_feat;
    if (!f) {
        int r[4];
        __cpuid(r, 0);
        int max = r[0];
        __cpuid(r, 1);
        f = F_READY;
        if (r[2] & (1 << 25)) f |= F_AES;
        if (r[2] & (1 << 1))  f |= F_CLMUL;
        if (r[2] & (1 << 9))  f |= F_SSSE3;
        if (r[2] & (1 << 30)) f |= F_RDRAND;
        if (max >= 7) {
            __cpuidex(r, 7, 0);
            if (r[1] & (1 << 18)) f |= F_RDSEED;
        }
        g_feat = f;
    }
    return f;
}

static int use_ni(void)
{
    int need = F_AES | F_CLMUL | F_SSSE3;
    return !g_soft && (features() & need) == need;
}

#ifdef HCRYPT_TEST
void hc_test_soft(int on) { g_soft = on; }
#endif

// ======================================================================
// SHA-256, HMAC-SHA256, PBKDF2-HMAC-SHA256
// ======================================================================

typedef struct { u32 h[8]; u64 len; u8 buf[64]; u32 n; } HcSha;

static const u32 g_k256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

static void sha_block(u32 *h, const u8 *p)
{
    u32 w[64];
    for (int i = 0; i < 16; i++) w[i] = rd32be(p + 4 * i);
    for (int i = 16; i < 64; i++) {
        u32 a = w[i - 15], b = w[i - 2];
        u32 s0 = ROR32(a, 7) ^ ROR32(a, 18) ^ (a >> 3);
        u32 s1 = ROR32(b, 17) ^ ROR32(b, 19) ^ (b >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 64; i++) {
        u32 t1 = k + (ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25)) + ((e & f) ^ (~e & g)) + g_k256[i] + w[i];
        u32 t2 = (ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

static void sha_init(HcSha *c)
{
    static const u32 iv[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    for (int i = 0; i < 8; i++) c->h[i] = iv[i];
    c->len = 0;
    c->n = 0;
}

static void sha_update(HcSha *c, const void *data, u64 n)
{
    const u8 *p = data;
    c->len += n;
    if (c->n) {
        u32 take = 64 - c->n;
        if (take > n) take = (u32)n;
        memcpy(c->buf + c->n, p, take);
        c->n += take; p += take; n -= take;
        if (c->n < 64) return;
        sha_block(c->h, c->buf);
        c->n = 0;
    }
    while (n >= 64) { sha_block(c->h, p); p += 64; n -= 64; }
    if (n) { memcpy(c->buf, p, (size_t)n); c->n = (u32)n; }
}

static void sha_final(HcSha *c, u8 out[32])
{
    u64 bits = c->len * 8;
    c->buf[c->n++] = 0x80;
    if (c->n > 56) {
        memset(c->buf + c->n, 0, 64 - c->n);
        sha_block(c->h, c->buf);
        c->n = 0;
    }
    memset(c->buf + c->n, 0, 56 - c->n);
    wr64be(c->buf + 56, bits);
    sha_block(c->h, c->buf);
    for (int i = 0; i < 8; i++) wr32be(out + 4 * i, c->h[i]);
    hc_wipe(c, sizeof(*c));
}

void hc_sha256(const void *data, long long n, unsigned char out[32])
{
    HcSha c;
    sha_init(&c);
    if (data && n > 0) sha_update(&c, data, (u64)n);
    sha_final(&c, out);
}

typedef struct { HcSha i, o; } HcHmac;

static void hmac_start(HcHmac *m, const u8 *key, u64 klen)
{
    u8 k[64], pad[64];
    memset(k, 0, 64);
    if (klen > 64) { HcSha s; sha_init(&s); sha_update(&s, key, klen); sha_final(&s, k); }
    else if (klen) memcpy(k, key, (size_t)klen);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    sha_init(&m->i); sha_update(&m->i, pad, 64);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    sha_init(&m->o); sha_update(&m->o, pad, 64);
    hc_wipe(k, 64); hc_wipe(pad, 64);
}

static void hmac_finish(HcHmac *m, u8 out[32])
{
    u8 ih[32];
    sha_final(&m->i, ih);
    sha_update(&m->o, ih, 32);
    sha_final(&m->o, out);
    hc_wipe(ih, 32);
}

void hc_hmac_sha256(const void *key, int keylen, const void *data, long long n, unsigned char out[32])
{
    HcHmac m;
    hmac_start(&m, key, key && keylen > 0 ? (u64)keylen : 0);
    if (data && n > 0) sha_update(&m.i, data, (u64)n);
    hmac_finish(&m, out);
}

// The same in pieces, for data that does not fit in memory (a .vdoc of
// hundreds of megabytes): programs hold the state as sha256_ctx /
// hmac_sha256_ctx, opaque blocks of 128 / 256 bytes.
_Static_assert(sizeof(HcSha) <= 128, "sha256_ctx is 128 bytes");
_Static_assert(sizeof(HcHmac) <= 256, "hmac_sha256_ctx is 256 bytes");

void hc_sha256_begin(void *ctx) { if (ctx) sha_init((HcSha *)ctx); }
void hc_sha256_update(void *ctx, const void *data, long long n) { if (ctx && data && n > 0) sha_update((HcSha *)ctx, data, (u64)n); }
void hc_sha256_end(void *ctx, unsigned char out[32]) { if (ctx) sha_final((HcSha *)ctx, out); }

void hc_hmac_sha256_begin(void *ctx, const void *key, int keylen)
{
    if (ctx) hmac_start((HcHmac *)ctx, key, key && keylen > 0 ? (u64)keylen : 0);
}

void hc_hmac_sha256_update(void *ctx, const void *data, long long n)
{
    if (ctx && data && n > 0) sha_update(&((HcHmac *)ctx)->i, data, (u64)n);
}

void hc_hmac_sha256_end(void *ctx, unsigned char out[32])
{
    if (!ctx) return;
    hmac_finish((HcHmac *)ctx, out);
    hc_wipe(ctx, sizeof(HcHmac));
}

// Constant time: the answer does not tell where the first difference is.
int hc_secure_equal(const void *a, const void *b, int n)
{
    if (!a || !b || n < 0) return 0;
    return ct_equal(a, b, n);
}

// Each further iteration is two bare compressions on prepared blocks.
void hc_pbkdf2_sha256(const void *pw, int pwlen, const void *salt, int saltlen, int iterations, void *out, int outlen)
{
    if (!out || outlen <= 0) return;
    if (iterations < 1) iterations = 1;
    HcHmac base;
    hmac_start(&base, pw, pw && pwlen > 0 ? (u64)pwlen : 0);
    u8 *o = out;
    u8 u[32], t[32], ib[64], ob[64];
    memset(ib, 0, 64); ib[32] = 0x80; ib[62] = 0x03;      // 96 bytes = 768 bits
    memset(ob, 0, 64); ob[32] = 0x80; ob[62] = 0x03;
    for (u32 blk = 1; outlen > 0; blk++) {
        HcHmac m = base;
        u8 cnt[4];
        wr32be(cnt, blk);
        if (salt && saltlen > 0) sha_update(&m.i, salt, (u64)saltlen);
        sha_update(&m.i, cnt, 4);
        hmac_finish(&m, u);
        memcpy(t, u, 32);
        for (int it = 1; it < iterations; it++) {
            u32 hs[8];
            memcpy(ib, u, 32);
            memcpy(hs, base.i.h, 32);
            sha_block(hs, ib);
            for (int k = 0; k < 8; k++) wr32be(ob + 4 * k, hs[k]);
            memcpy(hs, base.o.h, 32);
            sha_block(hs, ob);
            for (int k = 0; k < 8; k++) wr32be(u + 4 * k, hs[k]);
            for (int k = 0; k < 32; k++) t[k] ^= u[k];
        }
        int take = outlen < 32 ? outlen : 32;
        memcpy(o, t, (size_t)take);
        o += take;
        outlen -= take;
    }
    hc_wipe(&base, sizeof(base)); hc_wipe(u, 32); hc_wipe(t, 32); hc_wipe(ib, 64); hc_wipe(ob, 64);
}

// ======================================================================
// CRC-32 (the gzip / zip polynomial)
// ======================================================================

static u32 g_crc_tab[256];
static volatile int g_crc_ready;

static void crc_init(void)
{
    if (g_crc_ready) return;
    for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crc_tab[i] = c;
    }
    _ReadWriteBarrier();
    g_crc_ready = 1;
}

static u32 crc_update(u32 crc, const u8 *p, u64 n)
{
    crc_init();
    crc = ~crc;
    while (n--) crc = g_crc_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

unsigned int hc_crc32(const void *data, long long n)
{
    return data && n > 0 ? crc_update(0, data, (u64)n) : 0;
}

// The CRC of a stream given in pieces: crc 0 before the first.
unsigned int hc_crc32_update(unsigned int crc, const void *data, long long n)
{
    return data && n > 0 ? crc_update(crc, data, (u64)n) : crc;
}

// ======================================================================
// BLAKE2b (RFC 7693) -- Blake2b.cs, plus the key of the RFC
// ======================================================================

typedef struct { u64 h[8], t0, t1; u8 buf[128]; u32 n, outlen; } HcB2;

static const u64 g_b2iv[8] = {
    0x6A09E667F3BCC908ull, 0xBB67AE8584CAA73Bull, 0x3C6EF372FE94F82Bull, 0xA54FF53A5F1D36F1ull,
    0x510E527FADE682D1ull, 0x9B05688C2B3E6C1Full, 0x1F83D9ABFB41BD6Bull, 0x5BE0CD19137E2179ull
};

static const u8 g_b2sigma[10][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
};

#define B2G(a, b, c, d, x, y) \
    v[a] = v[a] + v[b] + (x); v[d] = ROR64(v[d] ^ v[a], 32); \
    v[c] = v[c] + v[d];       v[b] = ROR64(v[b] ^ v[c], 24); \
    v[a] = v[a] + v[b] + (y); v[d] = ROR64(v[d] ^ v[a], 16); \
    v[c] = v[c] + v[d];       v[b] = ROR64(v[b] ^ v[c], 63);

static void b2_compress(HcB2 *s, const u8 *blk, int last)
{
    u64 m[16], v[16];
    for (int i = 0; i < 16; i++) m[i] = rd64le(blk + 8 * i);
    for (int i = 0; i < 8; i++) { v[i] = s->h[i]; v[i + 8] = g_b2iv[i]; }
    v[12] ^= s->t0;
    v[13] ^= s->t1;
    if (last) v[14] = ~v[14];
    for (int r = 0; r < 12; r++) {
        const u8 *g = g_b2sigma[r % 10];
        B2G(0, 4,  8, 12, m[g[0]],  m[g[1]]);
        B2G(1, 5,  9, 13, m[g[2]],  m[g[3]]);
        B2G(2, 6, 10, 14, m[g[4]],  m[g[5]]);
        B2G(3, 7, 11, 15, m[g[6]],  m[g[7]]);
        B2G(0, 5, 10, 15, m[g[8]],  m[g[9]]);
        B2G(1, 6, 11, 12, m[g[10]], m[g[11]]);
        B2G(2, 7,  8, 13, m[g[12]], m[g[13]]);
        B2G(3, 4,  9, 14, m[g[14]], m[g[15]]);
    }
    for (int i = 0; i < 8; i++) s->h[i] ^= v[i] ^ v[i + 8];
}

static void b2_init(HcB2 *s, int outlen, const void *key, int keylen)
{
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < 8; i++) s->h[i] = g_b2iv[i];
    s->h[0] ^= 0x01010000ull ^ ((u64)keylen << 8) ^ (u64)outlen;
    s->outlen = (u32)outlen;
    if (keylen > 0) { memcpy(s->buf, key, (size_t)keylen); s->n = 128; }   // the key: a block of its own
}

static void b2_count(HcB2 *s, u32 n)
{
    s->t0 += n;
    if (s->t0 < n) s->t1++;
}

// A full buffer is compressed only when more data comes (it may be the last).
static void b2_update(HcB2 *s, const void *data, u64 len)
{
    const u8 *p = data;
    while (len > 0) {
        if (s->n == 128) { b2_count(s, 128); b2_compress(s, s->buf, 0); s->n = 0; }
        if (s->n == 0)
            while (len > 128) { b2_count(s, 128); b2_compress(s, p, 0); p += 128; len -= 128; }
        u32 take = 128 - s->n;
        if (take > len) take = (u32)len;
        memcpy(s->buf + s->n, p, take);
        s->n += take; p += take; len -= take;
    }
}

static void b2_final(HcB2 *s, u8 *out)
{
    b2_count(s, s->n);
    memset(s->buf + s->n, 0, 128 - s->n);
    b2_compress(s, s->buf, 1);
    for (u32 i = 0; i < s->outlen; i++) out[i] = (u8)(s->h[i >> 3] >> (8 * (i & 7)));
    hc_wipe(s, sizeof(*s));
}

void hc_blake2b(void *out, int outlen, const void *key, int keylen, const void *data, long long n)
{
    if (!out || outlen < 1 || outlen > 64 || keylen < 0 || keylen > 64 || (keylen && !key)) return;
    HcB2 s;
    b2_init(&s, outlen, key, keylen);
    if (data && n > 0) b2_update(&s, data, (u64)n);
    b2_final(&s, out);
}

// ======================================================================
// Argon2id v1.3 -- Argon2id.cs line for line; the lanes of one slice are
// filled at the same time on every core.
// ======================================================================

#define A2_WORDS 128                    // 64-bit words in a 1 KiB block

static const u64 g_zero_block[A2_WORDS] = { 0 };

static __forceinline u64 blamka(u64 x, u64 y)
{
    return x + y + 2 * ((x & 0xFFFFFFFFull) * (y & 0xFFFFFFFFull));
}

#define GB(a, b, c, d) \
    a = blamka(a, b); d = ROR64(d ^ a, 32); \
    c = blamka(c, d); b = ROR64(b ^ c, 24); \
    a = blamka(a, b); d = ROR64(d ^ a, 16); \
    c = blamka(c, d); b = ROR64(b ^ c, 63);

#define PERM(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15) \
    GB(v0, v4, v8,  v12) GB(v1, v5, v9,  v13) GB(v2, v6, v10, v14) GB(v3, v7, v11, v15) \
    GB(v0, v5, v10, v15) GB(v1, v6, v11, v12) GB(v2, v7, v8,  v13) GB(v3, v4, v9,  v14)

// G: R = X ^ Y; P over the 8 rows, then over the 8 columns; out (^)= Z ^ R.
static void a2_fill(const u64 *x, const u64 *y, u64 *out, int with_xor)
{
    u64 r[A2_WORDS], z[A2_WORDS];
    for (int i = 0; i < A2_WORDS; i++) { r[i] = x[i] ^ y[i]; z[i] = r[i]; }
    for (int row = 0; row < 8; row++) {
        u64 *v = z + 16 * row;
        PERM(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
             v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
    }
    for (int col = 0; col < 8; col++) {
        u64 *v = z + 2 * col;
        PERM(v[0], v[1], v[16], v[17], v[32], v[33], v[48], v[49],
             v[64], v[65], v[80], v[81], v[96], v[97], v[112], v[113]);
    }
    if (with_xor) for (int i = 0; i < A2_WORDS; i++) out[i] ^= z[i] ^ r[i];
    else          for (int i = 0; i < A2_WORDS; i++) out[i] = z[i] ^ r[i];
}

// H': BLAKE2b of LE32(len) || data up to 64 bytes; longer, a chain of 64-byte
// hashes giving 32 bytes each, the last one whole -- as HPrime in Argon2id.cs.
static void a2_hprime(u8 *out, u32 outlen, const u8 *in, u32 inlen)
{
    u8 len4[4], v[64];
    HcB2 s;
    wr32le(len4, outlen);
    if (outlen <= 64) {
        b2_init(&s, (int)outlen, 0, 0); b2_update(&s, len4, 4); b2_update(&s, in, inlen); b2_final(&s, out);
        return;
    }
    b2_init(&s, 64, 0, 0); b2_update(&s, len4, 4); b2_update(&s, in, inlen); b2_final(&s, v);
    u32 written = 0;
    while (outlen - written > 64) {
        memcpy(out + written, v, 32);
        written += 32;
        b2_init(&s, 64, 0, 0); b2_update(&s, v, 64); b2_final(&s, v);
    }
    memcpy(out + written, v, outlen - written);
    hc_wipe(v, 64);
}

typedef struct {
    u64 *mem;
    u32 lanes, lane_len, seg_len, blocks, passes, pass, slice;
} A2Ctx;

static u32 a2_ref_index(u32 pass, u32 slice, u32 index, u32 pos, u64 j1, int same_lane, u32 seg_len, u32 lane_len)
{
    u64 area;
    if (pass == 0) {
        if (slice == 0)     area = (u64)index - 1;                  // only its own lane
        else if (same_lane) area = (u64)pos - 1;
        else                area = (u64)slice * seg_len - (index == 0 ? 1 : 0);
    } else {
        if (same_lane) area = (u64)lane_len - seg_len + index - 1;
        else           area = (u64)lane_len - seg_len - (index == 0 ? 1 : 0);
    }
    u64 x = (j1 * j1) >> 32;
    u64 y = (area * x) >> 32;
    u64 rel = area - 1 - y;
    u64 start = 0;
    if (pass != 0) start = slice == 3 ? 0 : (u64)(slice + 1) * seg_len;
    return (u32)((start + rel) % lane_len);
}

static void a2_segment(int lane_i, void *p)
{
    const A2Ctx *c = p;
    u32 lane = (u32)lane_i, pass = c->pass, slice = c->slice;
    u32 seg_len = c->seg_len, lane_len = c->lane_len;
    u64 *mem = c->mem;
    u64 input[A2_WORDS], addr[A2_WORDS], tmp[A2_WORDS];
    int di = pass == 0 && slice < 2;            // data-independent addressing (Argon2i part)
    if (di) {
        memset(input, 0, sizeof(input));
        input[0] = pass; input[1] = lane; input[2] = slice;
        input[3] = c->blocks; input[4] = c->passes; input[5] = 2;   // Argon2id
    }
    u32 start = (pass == 0 && slice == 0) ? 2 : 0;
    if (di && start) {
        input[6]++;
        a2_fill(g_zero_block, input, tmp, 0);
        a2_fill(g_zero_block, tmp, addr, 0);
    }
    for (u32 index = start; index < seg_len; index++) {
        u32 pos = slice * seg_len + index;
        u64 cur = (u64)lane * lane_len + pos;
        u64 prev = (u64)lane * lane_len + (pos == 0 ? lane_len - 1 : pos - 1);
        u64 pr;
        if (di) {
            if (index % A2_WORDS == 0) {
                input[6]++;
                a2_fill(g_zero_block, input, tmp, 0);
                a2_fill(g_zero_block, tmp, addr, 0);
            }
            pr = addr[index % A2_WORDS];
        } else {
            pr = mem[prev * A2_WORDS];
        }
        u64 j1 = pr & 0xFFFFFFFFull, j2 = pr >> 32;
        u32 ref_lane = (pass == 0 && slice == 0) ? lane : (u32)(j2 % c->lanes);
        u32 ref_index = a2_ref_index(pass, slice, index, pos, j1, ref_lane == lane, seg_len, lane_len);
        u64 ref = (u64)ref_lane * lane_len + ref_index;
        a2_fill(mem + prev * A2_WORDS, mem + ref * A2_WORDS, mem + cur * A2_WORDS, pass != 0);
    }
    if (di) { hc_wipe(addr, sizeof(addr)); hc_wipe(tmp, sizeof(tmp)); }
}

static void a2_le32(HcB2 *s, u32 v)
{
    u8 b[4];
    wr32le(b, v);
    b2_update(s, b, 4);
}

int hc_argon2id(void *out, int outlen, const void *pw, int pwlen, const void *salt, int saltlen,
                const void *secret, int secretlen, const void *ad, int adlen,
                int passes, int memory_kib, int lanes)
{
    if (!out || outlen < 4 || pwlen < 0 || (pwlen && !pw) || !salt || saltlen < 8 ||
        secretlen < 0 || (secretlen && !secret) || adlen < 0 || (adlen && !ad) ||
        lanes < 1 || lanes > 0xFFFFFF || passes < 1 || memory_kib < 8 * lanes)
        return -1;

    u32 blocks = 4u * (u32)lanes * ((u32)memory_kib / (4u * (u32)lanes));   // m' = 4p * floor(m / 4p)
    A2Ctx c;
    c.lanes = (u32)lanes;
    c.lane_len = blocks / (u32)lanes;
    c.seg_len = c.lane_len / 4;
    c.blocks = blocks;
    c.passes = (u32)passes;
    u64 pages = (u64)blocks / 4;                 // four 1 KiB blocks to a page
    c.mem = page_alloc(pages);
    if (!c.mem) return -1;

    u8 h0[72];
    HcB2 s;
    b2_init(&s, 64, 0, 0);
    a2_le32(&s, (u32)lanes); a2_le32(&s, (u32)outlen); a2_le32(&s, (u32)memory_kib);
    a2_le32(&s, (u32)passes); a2_le32(&s, 0x13); a2_le32(&s, 2);
    a2_le32(&s, (u32)pwlen);     b2_update(&s, pw, (u64)pwlen);
    a2_le32(&s, (u32)saltlen);   b2_update(&s, salt, (u64)saltlen);
    a2_le32(&s, (u32)secretlen); b2_update(&s, secret, (u64)secretlen);
    a2_le32(&s, (u32)adlen);     b2_update(&s, ad, (u64)adlen);
    b2_final(&s, h0);

    u8 block[1024];
    for (u32 lane = 0; lane < (u32)lanes; lane++)
        for (u32 col = 0; col < 2; col++) {
            wr32le(h0 + 64, col);
            wr32le(h0 + 68, lane);
            a2_hprime(block, 1024, h0, 72);
            u64 *dst = c.mem + ((u64)lane * c.lane_len + col) * A2_WORDS;
            for (int i = 0; i < A2_WORDS; i++) dst[i] = rd64le(block + 8 * i);
        }
    hc_wipe(h0, sizeof(h0));

    for (u32 pass = 0; pass < (u32)passes; pass++)
        for (u32 slice = 0; slice < 4; slice++) {
            c.pass = pass;
            c.slice = slice;
            smp_parallel(lanes, a2_segment, &c);
        }

    u64 acc[A2_WORDS];
    memset(acc, 0, sizeof(acc));
    for (u32 lane = 0; lane < (u32)lanes; lane++) {
        const u64 *last = c.mem + ((u64)lane * c.lane_len + c.lane_len - 1) * A2_WORDS;
        for (int i = 0; i < A2_WORDS; i++) acc[i] ^= last[i];
    }
    for (int i = 0; i < A2_WORDS; i++) wr64le(block + 8 * i, acc[i]);
    a2_hprime(out, (u32)outlen, block, 1024);

    hc_wipe(acc, sizeof(acc));
    hc_wipe(block, sizeof(block));
    memset(c.mem, 0, (size_t)(pages * PAGE_SIZE));   // the password does not stay in RAM
    page_free(c.mem, pages);
    return 0;
}

// ======================================================================
// AES-256: AES-NI, or 32-bit tables made at the first use
// ======================================================================

static const u8 g_sbox[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16 };

static u8 g_isbox[256];
static u32 g_te[256], g_td[256];        // (2s,s,s,3s) and (14i,9i,13i,11i), high byte first
static volatile int g_aes_ready;

static u8 xtime(u8 x) { return (u8)((x << 1) ^ ((x >> 7) * 0x1b)); }
static u8 gmul(u8 a, u8 b)
{
    u8 p = 0;
    while (b) { if (b & 1) p ^= a; a = xtime(a); b >>= 1; }
    return p;
}

// Idempotent: two cores making it at once write the same values.
static void aes_tables(void)
{
    if (g_aes_ready) return;
    for (int i = 0; i < 256; i++) g_isbox[g_sbox[i]] = (u8)i;
    for (int i = 0; i < 256; i++) {
        u8 s = g_sbox[i], v = g_isbox[i];
        g_te[i] = ((u32)gmul(s, 2) << 24) | ((u32)s << 16) | ((u32)s << 8) | gmul(s, 3);
        g_td[i] = ((u32)gmul(v, 14) << 24) | ((u32)gmul(v, 9) << 16) | ((u32)gmul(v, 13) << 8) | gmul(v, 11);
    }
    _ReadWriteBarrier();
    g_aes_ready = 1;
}

#define TE0(x) g_te[x]
#define TE1(x) ROR32(g_te[x], 8)
#define TE2(x) ROR32(g_te[x], 16)
#define TE3(x) ROR32(g_te[x], 24)
#define TD0(x) g_td[x]
#define TD1(x) ROR32(g_td[x], 8)
#define TD2(x) ROR32(g_td[x], 16)
#define TD3(x) ROR32(g_td[x], 24)

typedef struct {
    __m128i ke[15], kd[15];             // AES-NI round keys (encryption, decryption)
    u32 ek[60], dk[60];                 // the same as words for the tables
    int ni;
} HcAes;

static u32 sub_word(u32 w)
{
    return ((u32)g_sbox[w >> 24] << 24) | ((u32)g_sbox[(w >> 16) & 255] << 16) |
           ((u32)g_sbox[(w >> 8) & 255] << 8) | g_sbox[w & 255];
}

static u32 inv_mix_word(u32 w)
{
    return TD0(g_sbox[w >> 24]) ^ TD1(g_sbox[(w >> 16) & 255]) ^ TD2(g_sbox[(w >> 8) & 255]) ^ TD3(g_sbox[w & 255]);
}

static void aes_setup(HcAes *a, const u8 key[32], int dec)
{
    aes_tables();
    u32 *w = a->ek;
    for (int i = 0; i < 8; i++) w[i] = rd32be(key + 4 * i);
    u8 rcon = 1;
    for (int i = 8; i < 60; i++) {
        u32 t = w[i - 1];
        if (i % 8 == 0) { t = sub_word(ROL32(t, 8)) ^ ((u32)rcon << 24); rcon = xtime(rcon); }
        else if (i % 8 == 4) t = sub_word(t);
        w[i] = w[i - 8] ^ t;
    }
    a->ni = use_ni();
    if (a->ni) {
        for (int r = 0; r < 15; r++) {
            u8 b[16];
            for (int j = 0; j < 4; j++) wr32be(b + 4 * j, w[4 * r + j]);
            a->ke[r] = _mm_loadu_si128((const __m128i *)b);
        }
        if (dec) {
            a->kd[0] = a->ke[14];
            for (int r = 1; r < 14; r++) a->kd[r] = _mm_aesimc_si128(a->ke[14 - r]);
            a->kd[14] = a->ke[0];
        }
    } else if (dec) {
        for (int j = 0; j < 4; j++) { a->dk[j] = w[56 + j]; a->dk[56 + j] = w[j]; }
        for (int r = 1; r < 14; r++)
            for (int j = 0; j < 4; j++) a->dk[4 * r + j] = inv_mix_word(w[4 * (14 - r) + j]);
    }
}

// nr rounds: 14 (AES-256) or 10 (AES-128)
static void aes_enc_rounds(const u32 *rk, int nr, const u8 in[16], u8 out[16])
{
    u32 s0 = rd32be(in) ^ rk[0], s1 = rd32be(in + 4) ^ rk[1], s2 = rd32be(in + 8) ^ rk[2], s3 = rd32be(in + 12) ^ rk[3];
    for (int r = 1; r < nr; r++) {
        const u32 *k = rk + 4 * r;
        u32 t0 = TE0(s0 >> 24) ^ TE1((s1 >> 16) & 255) ^ TE2((s2 >> 8) & 255) ^ TE3(s3 & 255) ^ k[0];
        u32 t1 = TE0(s1 >> 24) ^ TE1((s2 >> 16) & 255) ^ TE2((s3 >> 8) & 255) ^ TE3(s0 & 255) ^ k[1];
        u32 t2 = TE0(s2 >> 24) ^ TE1((s3 >> 16) & 255) ^ TE2((s0 >> 8) & 255) ^ TE3(s1 & 255) ^ k[2];
        u32 t3 = TE0(s3 >> 24) ^ TE1((s0 >> 16) & 255) ^ TE2((s1 >> 8) & 255) ^ TE3(s2 & 255) ^ k[3];
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    const u32 *k = rk + 4 * nr;
    const u8 *S = g_sbox;
    wr32be(out,      ((u32)S[s0 >> 24] << 24 | (u32)S[(s1 >> 16) & 255] << 16 | (u32)S[(s2 >> 8) & 255] << 8 | S[s3 & 255]) ^ k[0]);
    wr32be(out + 4,  ((u32)S[s1 >> 24] << 24 | (u32)S[(s2 >> 16) & 255] << 16 | (u32)S[(s3 >> 8) & 255] << 8 | S[s0 & 255]) ^ k[1]);
    wr32be(out + 8,  ((u32)S[s2 >> 24] << 24 | (u32)S[(s3 >> 16) & 255] << 16 | (u32)S[(s0 >> 8) & 255] << 8 | S[s1 & 255]) ^ k[2]);
    wr32be(out + 12, ((u32)S[s3 >> 24] << 24 | (u32)S[(s0 >> 16) & 255] << 16 | (u32)S[(s1 >> 8) & 255] << 8 | S[s2 & 255]) ^ k[3]);
}

static void aes_enc_sw(const u32 *rk, const u8 in[16], u8 out[16]) { aes_enc_rounds(rk, 14, in, out); }

static void aes_dec_sw(const u32 *rk, const u8 in[16], u8 out[16])
{
    u32 s0 = rd32be(in) ^ rk[0], s1 = rd32be(in + 4) ^ rk[1], s2 = rd32be(in + 8) ^ rk[2], s3 = rd32be(in + 12) ^ rk[3];
    for (int r = 1; r < 14; r++) {
        const u32 *k = rk + 4 * r;
        u32 t0 = TD0(s0 >> 24) ^ TD1((s3 >> 16) & 255) ^ TD2((s2 >> 8) & 255) ^ TD3(s1 & 255) ^ k[0];
        u32 t1 = TD0(s1 >> 24) ^ TD1((s0 >> 16) & 255) ^ TD2((s3 >> 8) & 255) ^ TD3(s2 & 255) ^ k[1];
        u32 t2 = TD0(s2 >> 24) ^ TD1((s1 >> 16) & 255) ^ TD2((s0 >> 8) & 255) ^ TD3(s3 & 255) ^ k[2];
        u32 t3 = TD0(s3 >> 24) ^ TD1((s2 >> 16) & 255) ^ TD2((s1 >> 8) & 255) ^ TD3(s0 & 255) ^ k[3];
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    const u32 *k = rk + 56;
    const u8 *S = g_isbox;
    wr32be(out,      ((u32)S[s0 >> 24] << 24 | (u32)S[(s3 >> 16) & 255] << 16 | (u32)S[(s2 >> 8) & 255] << 8 | S[s1 & 255]) ^ k[0]);
    wr32be(out + 4,  ((u32)S[s1 >> 24] << 24 | (u32)S[(s0 >> 16) & 255] << 16 | (u32)S[(s3 >> 8) & 255] << 8 | S[s2 & 255]) ^ k[1]);
    wr32be(out + 8,  ((u32)S[s2 >> 24] << 24 | (u32)S[(s1 >> 16) & 255] << 16 | (u32)S[(s0 >> 8) & 255] << 8 | S[s3 & 255]) ^ k[2]);
    wr32be(out + 12, ((u32)S[s3 >> 24] << 24 | (u32)S[(s2 >> 16) & 255] << 16 | (u32)S[(s1 >> 8) & 255] << 8 | S[s0 & 255]) ^ k[3]);
}

static __forceinline __m128i ni_enc(const __m128i *k, __m128i b)
{
    b = _mm_xor_si128(b, k[0]);
    for (int r = 1; r < 14; r++) b = _mm_aesenc_si128(b, k[r]);
    return _mm_aesenclast_si128(b, k[14]);
}

static __forceinline __m128i ni_dec(const __m128i *k, __m128i b)
{
    b = _mm_xor_si128(b, k[0]);
    for (int r = 1; r < 14; r++) b = _mm_aesdec_si128(b, k[r]);
    return _mm_aesdeclast_si128(b, k[14]);
}

static void aes_enc_block(const HcAes *a, const u8 in[16], u8 out[16])
{
    if (a->ni) _mm_storeu_si128((__m128i *)out, ni_enc(a->ke, _mm_loadu_si128((const __m128i *)in)));
    else aes_enc_sw(a->ek, in, out);
}

static void aes_dec_block(const HcAes *a, const u8 in[16], u8 out[16])
{
    if (a->ni) _mm_storeu_si128((__m128i *)out, ni_dec(a->kd, _mm_loadu_si128((const __m128i *)in)));
    else aes_dec_sw(a->dk, in, out);
}

// ======================================================================
// AES-128 (encryption only) and AES-CMAC (RFC 4493): Bluetooth LE pairing
// (c1/s1/e, f4/f5/f6/g2 in btsmp.c)
// ======================================================================

typedef struct {
    __m128i k[11];                      // AES-NI round keys
    u32 ek[44];                         // the same as words for the tables
    int ni;
} HcAes128;

static void aes128_setup(HcAes128 *a, const u8 key[16])
{
    aes_tables();
    u32 *w = a->ek;
    for (int i = 0; i < 4; i++) w[i] = rd32be(key + 4 * i);
    u8 rcon = 1;
    for (int i = 4; i < 44; i++) {
        u32 t = w[i - 1];
        if (i % 4 == 0) { t = sub_word(ROL32(t, 8)) ^ ((u32)rcon << 24); rcon = xtime(rcon); }
        w[i] = w[i - 4] ^ t;
    }
    a->ni = !g_soft && (features() & F_AES);
    if (a->ni) {
        for (int r = 0; r < 11; r++) {
            u8 b[16];
            for (int j = 0; j < 4; j++) wr32be(b + 4 * j, w[4 * r + j]);
            a->k[r] = _mm_loadu_si128((const __m128i *)b);
            hc_wipe(b, 16);
        }
    }
}

static void aes128_enc(const HcAes128 *a, const u8 in[16], u8 out[16])
{
    if (a->ni) {
        __m128i b = _mm_xor_si128(_mm_loadu_si128((const __m128i *)in), a->k[0]);
        for (int r = 1; r < 10; r++) b = _mm_aesenc_si128(b, a->k[r]);
        _mm_storeu_si128((__m128i *)out, _mm_aesenclast_si128(b, a->k[10]));
    } else aes_enc_rounds(a->ek, 10, in, out);
}

void hc_aes128_block(const unsigned char key[16], const unsigned char in[16], unsigned char out[16])
{
    if (!key || !in || !out) return;
    HcAes128 a;
    aes128_setup(&a, key);
    aes128_enc(&a, in, out);
    hc_wipe(&a, sizeof(a));
}

// x * 2 in GF(2^128), the CMAC subkeys
static void cmac_double(u8 o[16], const u8 i[16])
{
    u8 carry = (u8)(0 - (i[0] >> 7));
    for (int j = 0; j < 15; j++) o[j] = (u8)((i[j] << 1) | (i[j + 1] >> 7));
    o[15] = (u8)((i[15] << 1) ^ (carry & 0x87));
}

void hc_aes_cmac(const unsigned char key[16], const void *msg, long long n, unsigned char out[16])
{
    if (!key || !out) return;
    if (!msg || n < 0) n = 0;
    HcAes128 a;
    aes128_setup(&a, key);
    u8 L[16], K1[16], K2[16], x[16], last[16];
    memset(x, 0, 16);
    aes128_enc(&a, x, L);
    cmac_double(K1, L);
    cmac_double(K2, K1);
    const u8 *m = (const u8 *)msg;
    u64 len = (u64)n;
    u64 blocks = len ? (len + 15) / 16 : 1;
    for (u64 b = 0; b + 1 < blocks; b++) {      // all blocks but the last
        for (int j = 0; j < 16; j++) x[j] ^= m[16 * b + j];
        aes128_enc(&a, x, x);
    }
    u64 rest = len - 16 * (blocks - 1);         // 0..16 bytes in the last block
    if (rest == 16) {
        for (int j = 0; j < 16; j++) last[j] = m[16 * (blocks - 1) + j] ^ K1[j];
    } else {
        memset(last, 0, 16);
        if (rest) memcpy(last, m + 16 * (blocks - 1), (size_t)rest);
        last[rest] = 0x80;
        for (int j = 0; j < 16; j++) last[j] ^= K2[j];
    }
    for (int j = 0; j < 16; j++) x[j] ^= last[j];
    aes128_enc(&a, x, out);
    hc_wipe(&a, sizeof(a)); hc_wipe(L, 16); hc_wipe(K1, 16); hc_wipe(K2, 16); hc_wipe(x, 16); hc_wipe(last, 16);
}

// ======================================================================
// AES-256-GCM, 12-byte nonce, 16-byte tag (as AesGcmCng.cs)
// ======================================================================

typedef struct {
    HcAes a;
    __m128i hr, acc;                    // PCLMULQDQ: H and the running hash, bytes reversed
    u64 hh[16], hl[16];                 // tables: 4-bit multiples of H
    u8 y[16];                           // tables: the running hash
    u8 j0[16], ej0[16];
} HcGcm;

static const u16 g_last4[16] = {
    0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0 };

static void gh_table(u64 hh[16], u64 hl[16], const u8 h[16])
{
    u64 vh = rd64be(h), vl = rd64be(h + 8);
    hl[8] = vl; hh[8] = vh;
    hh[0] = 0; hl[0] = 0;
    for (int i = 4; i > 0; i >>= 1) {
        u32 t = (u32)(vl & 1) * 0xe1000000u;
        vl = (vh << 63) | (vl >> 1);
        vh = (vh >> 1) ^ ((u64)t << 32);
        hl[i] = vl; hh[i] = vh;
    }
    for (int i = 2; i <= 8; i *= 2) {
        u64 *hil = hl + i, *hih = hh + i;
        vh = *hih; vl = *hil;
        for (int j = 1; j < i; j++) { hih[j] = vh ^ hh[j]; hil[j] = vl ^ hl[j]; }
    }
}

static void gh_mult(const u64 hh[16], const u64 hl[16], u8 x[16])
{
    int lo = x[15] & 0xf;
    u64 zh = hh[lo], zl = hl[lo];
    for (int i = 15; i >= 0; i--) {
        lo = x[i] & 0xf;
        int hi = (x[i] >> 4) & 0xf, rem;
        if (i != 15) {
            rem = (int)(zl & 0xf);
            zl = (zh << 60) | (zl >> 4);
            zh = (zh >> 4) ^ ((u64)g_last4[rem] << 48);
            zh ^= hh[lo]; zl ^= hl[lo];
        }
        rem = (int)(zl & 0xf);
        zl = (zh << 60) | (zl >> 4);
        zh = (zh >> 4) ^ ((u64)g_last4[rem] << 48);
        zh ^= hh[hi]; zl ^= hl[hi];
    }
    wr64be(x, zh);
    wr64be(x + 8, zl);
}

// GF(2^128) product of byte-reversed operands (Intel's GCM white paper).
static __m128i gf_clmul(__m128i a, __m128i b)
{
    __m128i t2, t3, t4, t5, t6, t7, t8, t9;
    t3 = _mm_clmulepi64_si128(a, b, 0x00);
    t4 = _mm_clmulepi64_si128(a, b, 0x10);
    t5 = _mm_clmulepi64_si128(a, b, 0x01);
    t6 = _mm_clmulepi64_si128(a, b, 0x11);
    t4 = _mm_xor_si128(t4, t5);
    t5 = _mm_slli_si128(t4, 8);
    t4 = _mm_srli_si128(t4, 8);
    t3 = _mm_xor_si128(t3, t5);
    t6 = _mm_xor_si128(t6, t4);
    t7 = _mm_srli_epi32(t3, 31);
    t8 = _mm_srli_epi32(t6, 31);
    t3 = _mm_slli_epi32(t3, 1);
    t6 = _mm_slli_epi32(t6, 1);
    t9 = _mm_srli_si128(t7, 12);
    t8 = _mm_slli_si128(t8, 4);
    t7 = _mm_slli_si128(t7, 4);
    t3 = _mm_or_si128(t3, t7);
    t6 = _mm_or_si128(t6, t8);
    t6 = _mm_or_si128(t6, t9);
    t7 = _mm_slli_epi32(t3, 31);
    t8 = _mm_slli_epi32(t3, 30);
    t9 = _mm_slli_epi32(t3, 25);
    t7 = _mm_xor_si128(t7, t8);
    t7 = _mm_xor_si128(t7, t9);
    t8 = _mm_srli_si128(t7, 4);
    t7 = _mm_slli_si128(t7, 12);
    t3 = _mm_xor_si128(t3, t7);
    t2 = _mm_srli_epi32(t3, 1);
    t4 = _mm_srli_epi32(t3, 2);
    t5 = _mm_srli_epi32(t3, 7);
    t2 = _mm_xor_si128(t2, t4);
    t2 = _mm_xor_si128(t2, t5);
    t2 = _mm_xor_si128(t2, t8);
    t3 = _mm_xor_si128(t3, t2);
    return _mm_xor_si128(t6, t3);
}

#define BSWAP128 _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)

static void gcm_setup(HcGcm *g, const u8 key[32], const u8 nonce[12])
{
    aes_setup(&g->a, key, 0);
    u8 h[16];
    memset(h, 0, 16);
    aes_enc_block(&g->a, h, h);
    memcpy(g->j0, nonce, 12);
    g->j0[12] = 0; g->j0[13] = 0; g->j0[14] = 0; g->j0[15] = 1;
    aes_enc_block(&g->a, g->j0, g->ej0);
    if (g->a.ni) {
        g->hr = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)h), BSWAP128);
        g->acc = _mm_setzero_si128();
    } else {
        gh_table(g->hh, g->hl, h);
        memset(g->y, 0, 16);
    }
    hc_wipe(h, 16);
}

// Absorbs one field (AAD or ciphertext), zero-padded to a whole block.
static void gcm_absorb(HcGcm *g, const u8 *p, u64 n)
{
    u8 blk[16];
    if (g->a.ni) {
        const __m128i bs = BSWAP128;
        __m128i acc = g->acc;
        while (n >= 16) {
            acc = gf_clmul(_mm_xor_si128(acc, _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)p), bs)), g->hr);
            p += 16; n -= 16;
        }
        if (n) {
            memset(blk, 0, 16);
            memcpy(blk, p, (size_t)n);
            acc = gf_clmul(_mm_xor_si128(acc, _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)blk), bs)), g->hr);
        }
        g->acc = acc;
        return;
    }
    while (n >= 16) {
        for (int i = 0; i < 16; i++) g->y[i] ^= p[i];
        gh_mult(g->hh, g->hl, g->y);
        p += 16; n -= 16;
    }
    if (n) {
        for (u32 i = 0; i < (u32)n; i++) g->y[i] ^= p[i];
        gh_mult(g->hh, g->hl, g->y);
    }
}

static void gcm_tag(HcGcm *g, u64 alen, u64 clen, u8 tag[16])
{
    u8 lb[16], s[16];
    wr64be(lb, alen * 8);
    wr64be(lb + 8, clen * 8);
    gcm_absorb(g, lb, 16);
    if (g->a.ni) _mm_storeu_si128((__m128i *)s, _mm_shuffle_epi8(g->acc, BSWAP128));
    else memcpy(s, g->y, 16);
    for (int i = 0; i < 16; i++) tag[i] = s[i] ^ g->ej0[i];
}

// CTR from J0 + 1 (the low 32 bits count, as GCM wants).
static void gcm_ctr(const HcGcm *g, const u8 *in, u8 *out, u64 n)
{
    u32 c = rd32be(g->j0 + 12);
    u8 cb[64], ks[16];
    for (int j = 0; j < 4; j++) memcpy(cb + 16 * j, g->j0, 12);
    if (g->a.ni) {
        const __m128i *k = g->a.ke;
        while (n >= 64) {
            for (int j = 0; j < 4; j++) wr32be(cb + 16 * j + 12, ++c);
            __m128i b0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)cb), k[0]);
            __m128i b1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(cb + 16)), k[0]);
            __m128i b2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(cb + 32)), k[0]);
            __m128i b3 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(cb + 48)), k[0]);
            for (int r = 1; r < 14; r++) {
                b0 = _mm_aesenc_si128(b0, k[r]); b1 = _mm_aesenc_si128(b1, k[r]);
                b2 = _mm_aesenc_si128(b2, k[r]); b3 = _mm_aesenc_si128(b3, k[r]);
            }
            b0 = _mm_aesenclast_si128(b0, k[14]); b1 = _mm_aesenclast_si128(b1, k[14]);
            b2 = _mm_aesenclast_si128(b2, k[14]); b3 = _mm_aesenclast_si128(b3, k[14]);
            _mm_storeu_si128((__m128i *)out,        _mm_xor_si128(_mm_loadu_si128((const __m128i *)in), b0));
            _mm_storeu_si128((__m128i *)(out + 16), _mm_xor_si128(_mm_loadu_si128((const __m128i *)(in + 16)), b1));
            _mm_storeu_si128((__m128i *)(out + 32), _mm_xor_si128(_mm_loadu_si128((const __m128i *)(in + 32)), b2));
            _mm_storeu_si128((__m128i *)(out + 48), _mm_xor_si128(_mm_loadu_si128((const __m128i *)(in + 48)), b3));
            in += 64; out += 64; n -= 64;
        }
    }
    while (n > 0) {
        wr32be(cb + 12, ++c);
        aes_enc_block(&g->a, cb, ks);
        u32 take = n < 16 ? (u32)n : 16;
        for (u32 i = 0; i < take; i++) out[i] = in[i] ^ ks[i];
        in += take; out += take; n -= take;
    }
    hc_wipe(ks, 16);
}

void hc_aes256gcm_encrypt(const unsigned char key[32], const unsigned char nonce[12], const void *aad, int aadlen,
                          const void *in, long long n, void *out, unsigned char tag[16])
{
    if (!key || !nonce || !tag || n < 0 || aadlen < 0 || (aadlen && !aad) || (n && (!in || !out))) return;
    HcGcm g;
    gcm_setup(&g, key, nonce);
    if (n) gcm_ctr(&g, in, out, (u64)n);
    gcm_absorb(&g, aad, (u64)aadlen);
    gcm_absorb(&g, out, (u64)n);
    gcm_tag(&g, (u64)aadlen, (u64)n, tag);
    hc_wipe(&g, sizeof(g));
}

int hc_aes256gcm_decrypt(const unsigned char key[32], const unsigned char nonce[12], const void *aad, int aadlen,
                         const void *in, long long n, const unsigned char tag[16], void *out)
{
    if (!key || !nonce || !tag || n < 0 || aadlen < 0 || (aadlen && !aad) || (n && (!in || !out))) return 0;
    HcGcm g;
    u8 want[16];
    gcm_setup(&g, key, nonce);
    gcm_absorb(&g, aad, (u64)aadlen);
    gcm_absorb(&g, in, (u64)n);
    gcm_tag(&g, (u64)aadlen, (u64)n, want);
    int ok = ct_equal(want, tag, 16);
    if (ok && n) gcm_ctr(&g, in, out, (u64)n);        // only a true tag lets anything out
    hc_wipe(&g, sizeof(g));
    return ok;
}

// ======================================================================
// AES-256-CBC with PKCS7 (as Aes.Create() in Хетеп\Печать.cs)
// ======================================================================

long long hc_aes256cbc_encrypt(const unsigned char key[32], const unsigned char iv[16], const void *in, long long n, void *out)
{
    if (!key || !iv || !out || n < 0 || (n && !in)) return -1;
    HcAes a;
    aes_setup(&a, key, 0);
    const u8 *p = in;
    u8 *o = out, x[16], prev[16];
    memcpy(prev, iv, 16);
    u64 full = (u64)n / 16, rest = (u64)n % 16;
    for (u64 b = 0; b < full; b++) {
        for (int i = 0; i < 16; i++) x[i] = p[i] ^ prev[i];
        aes_enc_block(&a, x, prev);
        memcpy(o, prev, 16);
        p += 16; o += 16;
    }
    u8 pad = (u8)(16 - rest);
    for (int i = 0; i < 16; i++) x[i] = (u8)(((u64)i < rest ? p[i] : pad) ^ prev[i]);
    aes_enc_block(&a, x, o);
    hc_wipe(&a, sizeof(a)); hc_wipe(x, 16);
    return (long long)((full + 1) * 16);
}

long long hc_aes256cbc_decrypt(const unsigned char key[32], const unsigned char iv[16], const void *in, long long n, void *out)
{
    if (!key || !iv || !in || !out || n <= 0 || n % 16) return -1;
    HcAes a;
    aes_setup(&a, key, 1);
    const u8 *p = in;
    u8 *o = out, c[16], x[16], prev[16];
    memcpy(prev, iv, 16);
    for (u64 b = 0; b < (u64)n / 16; b++) {
        memcpy(c, p, 16);                       // in place: read before writing
        aes_dec_block(&a, c, x);
        for (int i = 0; i < 16; i++) o[i] = x[i] ^ prev[i];
        memcpy(prev, c, 16);
        p += 16; o += 16;
    }
    hc_wipe(&a, sizeof(a)); hc_wipe(x, 16);
    u8 *end = (u8 *)out + n;
    u32 pad = end[-1], bad = (u32)(pad == 0) | (u32)(pad > 16);
    if (!bad)
        for (u32 i = 1; i <= pad; i++) bad |= (u32)(end[-(int)i] != pad);
    if (bad) { hc_wipe(out, (u64)n); return -1; }
    return n - (long long)pad;
}

// CBC over whole blocks without padding, chained through iv: iv comes in as
// the block before `in` (the IV, or the cipher block before a piece) and goes
// out as the last cipher block -- the next piece goes on from it. A long
// stream is encrypted piece by piece and its tail by hc_aes256cbc_encrypt
// (with the iv left here); any 16-byte-aligned part of a ciphertext is
// decrypted on its own given the block before it (random access).
long long hc_aes256cbc_encrypt_blocks(const unsigned char key[32], unsigned char iv[16], const void *in, long long n, void *out)
{
    if (!key || !iv || n < 0 || n % 16 || (n && (!in || !out))) return -1;
    HcAes a;
    aes_setup(&a, key, 0);
    const u8 *p = in;
    u8 *o = out, x[16];
    for (long long b = 0; b < n; b += 16) {
        for (int i = 0; i < 16; i++) x[i] = p[b + i] ^ iv[i];
        aes_enc_block(&a, x, iv);
        memcpy(o + b, iv, 16);
    }
    hc_wipe(&a, sizeof(a)); hc_wipe(x, 16);
    return n;
}

// AES-256 in counter mode as WinZip AES has it (ZIP, zipcrypt.c): the counter
// a 16-byte little-endian number, the first block's 1; ctr comes in as the
// next block's counter and goes out so -- a long entry goes piece by piece
// (pieces a multiple of 16 bytes, but the last). Encrypts and decrypts alike.
long long hc_aes256ctr_le(const unsigned char key[32], unsigned char ctr[16], const void *in, long long n, void *out)
{
    if (!key || !ctr || n < 0 || (n && (!in || !out))) return -1;
    HcAes a;
    aes_setup(&a, key, 0);
    const u8 *p = in;
    u8 *o = out, ks[16];
    for (long long b = 0; b < n; b += 16) {
        aes_enc_block(&a, ctr, ks);
        long long m = n - b < 16 ? n - b : 16;
        for (long long i = 0; i < m; i++) o[b + i] = p[b + i] ^ ks[i];
        for (int i = 0; i < 16 && ++ctr[i] == 0; i++) { }
    }
    hc_wipe(&a, sizeof(a)); hc_wipe(ks, 16);
    return n;
}

long long hc_aes256cbc_decrypt_blocks(const unsigned char key[32], unsigned char iv[16], const void *in, long long n, void *out)
{
    if (!key || !iv || n < 0 || n % 16 || (n && (!in || !out))) return -1;
    HcAes a;
    aes_setup(&a, key, 1);
    const u8 *p = in;
    u8 *o = out;
    long long b = 0;
    if (a.ni) {                                         // four blocks at a time: AESDEC pipelines
        __m128i prev = _mm_loadu_si128((const __m128i *)iv);
        for (; b + 64 <= n; b += 64) {
            __m128i c0 = _mm_loadu_si128((const __m128i *)(p + b)), c1 = _mm_loadu_si128((const __m128i *)(p + b + 16));
            __m128i c2 = _mm_loadu_si128((const __m128i *)(p + b + 32)), c3 = _mm_loadu_si128((const __m128i *)(p + b + 48));
            __m128i x0 = _mm_xor_si128(c0, a.kd[0]), x1 = _mm_xor_si128(c1, a.kd[0]);
            __m128i x2 = _mm_xor_si128(c2, a.kd[0]), x3 = _mm_xor_si128(c3, a.kd[0]);
            for (int r = 1; r < 14; r++) {
                x0 = _mm_aesdec_si128(x0, a.kd[r]); x1 = _mm_aesdec_si128(x1, a.kd[r]);
                x2 = _mm_aesdec_si128(x2, a.kd[r]); x3 = _mm_aesdec_si128(x3, a.kd[r]);
            }
            x0 = _mm_aesdeclast_si128(x0, a.kd[14]); x1 = _mm_aesdeclast_si128(x1, a.kd[14]);
            x2 = _mm_aesdeclast_si128(x2, a.kd[14]); x3 = _mm_aesdeclast_si128(x3, a.kd[14]);
            _mm_storeu_si128((__m128i *)(o + b), _mm_xor_si128(x0, prev));
            _mm_storeu_si128((__m128i *)(o + b + 16), _mm_xor_si128(x1, c0));
            _mm_storeu_si128((__m128i *)(o + b + 32), _mm_xor_si128(x2, c1));
            _mm_storeu_si128((__m128i *)(o + b + 48), _mm_xor_si128(x3, c2));
            prev = c3;
        }
        _mm_storeu_si128((__m128i *)iv, prev);
    }
    u8 c[16], x[16];
    for (; b < n; b += 16) {
        memcpy(c, p + b, 16);                           // in place: read before writing
        aes_dec_block(&a, c, x);
        for (int i = 0; i < 16; i++) o[b + i] = x[i] ^ iv[i];
        memcpy(iv, c, 16);
    }
    hc_wipe(&a, sizeof(a)); hc_wipe(x, 16);
    return n;
}

// ======================================================================
// ChaCha20, Poly1305, XChaCha20-Poly1305 -- XChaCha20Poly1305.cs (Ua)
// ======================================================================

#define QR(a, b, c, d) \
    a += b; d = ROL32(d ^ a, 16); c += d; b = ROL32(b ^ c, 12); \
    a += b; d = ROL32(d ^ a, 8);  c += d; b = ROL32(b ^ c, 7);

static void chacha_rounds(u32 x[16])
{
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8],  x[12]) QR(x[1], x[5], x[9],  x[13])
        QR(x[2], x[6], x[10], x[14]) QR(x[3], x[7], x[11], x[15])
        QR(x[0], x[5], x[10], x[15]) QR(x[1], x[6], x[11], x[12])
        QR(x[2], x[7], x[8],  x[13]) QR(x[3], x[4], x[9],  x[14])
    }
}

static void chacha_state(u32 st[16], const u8 key[32], u32 counter, const u8 nonce12[12])
{
    st[0] = 0x61707865; st[1] = 0x3320646e; st[2] = 0x79622d32; st[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) st[4 + i] = rd32le(key + 4 * i);
    st[12] = counter;
    st[13] = rd32le(nonce12); st[14] = rd32le(nonce12 + 4); st[15] = rd32le(nonce12 + 8);
}

static void chacha_xor_state(u32 st[16], const u8 *in, u8 *out, u64 n)
{
    u32 x[16];
    u8 ks[64];
    while (n > 0) {
        for (int i = 0; i < 16; i++) x[i] = st[i];
        chacha_rounds(x);
        for (int i = 0; i < 16; i++) wr32le(ks + 4 * i, x[i] + st[i]);
        u32 take = n < 64 ? (u32)n : 64;
        for (u32 i = 0; i < take; i++) out[i] = in[i] ^ ks[i];
        in += take; out += take; n -= take;
        st[12]++;
    }
    hc_wipe(x, sizeof(x)); hc_wipe(ks, sizeof(ks));
}

void hc_chacha20_xor(const unsigned char key[32], const unsigned char nonce[12], unsigned int counter,
                     const void *in, long long n, void *out)
{
    if (!key || !nonce || n <= 0 || !in || !out) return;
    u32 st[16];
    chacha_state(st, key, counter, nonce);
    chacha_xor_state(st, in, out, (u64)n);
    hc_wipe(st, sizeof(st));
}

// HChaCha20: twenty rounds, no addition; words 0..3 and 12..15 are the subkey.
static void hchacha20(const u8 key[32], const u8 nonce16[16], u8 sub[32])
{
    u32 x[16];
    x[0] = 0x61707865; x[1] = 0x3320646e; x[2] = 0x79622d32; x[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) x[4 + i] = rd32le(key + 4 * i);
    for (int i = 0; i < 4; i++) x[12 + i] = rd32le(nonce16 + 4 * i);
    chacha_rounds(x);
    for (int i = 0; i < 4; i++) { wr32le(sub + 4 * i, x[i]); wr32le(sub + 16 + 4 * i, x[12 + i]); }
    hc_wipe(x, sizeof(x));
}

// Poly1305 on five 26-bit limbs, as Poly1305State.
typedef struct { u32 r[5], h[5], pad[4]; u8 tail[16]; u32 tn; } HcPoly;

static void poly_init(HcPoly *m, const u8 key[32])
{
    memset(m, 0, sizeof(*m));
    m->r[0] = rd32le(key) & 0x3ffffff;
    m->r[1] = (rd32le(key + 3) >> 2) & 0x3ffff03;
    m->r[2] = (rd32le(key + 6) >> 4) & 0x3ffc0ff;
    m->r[3] = (rd32le(key + 9) >> 6) & 0x3f03fff;
    m->r[4] = (rd32le(key + 12) >> 8) & 0x00fffff;
    for (int i = 0; i < 4; i++) m->pad[i] = rd32le(key + 16 + 4 * i);
}

static void poly_block(HcPoly *m, const u8 *p, int full)
{
    u32 hibit = full ? (1u << 24) : 0;
    u32 *h = m->h, *r = m->r;
    h[0] += rd32le(p) & 0x3ffffff;
    h[1] += (rd32le(p + 3) >> 2) & 0x3ffffff;
    h[2] += (rd32le(p + 6) >> 4) & 0x3ffffff;
    h[3] += (rd32le(p + 9) >> 6) & 0x3ffffff;
    h[4] += (rd32le(p + 12) >> 8) | hibit;
    u64 s1 = r[1] * 5ull, s2 = r[2] * 5ull, s3 = r[3] * 5ull, s4 = r[4] * 5ull;
    u64 d0 = (u64)h[0] * r[0] + h[1] * s4 + h[2] * s3 + h[3] * s2 + h[4] * s1;
    u64 d1 = (u64)h[0] * r[1] + (u64)h[1] * r[0] + h[2] * s4 + h[3] * s3 + h[4] * s2;
    u64 d2 = (u64)h[0] * r[2] + (u64)h[1] * r[1] + (u64)h[2] * r[0] + h[3] * s4 + h[4] * s3;
    u64 d3 = (u64)h[0] * r[3] + (u64)h[1] * r[2] + (u64)h[2] * r[1] + (u64)h[3] * r[0] + h[4] * s4;
    u64 d4 = (u64)h[0] * r[4] + (u64)h[1] * r[3] + (u64)h[2] * r[2] + (u64)h[3] * r[1] + (u64)h[4] * r[0];
    u64 c = d0 >> 26; h[0] = (u32)d0 & 0x3ffffff;
    d1 += c; c = d1 >> 26; h[1] = (u32)d1 & 0x3ffffff;
    d2 += c; c = d2 >> 26; h[2] = (u32)d2 & 0x3ffffff;
    d3 += c; c = d3 >> 26; h[3] = (u32)d3 & 0x3ffffff;
    d4 += c; c = d4 >> 26; h[4] = (u32)d4 & 0x3ffffff;
    h[0] += (u32)(c * 5); c = h[0] >> 26; h[0] &= 0x3ffffff;
    h[1] += (u32)c;
}

static void poly_update(HcPoly *m, const u8 *p, u64 n)
{
    if (m->tn) {
        u32 take = 16 - m->tn;
        if (take > n) take = (u32)n;
        memcpy(m->tail + m->tn, p, take);
        m->tn += take; p += take; n -= take;
        if (m->tn < 16) return;
        poly_block(m, m->tail, 1);
        m->tn = 0;
    }
    while (n >= 16) { poly_block(m, p, 1); p += 16; n -= 16; }
    if (n) { memcpy(m->tail, p, (size_t)n); m->tn = (u32)n; }
}

static void poly_pad16(HcPoly *m, u64 written)
{
    static const u8 zeros[16] = { 0 };
    u32 rest = (u32)(written % 16);
    if (rest) poly_update(m, zeros, 16 - rest);
}

static void poly_finish(HcPoly *m, u8 tag[16])
{
    if (m->tn) {
        m->tail[m->tn] = 1;
        for (u32 i = m->tn + 1; i < 16; i++) m->tail[i] = 0;
        poly_block(m, m->tail, 0);
    }
    u32 h0 = m->h[0], h1 = m->h[1], h2 = m->h[2], h3 = m->h[3], h4 = m->h[4], c;
    c = h1 >> 26; h1 &= 0x3ffffff; h2 += c;
    c = h2 >> 26; h2 &= 0x3ffffff; h3 += c;
    c = h3 >> 26; h3 &= 0x3ffffff; h4 += c;
    c = h4 >> 26; h4 &= 0x3ffffff; h0 += c * 5;
    c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;
    u32 g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    u32 g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    u32 g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    u32 g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    u32 g4 = h4 + c - (1u << 26);
    u32 mask = (g4 >> 31) - 1;                   // all ones if h >= p
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1; h2 = (h2 & mask) | g2; h3 = (h3 & mask) | g3; h4 = (h4 & mask) | g4;
    u32 f0 = h0 | (h1 << 26), f1 = (h1 >> 6) | (h2 << 20), f2 = (h2 >> 12) | (h3 << 14), f3 = (h3 >> 18) | (h4 << 8);
    u64 acc = (u64)f0 + m->pad[0]; wr32le(tag, (u32)acc);
    acc = (acc >> 32) + f1 + m->pad[1]; wr32le(tag + 4, (u32)acc);
    acc = (acc >> 32) + f2 + m->pad[2]; wr32le(tag + 8, (u32)acc);
    acc = (acc >> 32) + f3 + m->pad[3]; wr32le(tag + 12, (u32)acc);
    hc_wipe(m, sizeof(*m));
}

void hc_poly1305(const unsigned char key[32], const void *data, long long n, unsigned char tag[16])
{
    if (!key || !tag) return;
    HcPoly m;
    poly_init(&m, key);
    if (data && n > 0) poly_update(&m, data, (u64)n);
    poly_finish(&m, tag);
}

// The subkey, the inner nonce (4 zeros + the last 8 bytes) and the Poly1305 key.
static void xc_setup(const u8 key[32], const u8 nonce[24], u32 st[16], u8 otk[32])
{
    u8 sub[32], inner[12];
    hchacha20(key, nonce, sub);
    memset(inner, 0, 4);
    memcpy(inner + 4, nonce + 16, 8);
    chacha_state(st, sub, 0, inner);
    memset(otk, 0, 32);
    chacha_xor_state(st, otk, otk, 32);          // block 0; st[12] is now 1 for the data
    hc_wipe(sub, 32);
}

static void xc_tag(const u8 otk[32], const u8 *aad, u64 alen, const u8 *ct, u64 n, u8 tag[16])
{
    HcPoly m;
    u8 lens[16];
    poly_init(&m, otk);
    if (alen) { poly_update(&m, aad, alen); poly_pad16(&m, alen); }
    poly_update(&m, ct, n);
    poly_pad16(&m, n);
    wr64le(lens, alen);
    wr64le(lens + 8, n);
    poly_update(&m, lens, 16);
    poly_finish(&m, tag);
}

void hc_xchacha20poly1305_encrypt(const unsigned char key[32], const unsigned char nonce[24], const void *aad, int aadlen,
                                  const void *in, long long n, void *out, unsigned char tag[16])
{
    if (!key || !nonce || !tag || n < 0 || aadlen < 0 || (aadlen && !aad) || (n && (!in || !out))) return;
    u32 st[16];
    u8 otk[32];
    xc_setup(key, nonce, st, otk);
    if (n) chacha_xor_state(st, in, out, (u64)n);
    xc_tag(otk, aad, (u64)aadlen, out, (u64)n, tag);
    hc_wipe(st, sizeof(st)); hc_wipe(otk, 32);
}

int hc_xchacha20poly1305_decrypt(const unsigned char key[32], const unsigned char nonce[24], const void *aad, int aadlen,
                                 const void *in, long long n, const unsigned char tag[16], void *out)
{
    if (!key || !nonce || !tag || n < 0 || aadlen < 0 || (aadlen && !aad) || (n && (!in || !out))) return 0;
    u32 st[16];
    u8 otk[32], want[16];
    xc_setup(key, nonce, st, otk);
    xc_tag(otk, aad, (u64)aadlen, in, (u64)n, want);
    int ok = ct_equal(want, tag, 16);
    if (ok && n) chacha_xor_state(st, in, out, (u64)n);
    hc_wipe(st, sizeof(st)); hc_wipe(otk, 32);
    return ok;
}

// ======================================================================
// Random bytes: rdseed/rdrand and the clocks, through a SHA-256 pool
// ======================================================================

static volatile long g_rnd_lock;
static u8 g_rnd_pool[32];
static u64 g_rnd_count;

static u64 hw_random(void)
{
    int f = features();
    unsigned __int64 x;
    if (f & F_RDSEED)
        for (int i = 0; i < 32; i++) if (_rdseed64_step(&x)) return x;
    if (f & F_RDRAND)
        for (int i = 0; i < 16; i++) if (_rdrand64_step(&x)) return x;
    return __rdtsc();
}

// Safe on any core: the pool is locked, no device is touched.
void hc_random_bytes(void *buf, int n)
{
    if (!buf || n <= 0) return;
    while (_InterlockedCompareExchange(&g_rnd_lock, 1, 0)) _mm_pause();
    u64 e[12];
    e[0] = __rdtsc();
    e[1] = timer_ticks();
    e[2] = mouse_seq();
    e[3] = ++g_rnd_count;
    for (int i = 0; i < 6; i++) e[4 + i] = hw_random();
    e[10] = (u64)buf ^ ((u64)&e << 16);
    e[11] = __rdtsc();
    HcSha s;
    sha_init(&s); sha_update(&s, g_rnd_pool, 32); sha_update(&s, e, sizeof(e)); sha_final(&s, g_rnd_pool);
    u8 *o = buf, blk[32], tagc[9];
    for (u64 ctr = 0; n > 0; ctr++) {
        tagc[0] = 'o';
        wr64le(tagc + 1, ctr);
        sha_init(&s); sha_update(&s, g_rnd_pool, 32); sha_update(&s, tagc, 9); sha_final(&s, blk);
        int take = n < 32 ? n : 32;
        memcpy(o, blk, (size_t)take);
        o += take; n -= take;
    }
    tagc[0] = 'n';                               // the pool moves on: what was given cannot be recomputed
    sha_init(&s); sha_update(&s, g_rnd_pool, 32); sha_update(&s, tagc, 1); sha_final(&s, g_rnd_pool);
    hc_wipe(blk, 32); hc_wipe(e, sizeof(e));
    _InterlockedExchange(&g_rnd_lock, 0);
}

// ======================================================================
// ECDSA P-256 with SHA-256 (as ECDsaCng): Montgomery arithmetic on four
// 64-bit limbs, complete projective formulas (Renes-Costello-Batina, a = -3),
// a fixed 4-bit window with constant-time table reads; k by RFC 6979.
// ======================================================================

typedef struct { u64 m[4], rr[4], one[4], inv; } HcMod;
typedef struct { u64 x[4], y[4], z[4]; } HcPt;

static const u64 P256_P[4]  = { 0xFFFFFFFFFFFFFFFFull, 0x00000000FFFFFFFFull, 0x0000000000000000ull, 0xFFFFFFFF00000001ull };
static const u64 P256_N[4]  = { 0xF3B9CAC2FC632551ull, 0xBCE6FAADA7179E84ull, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF00000000ull };
static const u64 P256_B[4]  = { 0x3BCE3C3E27D2604Bull, 0x651D06B0CC53B0F6ull, 0xB3EBBD55769886BCull, 0x5AC635D8AA3A93E7ull };
static const u64 P256_GX[4] = { 0xF4A13945D898C296ull, 0x77037D812DEB33A0ull, 0xF8BCE6E563A440F2ull, 0x6B17D1F2E12C4247ull };
static const u64 P256_GY[4] = { 0xCBB6406837BF51F5ull, 0x2BCE33576B315ECEull, 0x8EE7EB4A7C0F9E16ull, 0x4FE342E2FE1A7F9Bull };

static HcMod g_mp, g_mn;
static u64 g_bm[4];                     // b in the Montgomery form
static HcPt g_gm;                       // the base point, Montgomery projective
static volatile int g_p256_ready;

static u64 bn_add(u64 r[4], const u64 a[4], const u64 b[4])
{
    unsigned char c = 0;
    for (int i = 0; i < 4; i++) c = _addcarry_u64(c, a[i], b[i], &r[i]);
    return c;
}

static u64 bn_sub(u64 r[4], const u64 a[4], const u64 b[4])
{
    unsigned char c = 0;
    for (int i = 0; i < 4; i++) c = _subborrow_u64(c, a[i], b[i], &r[i]);
    return c;
}

static int bn_is_zero(const u64 a[4]) { return (a[0] | a[1] | a[2] | a[3]) == 0; }
static int bn_lt(const u64 a[4], const u64 b[4]) { u64 t[4]; return (int)bn_sub(t, a, b); }
static int bn_eq(const u64 a[4], const u64 b[4]) { return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2]) | (a[3] ^ b[3])) == 0; }

static void bn_from_be(u64 r[4], const u8 b[32]) { for (int i = 0; i < 4; i++) r[3 - i] = rd64be(b + 8 * i); }
static void bn_to_be(u8 b[32], const u64 a[4]) { for (int i = 0; i < 4; i++) wr64be(b + 8 * i, a[3 - i]); }

static void mod_add(u64 r[4], const u64 a[4], const u64 b[4], const HcMod *M)
{
    u64 t[4], s[4];
    u64 c = bn_add(t, a, b);
    u64 bo = bn_sub(s, t, M->m);
    u64 mask = 0 - (c | (bo ^ 1));              // carried, or no borrow: take t - m
    for (int i = 0; i < 4; i++) r[i] = (t[i] & ~mask) | (s[i] & mask);
}

static void mod_sub(u64 r[4], const u64 a[4], const u64 b[4], const HcMod *M)
{
    u64 t[4], s[4];
    u64 bo = bn_sub(t, a, b);
    bn_add(s, t, M->m);
    u64 mask = 0 - bo;
    for (int i = 0; i < 4; i++) r[i] = (t[i] & ~mask) | (s[i] & mask);
}

// a * b / 2^256 mod m (CIOS). r may be a or b.
static void mont_mul(u64 r[4], const u64 a[4], const u64 b[4], const HcMod *M)
{
    u64 t[6] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++) {
        u64 c = 0, hi, lo;
        unsigned char cf;
        for (int j = 0; j < 4; j++) {
            lo = _umul128(a[j], b[i], &hi);
            cf = _addcarry_u64(0, lo, t[j], &lo); hi += cf;
            cf = _addcarry_u64(0, lo, c, &lo);    hi += cf;
            t[j] = lo; c = hi;
        }
        cf = _addcarry_u64(0, t[4], c, &t[4]);
        t[5] = cf;
        u64 m = t[0] * M->inv;
        lo = _umul128(m, M->m[0], &hi);
        cf = _addcarry_u64(0, lo, t[0], &lo);
        c = hi + cf;
        for (int j = 1; j < 4; j++) {
            lo = _umul128(m, M->m[j], &hi);
            cf = _addcarry_u64(0, lo, t[j], &lo); hi += cf;
            cf = _addcarry_u64(0, lo, c, &lo);    hi += cf;
            t[j - 1] = lo; c = hi;
        }
        cf = _addcarry_u64(0, t[4], c, &t[3]);
        t[4] = t[5] + cf;
    }
    u64 s[4];
    u64 bo = bn_sub(s, t, M->m);
    u64 mask = 0 - (u64)((t[4] != 0) | (bo == 0));
    for (int i = 0; i < 4; i++) r[i] = (t[i] & ~mask) | (s[i] & mask);
}

static void mod_setup(HcMod *M, const u64 m[4])
{
    static const u64 zero[4] = { 0 };
    for (int i = 0; i < 4; i++) M->m[i] = m[i];
    u64 x = 1;
    for (int i = 0; i < 6; i++) x *= 2 - m[0] * x;      // m^-1 mod 2^64 (Newton)
    M->inv = 0 - x;
    bn_sub(M->one, zero, m);                            // 2^256 mod m (m > 2^255)
    u64 v[4];
    for (int i = 0; i < 4; i++) v[i] = M->one[i];
    for (int i = 0; i < 256; i++) mod_add(v, v, v, M); // 2^512 mod m
    for (int i = 0; i < 4; i++) M->rr[i] = v[i];
}

static void to_mont(u64 r[4], const u64 a[4], const HcMod *M) { mont_mul(r, a, M->rr, M); }
static void from_mont(u64 r[4], const u64 a[4], const HcMod *M) { static const u64 one[4] = { 1, 0, 0, 0 }; mont_mul(r, a, one, M); }

// a^e (Montgomery form in and out); e is public (m - 2).
static void mont_pow(u64 r[4], const u64 a[4], const u64 e[4], const HcMod *M)
{
    u64 x[4];
    for (int i = 0; i < 4; i++) x[i] = M->one[i];
    for (int b = 255; b >= 0; b--) {
        mont_mul(x, x, x, M);
        if ((e[b >> 6] >> (b & 63)) & 1) mont_mul(x, x, a, M);
    }
    for (int i = 0; i < 4; i++) r[i] = x[i];
}

static void mont_inv(u64 r[4], const u64 a[4], const HcMod *M)
{
    static const u64 two[4] = { 2, 0, 0, 0 };
    u64 e[4];
    bn_sub(e, M->m, two);
    mont_pow(r, a, e, M);
}

static void p256_init(void)
{
    if (g_p256_ready) return;
    mod_setup(&g_mp, P256_P);
    mod_setup(&g_mn, P256_N);
    to_mont(g_bm, P256_B, &g_mp);
    to_mont(g_gm.x, P256_GX, &g_mp);
    to_mont(g_gm.y, P256_GY, &g_mp);
    for (int i = 0; i < 4; i++) g_gm.z[i] = g_mp.one[i];
    _ReadWriteBarrier();
    g_p256_ready = 1;
}

#define FMUL(r, a, b) mont_mul(r, a, b, &g_mp)
#define FADD(r, a, b) mod_add(r, a, b, &g_mp)
#define FSUB(r, a, b) mod_sub(r, a, b, &g_mp)

// Complete addition (also doubling, also the point at infinity).
static void pt_add(HcPt *out, const HcPt *p, const HcPt *q)
{
    u64 t0[4], t1[4], t2[4], t3[4], t4[4], x3[4], y3[4], z3[4];
    FMUL(t0, p->x, q->x); FMUL(t1, p->y, q->y); FMUL(t2, p->z, q->z);
    FADD(t3, p->x, p->y); FADD(t4, q->x, q->y); FMUL(t3, t3, t4);
    FADD(t4, t0, t1);     FSUB(t3, t3, t4);     FADD(t4, p->y, p->z);
    FADD(x3, q->y, q->z); FMUL(t4, t4, x3);     FADD(x3, t1, t2);
    FSUB(t4, t4, x3);     FADD(x3, p->x, p->z); FADD(y3, q->x, q->z);
    FMUL(x3, x3, y3);     FADD(y3, t0, t2);     FSUB(y3, x3, y3);
    FMUL(z3, g_bm, t2);   FSUB(x3, y3, z3);     FADD(z3, x3, x3);
    FADD(x3, x3, z3);     FSUB(z3, t1, x3);     FADD(x3, t1, x3);
    FMUL(y3, g_bm, y3);   FADD(t1, t2, t2);     FADD(t2, t1, t2);
    FSUB(y3, y3, t2);     FSUB(y3, y3, t0);     FADD(t1, y3, y3);
    FADD(y3, t1, y3);     FADD(t1, t0, t0);     FADD(t0, t1, t0);
    FSUB(t0, t0, t2);     FMUL(t1, t4, y3);     FMUL(t2, t0, y3);
    FMUL(y3, x3, z3);     FADD(y3, y3, t2);     FMUL(x3, t3, x3);
    FSUB(x3, x3, t1);     FMUL(z3, t4, z3);     FMUL(t1, t3, t0);
    FADD(z3, z3, t1);
    for (int i = 0; i < 4; i++) { out->x[i] = x3[i]; out->y[i] = y3[i]; out->z[i] = z3[i]; }
}

static void pt_identity(HcPt *p)
{
    for (int i = 0; i < 4; i++) { p->x[i] = 0; p->y[i] = g_mp.one[i]; p->z[i] = 0; }
}

// k * P, k < 2^256: the same additions whatever k is.
static void pt_mul(HcPt *r, const HcPt *p, const u64 k[4])
{
    HcPt tab[16], acc, sel;
    pt_identity(&tab[0]);
    tab[1] = *p;
    for (int i = 2; i < 16; i++) pt_add(&tab[i], &tab[i - 1], p);
    pt_identity(&acc);
    for (int w = 63; w >= 0; w--) {
        for (int i = 0; i < 4; i++) pt_add(&acc, &acc, &acc);
        u64 d = (k[w >> 4] >> ((w & 15) * 4)) & 15;
        memset(&sel, 0, sizeof(sel));
        for (u64 i = 0; i < 16; i++) {
            u64 e = i ^ d;
            u64 mask = ((e | (0 - e)) >> 63) - 1;      // all ones when i == d
            for (int j = 0; j < 4; j++) {
                sel.x[j] |= tab[i].x[j] & mask;
                sel.y[j] |= tab[i].y[j] & mask;
                sel.z[j] |= tab[i].z[j] & mask;
            }
        }
        pt_add(&acc, &acc, &sel);
    }
    *r = acc;
    hc_wipe(tab, sizeof(tab)); hc_wipe(&sel, sizeof(sel));
}

// Affine coordinates in the normal form; 0 for the point at infinity.
static int pt_affine(const HcPt *p, u64 x[4], u64 y[4])
{
    if (bn_is_zero(p->z)) return 0;
    u64 zi[4];
    mont_inv(zi, p->z, &g_mp);
    FMUL(x, p->x, zi); FMUL(y, p->y, zi);
    from_mont(x, x, &g_mp); from_mont(y, y, &g_mp);
    return 1;
}

static int on_curve(const u64 x[4], const u64 y[4])
{
    if (!bn_lt(x, P256_P) || !bn_lt(y, P256_P)) return 0;
    u64 xm[4], ym[4], l[4], r[4], t[4];
    to_mont(xm, x, &g_mp); to_mont(ym, y, &g_mp);
    FMUL(l, ym, ym);                                   // y^2
    FMUL(r, xm, xm); FMUL(r, r, xm);                   // x^3
    FADD(t, xm, xm); FADD(t, t, xm); FSUB(r, r, t);    // - 3x
    FADD(r, r, g_bm);                                  // + b
    return bn_eq(l, r);
}

static void reduce_n(u64 a[4])                         // a < 2n -> a mod n
{
    u64 t[4];
    if (!bn_sub(t, a, P256_N)) for (int i = 0; i < 4; i++) a[i] = t[i];
}

#ifdef HCRYPT_TEST
int hc_test_p256_pub(const unsigned char priv[32], unsigned char pub[64])
{
    p256_init();
    u64 d[4], x[4], y[4];
    HcPt q;
    bn_from_be(d, priv);
    pt_mul(&q, &g_gm, d);
    if (!pt_affine(&q, x, y)) return 0;
    bn_to_be(pub, x);
    bn_to_be(pub + 32, y);
    return 1;
}
#endif

int hc_p256_keygen(unsigned char priv[32], unsigned char pub[64])
{
    if (!priv || !pub) return 0;
    p256_init();
    u64 d[4], x[4], y[4];
    for (int tries = 0; tries < 64; tries++) {
        hc_random_bytes(priv, 32);
        bn_from_be(d, priv);
        if (bn_is_zero(d) || !bn_lt(d, P256_N)) continue;
        HcPt q;
        pt_mul(&q, &g_gm, d);
        if (!pt_affine(&q, x, y)) continue;
        bn_to_be(pub, x);
        bn_to_be(pub + 32, y);
        hc_wipe(d, sizeof(d));
        return 1;
    }
    hc_wipe(d, sizeof(d));
    return 0;
}

static void hmac_rfc6979(const u8 k[32], const u8 *v, int sep, const u8 *x, const u8 *h1, u8 out[32])
{
    HcHmac m;
    hmac_start(&m, k, 32);
    sha_update(&m.i, v, 32);
    if (sep >= 0) {
        u8 b = (u8)sep;
        sha_update(&m.i, &b, 1);
        if (x) { sha_update(&m.i, x, 32); sha_update(&m.i, h1, 32); }
    }
    hmac_finish(&m, out);
}

int hc_p256_sign(const unsigned char priv[32], const unsigned char hash[32], unsigned char sig[64])
{
    if (!priv || !hash || !sig) return 0;
    p256_init();
    u64 d[4], e[4];
    bn_from_be(d, priv);
    if (bn_is_zero(d) || !bn_lt(d, P256_N)) return 0;
    bn_from_be(e, hash);
    reduce_n(e);
    u8 h1[32], V[32], K[32];
    bn_to_be(h1, e);                                   // bits2octets(H(m))
    memset(V, 1, 32);
    memset(K, 0, 32);
    hmac_rfc6979(K, V, 0, priv, h1, K); hmac_rfc6979(K, V, -1, 0, 0, V);
    hmac_rfc6979(K, V, 1, priv, h1, K); hmac_rfc6979(K, V, -1, 0, 0, V);
    int ok = 0;
    for (int tries = 0; tries < 64 && !ok; tries++) {
        u64 k[4], x[4], y[4];
        hmac_rfc6979(K, V, -1, 0, 0, V);
        bn_from_be(k, V);
        if (!bn_is_zero(k) && bn_lt(k, P256_N)) {
            HcPt R;
            pt_mul(&R, &g_gm, k);
            if (pt_affine(&R, x, y)) {
                reduce_n(x);                           // r = x mod n
                if (!bn_is_zero(x)) {
                    u64 km[4], rm[4], dm[4], em[4], s[4];
                    to_mont(km, k, &g_mn); to_mont(rm, x, &g_mn);
                    to_mont(dm, d, &g_mn); to_mont(em, e, &g_mn);
                    mont_mul(s, rm, dm, &g_mn);        // r d
                    mod_add(s, s, em, &g_mn);          // e + r d
                    mont_inv(km, km, &g_mn);           // 1 / k
                    mont_mul(s, s, km, &g_mn);
                    from_mont(s, s, &g_mn);
                    if (!bn_is_zero(s)) {
                        bn_to_be(sig, x);
                        bn_to_be(sig + 32, s);
                        ok = 1;
                    }
                    hc_wipe(km, 32); hc_wipe(dm, 32);
                }
            }
        }
        hc_wipe(k, 32);
        if (!ok) { hmac_rfc6979(K, V, 0, 0, 0, K); hmac_rfc6979(K, V, -1, 0, 0, V); }
    }
    hc_wipe(d, 32); hc_wipe(K, 32); hc_wipe(V, 32);
    return ok;
}

int hc_p256_verify(const unsigned char pub[64], const unsigned char hash[32], const unsigned char sig[64])
{
    if (!pub || !hash || !sig) return 0;
    p256_init();
    u64 qx[4], qy[4], r[4], s[4], e[4];
    bn_from_be(qx, pub); bn_from_be(qy, pub + 32);
    bn_from_be(r, sig);  bn_from_be(s, sig + 32);
    if (!on_curve(qx, qy)) return 0;
    if (bn_is_zero(r) || bn_is_zero(s) || !bn_lt(r, P256_N) || !bn_lt(s, P256_N)) return 0;
    bn_from_be(e, hash);
    reduce_n(e);
    u64 w[4], u1[4], u2[4], t[4];
    to_mont(w, s, &g_mn);
    mont_inv(w, w, &g_mn);                             // 1 / s
    to_mont(t, e, &g_mn); mont_mul(u1, t, w, &g_mn); from_mont(u1, u1, &g_mn);
    to_mont(t, r, &g_mn); mont_mul(u2, t, w, &g_mn); from_mont(u2, u2, &g_mn);
    HcPt q, a, b;
    to_mont(q.x, qx, &g_mp); to_mont(q.y, qy, &g_mp);
    for (int i = 0; i < 4; i++) q.z[i] = g_mp.one[i];
    pt_mul(&a, &g_gm, u1);
    pt_mul(&b, &q, u2);
    pt_add(&a, &a, &b);
    u64 x[4], y[4];
    if (!pt_affine(&a, x, y)) return 0;
    reduce_n(x);
    return bn_eq(x, r);
}

// ---- ECDH P-256 (Bluetooth LE Secure Connections, RFC 5903) ----

static int scalar_ok(const u64 d[4]) { return !bn_is_zero(d) && bn_lt(d, P256_N); }

int hc_p256_public(const unsigned char priv[32], unsigned char pub[64])
{
    if (!priv || !pub) return 0;
    p256_init();
    u64 d[4], x[4], y[4];
    bn_from_be(d, priv);
    int ok = 0;
    if (scalar_ok(d)) {
        HcPt q;
        pt_mul(&q, &g_gm, d);
        if (pt_affine(&q, x, y)) {
            bn_to_be(pub, x);
            bn_to_be(pub + 32, y);
            ok = 1;
        }
        hc_wipe(&q, sizeof(q));
    }
    hc_wipe(d, sizeof(d));
    return ok;
}

int hc_p256_keypair(unsigned char priv[32], unsigned char pub[64])
{
    return hc_p256_keygen(priv, pub);
}

// The peer's point is checked first (an invalid-curve attack gives nothing):
// both coordinates below p and y^2 = x^3 - 3x + b; (0,0) is not on it.
int hc_p256_shared(const unsigned char priv[32], const unsigned char peer[64], unsigned char out[32])
{
    if (!priv || !peer || !out) return 0;
    p256_init();
    u64 d[4], px[4], py[4], x[4], y[4];
    bn_from_be(px, peer);
    bn_from_be(py, peer + 32);
    if (!on_curve(px, py)) return 0;
    bn_from_be(d, priv);
    int ok = 0;
    if (scalar_ok(d)) {
        HcPt q, s;
        to_mont(q.x, px, &g_mp); to_mont(q.y, py, &g_mp);
        for (int i = 0; i < 4; i++) q.z[i] = g_mp.one[i];
        pt_mul(&s, &q, d);
        if (pt_affine(&s, x, y)) {                    // never infinity: n is prime, d in 1..n-1
            bn_to_be(out, x);
            ok = 1;
        }
        hc_wipe(&s, sizeof(s)); hc_wipe(x, 32); hc_wipe(y, 32);
    }
    hc_wipe(d, sizeof(d));
    return ok;
}

// ======================================================================
// gzip: deflate (RFC 1951) out, inflate.c in, the gzip wrapper (RFC 1952)
// ======================================================================

#define DF_WSIZE 32768
#define DF_WMASK (DF_WSIZE - 1)
#define DF_HBITS 15
#define DF_HSIZE (1 << DF_HBITS)
#define DF_SYMS  16384                  // symbols to a block
#define DF_CHAIN 128                    // zlib level 6: chain 128, good 8, nice 128, lazy 16
#define DF_GOOD  8
#define DF_NICE  128
#define DF_LAZY  16

typedef struct {
    i64 head[DF_HSIZE];                 // the newest position with that hash, -1 none
    i64 prev[DF_WSIZE];                 // the one before it, by position & DF_WMASK
    u16 sl[DF_SYMS];                    // < 256 a literal, else 256 + length - 3
    u16 sd[DF_SYMS];                    // distance - 1
    u32 lf[286], df[30];
    int ns;
    u64 covered;                        // input bytes the symbols stand for
    u8 *out;
    u64 cap, pos, bb;
    int bc, err;
} Defl;

static void df_bits(Defl *d, u32 v, int n)
{
    d->bb |= (u64)v << d->bc;
    d->bc += n;
    while (d->bc >= 8) {
        if (d->pos < d->cap) d->out[d->pos] = (u8)d->bb; else d->err = 1;
        d->pos++;
        d->bb >>= 8;
        d->bc -= 8;
    }
}

static void df_align(Defl *d)
{
    if (d->bc > 0) df_bits(d, 0, 8 - d->bc);
}

static void len_code(u32 len, u32 *code, u32 *eb, u32 *ev)
{
    u32 l = len - 3;
    if (l < 8)         { *code = 257 + l; *eb = 0; *ev = 0; }
    else if (l == 255) { *code = 285; *eb = 0; *ev = 0; }
    else {
        unsigned long nb;
        _BitScanReverse(&nb, l);
        *code = 257 + 4 * (nb - 1) + ((l >> (nb - 2)) & 3);
        *eb = nb - 2;
        *ev = l & ((1u << (nb - 2)) - 1);
    }
}

static void dist_code(u32 dist, u32 *code, u32 *eb, u32 *ev)
{
    u32 x = dist - 1;
    if (x < 4) { *code = x; *eb = 0; *ev = 0; }
    else {
        unsigned long nb;
        _BitScanReverse(&nb, x);
        *code = 2 * nb + ((x >> (nb - 1)) & 1);
        *eb = nb - 1;
        *ev = x & ((1u << (nb - 1)) - 1);
    }
}

// Code lengths for the frequencies, none longer than limit: Huffman, then the
// overflow moved down as miniz does, the longest codes to the rarest symbols.
static void huff_build(const u32 *freq, int n, int limit, u8 *len)
{
    int sym[288], par[576], dep[576];
    u32 w[576];
    int cnt = 0;
    for (int i = 0; i < n; i++) { len[i] = 0; if (freq[i]) sym[cnt++] = i; }
    if (cnt == 0) return;
    if (cnt == 1) { len[sym[0]] = 1; return; }
    for (int i = 1; i < cnt; i++) {                    // by frequency, rarest first
        int s = sym[i], j = i - 1;
        while (j >= 0 && freq[sym[j]] > freq[s]) { sym[j + 1] = sym[j]; j--; }
        sym[j + 1] = s;
    }
    for (int i = 0; i < cnt; i++) w[i] = freq[sym[i]];
    int li = 0, ii = cnt, nn = cnt;                    // two queues: leaves, inner nodes
    for (int k = 0; k < cnt - 1; k++) {
        int a, b;
        if (li < cnt && (ii >= nn || w[li] <= w[ii])) a = li++; else a = ii++;
        if (li < cnt && (ii >= nn || w[li] <= w[ii])) b = li++; else b = ii++;
        w[nn] = w[a] + w[b];
        par[a] = nn; par[b] = nn;
        nn++;
    }
    dep[nn - 1] = 0;
    for (int i = nn - 2; i >= 0; i--) dep[i] = dep[par[i]] + 1;
    int blc[16];
    for (int i = 0; i < 16; i++) blc[i] = 0;
    for (int i = 0; i < cnt; i++) blc[dep[i] > limit ? limit : dep[i]]++;
    u32 total = 0;
    for (int i = 1; i <= limit; i++) total += (u32)blc[i] << (limit - i);
    while (total != (1u << limit)) {
        blc[limit]--;
        for (int i = limit - 1; i > 0; i--)
            if (blc[i]) { blc[i]--; blc[i + 1] += 2; break; }
        total--;
    }
    int k = 0;
    for (int l = limit; l >= 1; l--)
        for (int c = blc[l]; c > 0; c--) len[sym[k++]] = (u8)l;
}

static void huff_codes(const u8 *len, int n, u16 *code)
{
    u32 blc[16], next[16], c = 0;
    for (int i = 0; i < 16; i++) blc[i] = 0;
    for (int i = 0; i < n; i++) blc[len[i]]++;
    blc[0] = 0;
    next[0] = 0;
    for (int b = 1; b < 16; b++) { c = (c + blc[b - 1]) << 1; next[b] = c; }
    for (int i = 0; i < n; i++) {
        code[i] = 0;
        if (!len[i]) continue;
        u32 v = next[len[i]]++, r = 0;
        for (int b = 0; b < len[i]; b++) { r = (r << 1) | (v & 1); v >>= 1; }
        code[i] = (u16)r;                              // bit-reversed: deflate packs from the low bit
    }
}

static u8 fixed_len(int sym) { return sym < 144 ? 8 : sym < 256 ? 9 : sym < 280 ? 7 : 8; }

static void df_symbols(Defl *d, const u16 *lc, const u8 *ll, const u16 *dc, const u8 *dl)
{
    for (int i = 0; i < d->ns; i++) {
        u32 s = d->sl[i];
        if (s < 256) { df_bits(d, lc[s], ll[s]); continue; }
        u32 c, eb, ev;
        len_code(s - 256 + 3, &c, &eb, &ev);
        df_bits(d, lc[c], ll[c]);
        if (eb) df_bits(d, ev, (int)eb);
        dist_code((u32)d->sd[i] + 1, &c, &eb, &ev);
        df_bits(d, dc[c], dl[c]);
        if (eb) df_bits(d, ev, (int)eb);
    }
    df_bits(d, lc[256], ll[256]);
}

static void df_flush(Defl *d, const u8 *src, u64 bstart, int final)
{
    static const u8 lext[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
    static const u8 dext[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
    static const u8 ord[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
    u64 blen = d->covered;
    d->lf[256] = 1;

    // dynamic Huffman: at least two codes in each tree, as zlib makes them
    u32 lfc[286], dfc[30];
    memcpy(lfc, d->lf, sizeof(lfc));
    memcpy(dfc, d->df, sizeof(dfc));
    int nz = 0;
    for (int i = 0; i < 286; i++) nz += lfc[i] != 0;
    if (nz < 2) lfc[lfc[0] ? 1 : 0] = 1;
    nz = 0;
    for (int i = 0; i < 30; i++) nz += dfc[i] != 0;
    for (int i = 0; nz < 2 && i < 2; i++) if (!dfc[i]) { dfc[i] = 1; nz++; }
    u8 ll[286], dl[30];
    huff_build(lfc, 286, 15, ll);
    huff_build(dfc, 30, 15, dl);
    int hlit = 286, hdist = 30;
    while (hlit > 257 && !ll[hlit - 1]) hlit--;
    while (hdist > 1 && !dl[hdist - 1]) hdist--;
    u8 all[316], rs[316], rx[316];
    int total = hlit + hdist, nr = 0;
    memcpy(all, ll, (size_t)hlit);
    memcpy(all + hlit, dl, (size_t)hdist);
    u32 cf[19];
    for (int i = 0; i < 19; i++) cf[i] = 0;
    for (int i = 0; i < total;) {                      // run-length codes 16, 17, 18
        u8 cur = all[i];
        int run = 1;
        while (i + run < total && all[i + run] == cur) run++;
        if (cur == 0) {
            while (run >= 11) { int r = run > 138 ? 138 : run; rs[nr] = 18; rx[nr++] = (u8)(r - 11); run -= r; i += r; }
            if (run >= 3) { rs[nr] = 17; rx[nr++] = (u8)(run - 3); i += run; run = 0; }
            while (run > 0) { rs[nr] = 0; rx[nr++] = 0; run--; i++; }
        } else {
            rs[nr] = cur; rx[nr++] = 0; run--; i++;
            while (run >= 3) { int r = run > 6 ? 6 : run; rs[nr] = 16; rx[nr++] = (u8)(r - 3); run -= r; i += r; }
            while (run > 0) { rs[nr] = cur; rx[nr++] = 0; run--; i++; }
        }
    }
    for (int i = 0; i < nr; i++) cf[rs[i]]++;
    u8 cl[19];
    huff_build(cf, 19, 7, cl);
    int hclen = 19;
    while (hclen > 4 && !cl[ord[hclen - 1]]) hclen--;

    u64 extra = 0, dyn = 3 + 5 + 5 + 4 + 3 * (u64)hclen, fix = 3, sto;
    for (int i = 257; i < 286; i++) extra += (u64)d->lf[i] * lext[i - 257];
    for (int i = 0; i < 30; i++) extra += (u64)d->df[i] * dext[i];
    for (int i = 0; i < nr; i++) dyn += cl[rs[i]] + (rs[i] == 16 ? 2 : rs[i] == 17 ? 3 : rs[i] == 18 ? 7 : 0);
    for (int i = 0; i < 286; i++) { dyn += (u64)d->lf[i] * ll[i]; fix += (u64)d->lf[i] * fixed_len(i); }
    for (int i = 0; i < 30; i++) { dyn += (u64)d->df[i] * dl[i]; fix += (u64)d->df[i] * 5; }
    dyn += extra;
    fix += extra;
    u64 nst = blen ? (blen + 65534) / 65535 : 1;
    sto = 3 + (u64)((8 - (d->bc + 3) % 8) % 8) + 32 + (nst - 1) * 40 + 8 * blen;

    if (sto < dyn && sto < fix) {
        const u8 *p = src + bstart;
        u64 left = blen;
        do {
            u32 take = left > 65535 ? 65535 : (u32)left;
            left -= take;
            df_bits(d, final && !left ? 1 : 0, 1);
            df_bits(d, 0, 2);
            df_align(d);
            df_bits(d, take, 16);
            df_bits(d, ~take & 0xFFFF, 16);
            if (d->pos + take <= d->cap) memcpy(d->out + d->pos, p, take); else d->err = 1;
            d->pos += take;
            p += take;
        } while (left > 0);
    } else if (fix <= dyn) {
        u8 fl[288], fd[30];
        u16 fc[288], fdc[30];
        for (int i = 0; i < 288; i++) fl[i] = fixed_len(i);
        for (int i = 0; i < 30; i++) fd[i] = 5;
        huff_codes(fl, 288, fc);
        huff_codes(fd, 30, fdc);
        df_bits(d, final ? 1 : 0, 1);
        df_bits(d, 1, 2);
        df_symbols(d, fc, fl, fdc, fd);
    } else {
        u16 lc[286], dc[30], cc[19];
        huff_codes(ll, 286, lc);
        huff_codes(dl, 30, dc);
        huff_codes(cl, 19, cc);
        df_bits(d, final ? 1 : 0, 1);
        df_bits(d, 2, 2);
        df_bits(d, (u32)(hlit - 257), 5);
        df_bits(d, (u32)(hdist - 1), 5);
        df_bits(d, (u32)(hclen - 4), 4);
        for (int i = 0; i < hclen; i++) df_bits(d, cl[ord[i]], 3);
        for (int i = 0; i < nr; i++) {
            df_bits(d, cc[rs[i]], cl[rs[i]]);
            if (rs[i] == 16) df_bits(d, rx[i], 2);
            else if (rs[i] == 17) df_bits(d, rx[i], 3);
            else if (rs[i] == 18) df_bits(d, rx[i], 7);
        }
        df_symbols(d, lc, ll, dc, dl);
    }
    memset(d->lf, 0, sizeof(d->lf));
    memset(d->df, 0, sizeof(d->df));
    d->ns = 0;
    d->covered = 0;
}

static __forceinline u32 df_hash(const u8 *p)
{
    return (((u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16)) * 0x9E3779B1u) >> (32 - DF_HBITS);
}

static __forceinline void df_insert(Defl *d, const u8 *src, i64 pos)
{
    u32 h = df_hash(src + pos);
    d->prev[pos & DF_WMASK] = d->head[h];
    d->head[h] = pos;
}

static __forceinline u64 ld64(const u8 *p) { return *(const u64 __unaligned *)p; }

static int match_len(const u8 *s, const u8 *m, int maxlen)
{
    int l = 0;
    while (l + 8 <= maxlen) {
        u64 x = ld64(s + l) ^ ld64(m + l);
        if (x) { unsigned long b; _BitScanForward64(&b, x); return l + (int)(b >> 3); }
        l += 8;
    }
    while (l < maxlen && s[l] == m[l]) l++;
    return l;
}

// The longest match better than best at pos (pos already inserted), or 0.
static int df_match(Defl *d, const u8 *src, i64 pos, i64 n, int best, u32 *dist)
{
    int maxlen = n - pos < 258 ? (int)(n - pos) : 258;
    if (maxlen < 3 || best >= maxlen) return 0;
    int chain = best >= DF_GOOD ? DF_CHAIN / 4 : DF_CHAIN, found = 0;
    const u8 *s = src + pos;
    i64 cand = d->prev[pos & DF_WMASK];
    while (cand >= 0 && chain-- > 0) {
        i64 dd = pos - cand;
        if (dd <= 0 || dd >= DF_WSIZE) break;
        const u8 *m = src + cand;
        if (m[best] == s[best] && m[0] == s[0] && m[1] == s[1]) {
            int l = match_len(s, m, maxlen);
            if (l > best) {
                best = l;
                *dist = (u32)dd;
                found = 1;
                if (l >= DF_NICE || l >= maxlen) break;
            }
        }
        i64 nx = d->prev[cand & DF_WMASK];
        if (nx >= cand) break;
        cand = nx;
    }
    return found ? best : 0;
}

static void df_lit(Defl *d, u8 c)
{
    d->sl[d->ns] = c;
    d->sd[d->ns++] = 0;
    d->lf[c]++;
    d->covered++;
}

static void df_pair(Defl *d, int len, u32 dist)
{
    u32 c, eb, ev;
    d->sl[d->ns] = (u16)(256 + len - 3);
    d->sd[d->ns++] = (u16)(dist - 1);
    len_code((u32)len, &c, &eb, &ev);
    d->lf[c]++;
    dist_code(dist, &c, &eb, &ev);
    d->df[c]++;
    d->covered += (u64)len;
}

// LZ77 with lazy matching, as zlib's deflate_slow: src[start..n) is coded,
// src[0..start) is what came before (already in the hash chains) and may be
// matched against. The last block is final only if `final`; otherwise the
// bits not yet a whole byte stay in d for the next piece.
static void df_compress_from(Defl *d, const u8 *src, i64 start, i64 n, int final)
{
    i64 pos = start;
    u64 bstart = (u64)start;
    int plen = 0, pending = 0;
    u32 pdist = 0;
    while (pos < n) {
        int len = 0;
        u32 dist = 0;
        if (pos + 3 <= n) {
            df_insert(d, src, pos);
            if (!(pending && plen >= DF_LAZY)) len = df_match(d, src, pos, n, pending && plen > 2 ? plen : 2, &dist);
        }
        if (pending && plen >= 3 && len <= plen) {
            df_pair(d, plen, pdist);
            i64 end = pos - 1 + plen;
            for (i64 q = pos + 1; q < end; q++) if (q + 3 <= n) df_insert(d, src, q);
            pos = end;
            pending = 0;
            plen = 0;
        } else {
            if (pending) df_lit(d, src[pos - 1]);
            pending = 1;
            plen = len;
            pdist = dist;
            pos++;
        }
        if (d->ns >= DF_SYMS - 2) {
            u64 cov = d->covered;
            df_flush(d, src, bstart, 0);
            bstart += cov;
        }
    }
    if (pending) df_lit(d, src[pos - 1]);
    if (d->covered || final) df_flush(d, src, bstart, final);
    if (final) df_align(d);
}

static void df_compress(Defl *d, const u8 *src, i64 n) { df_compress_from(d, src, 0, n, 1); }

// ---- deflate in pieces (a ZIP entry of any size, zipwrite.c) ----
// One raw deflate stream made of pieces of up to HC_DEFLATE_PIECE bytes; the
// last 32 KB of each piece stay as the next one's dictionary, so the stream
// packs about as well as one made whole. Each block is coded the shortest of
// stored, fixed and dynamic: a piece never grows by more than 5 bytes per
// 64 KB plus a few.

struct HcDeflate {
    Defl d;
    i64 hist;                           // bytes of the last piece kept at buf's start
    u8 buf[DF_WSIZE + HC_DEFLATE_PIECE + 8];
};

HcDeflate *hc_deflate_new(void)
{
    u64 pages = (sizeof(HcDeflate) + PAGE_SIZE - 1) / PAGE_SIZE;
    HcDeflate *z = page_alloc(pages);
    if (!z) return 0;
    Defl *d = &z->d;
    d->ns = 0; d->covered = 0; d->bb = 0; d->bc = 0; d->err = 0;
    memset(d->lf, 0, sizeof(d->lf));
    memset(d->df, 0, sizeof(d->df));
    z->hist = 0;
    return z;
}

long long hc_deflate_piece(HcDeflate *z, const void *in, long long n, int last, void *out, long long cap)
{
    if (!z || n < 0 || n > HC_DEFLATE_PIECE || (n && !in) || !out) return -1;
    Defl *d = &z->d;
    memcpy(z->buf + z->hist, in, (u64)n);
    i64 total = z->hist + n;
    memset(d->head, 0xFF, sizeof(d->head));             // the chains again, over the dictionary
    for (i64 q = 0; q < z->hist && q + 3 <= total; q++) df_insert(d, z->buf, q);
    d->out = out;
    d->cap = (u64)cap;
    d->pos = 0;
    d->err = 0;
    df_compress_from(d, z->buf, z->hist, total, last);
    if (d->err) return -1;
    i64 keep = total < DF_WSIZE ? total : DF_WSIZE;     // the dictionary of the next piece
    memmove(z->buf, z->buf + total - keep, (u64)keep);
    z->hist = keep;
    return (long long)d->pos;
}

void hc_deflate_free(HcDeflate *z)
{
    if (z) page_free(z, (sizeof(HcDeflate) + PAGE_SIZE - 1) / PAGE_SIZE);
}

long long hc_gzip_pack(const void *in, long long n, void *out, long long cap)
{
    if (n < 0 || (n && !in) || !out || cap < 18) return -1;
    u64 pages = (sizeof(Defl) + PAGE_SIZE - 1) / PAGE_SIZE;
    Defl *d = page_alloc(pages);
    if (!d) return -1;
    memset(d->head, 0xFF, sizeof(d->head));
    d->ns = 0; d->covered = 0; d->bb = 0; d->bc = 0; d->err = 0;
    memset(d->lf, 0, sizeof(d->lf));
    memset(d->df, 0, sizeof(d->df));
    u8 *o = out;
    static const u8 hdr[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 255 };   // no name, no time, OS unknown
    memcpy(o, hdr, 10);
    d->out = o;
    d->cap = (u64)cap - 8;
    d->pos = 10;
    df_compress(d, in, n);
    int err = d->err;
    u64 pos = d->pos;
    page_free(d, pages);
    if (err || pos + 8 > (u64)cap) return -1;
    wr32le(o + pos, crc_update(0, in, (u64)n));
    wr32le(o + pos + 4, (u32)n);
    return (long long)(pos + 8);
}

long long hc_gzip_unpacked_size(const void *in, long long n)
{
    const u8 *p = in;
    if (!p || n < 18 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8) return -1;
    return rd32le(p + n - 4);
}

long long hc_gzip_unpack(const void *in, long long n, void *out, long long cap)
{
    const u8 *p = in;
    if (!p || n < 18 || cap < 0 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8 || (p[3] & 0xE0)) return -1;
    u8 flg = p[3];
    i64 pos = 10;
    if (flg & 4) { if (pos + 2 > n) return -1; pos += 2 + (p[pos] | ((i64)p[pos + 1] << 8)); }   // FEXTRA
    if (flg & 8)  { while (pos < n && p[pos]) pos++; pos++; }                                     // FNAME
    if (flg & 16) { while (pos < n && p[pos]) pos++; pos++; }                                     // FCOMMENT
    if (flg & 2) pos += 2;                                                                        // FHCRC
    if (pos > n - 8) return -1;
    u32 crc = rd32le(p + n - 8), isize = rd32le(p + n - 4);
    if ((i64)isize > cap) return -1;                    // the true size is isize + k * 2^32
    u64 olen = 0;
    u8 *buf = inflate_buf(p + pos, (u64)(n - 8 - pos), &olen);
    if (!buf) return (olen == 0 && isize == 0 && crc == 0) ? 0 : -1;
    i64 r = -1;
    if (olen <= (u64)cap && (u32)olen == isize && crc_update(0, buf, olen) == crc) {
        if (olen) {
            if (!out) { kfree(buf); return -1; }
            memcpy(out, buf, (size_t)olen);
        }
        r = (i64)olen;
    }
    kfree(buf);
    return r;
}

// ======================================================================
// local_seal: secrets sealed to this installation (DPAPI's place).
// The key is 32 random bytes in %LOCALAPPDATA%\hcrypt\hcrypt.key, made at the first use.
// Sealed: nonce (12) || AES-256-GCM ciphertext || tag (16).
// ======================================================================

#define LOCAL_KEY_FILE "hcrypt.key"
static const char g_local_aad[] = "DynastyOS local_seal v1";
static u8 g_local_key[32];
static int g_local_ready;

static int local_key(void)
{
    if (g_local_ready) return 1;
    u64 sz = 0;
    const char *k = fs_read(LOCAL_KEY_FILE, &sz);
    if (k) {
        if (sz != 32) return 0;                         // damaged: never replaced silently
        memcpy(g_local_key, k, 32);
        g_local_ready = 1;
        return 1;
    }
    u8 nk[32];
    hc_random_bytes(nk, 32);
    if (!fs_write(LOCAL_KEY_FILE, (const char *)nk, 32)) { hc_wipe(nk, 32); return 0; }
    memcpy(g_local_key, nk, 32);
    hc_wipe(nk, 32);
    g_local_ready = 1;
    return 1;
}

#ifdef HCRYPT_TEST
void hc_test_local_reset(void) { g_local_ready = 0; }
#endif

long long hc_local_seal(const void *in, long long n, void *out, long long cap)
{
    if (n < 0 || (n && !in) || !out || cap < n + 28 || !local_key()) return -1;
    u8 *o = out;
    if (n) memmove(o + 12, in, (size_t)n);              // out may be in
    hc_random_bytes(o, 12);
    hc_aes256gcm_encrypt(g_local_key, o, g_local_aad, (int)sizeof(g_local_aad) - 1, o + 12, n, o + 12, o + 12 + n);
    return n + 28;
}

long long hc_local_unseal(const void *in, long long n, void *out, long long cap)
{
    if (!in || n < 28 || !out || cap < n - 28 || !local_key()) return -1;
    const u8 *p = in;
    long long m = n - 28;
    if (!hc_aes256gcm_decrypt(g_local_key, p, g_local_aad, (int)sizeof(g_local_aad) - 1, p + 12, m, p + 12 + m, out))
        return -1;
    return m;
}
