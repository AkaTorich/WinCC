// Deflate decompression (RFC 1951) with an optional zlib wrapper (RFC 1950):
// stored, fixed-Huffman and dynamic-Huffman blocks. Used for PDF streams
// (FlateDecode) and PNG. A damaged or cut stream yields what was decoded.
// Ported from DynastyOS (src\inflate.c); here gzip_unpack uses it.
#include "hc_internal.h"

typedef struct {
    const u8 *in;
    u64 inlen, inpos;
    u32 bitbuf;
    int bitcnt;
    u8 *out;
    u64 outlen, outcap;
    int err;
} Inf;

typedef struct { u16 count[16]; u16 symbol[320]; } Huff;

static int need_out(Inf *s, u64 n)
{
    if (s->outlen + n <= s->outcap) return 1;
    u64 nc = s->outcap ? s->outcap : 65536;
    while (nc < s->outlen + n) nc *= 2;
    u8 *b = kmalloc(nc);
    if (!b) { s->err = 1; return 0; }
    if (s->out) { memcpy(b, s->out, s->outlen); kfree(s->out); }
    s->out = b;
    s->outcap = nc;
    return 1;
}

static int bits(Inf *s, int need)
{
    u32 v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) { s->err = 1; return 0; }
        v |= (u32)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1u << need) - 1));
}

static int build(Huff *h, const u8 *len, int n)
{
    for (int i = 0; i < 16; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[len[i]]++;
    if (h->count[0] == n) return 0;
    u16 offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; i++)
        if (len[i]) h->symbol[offs[len[i]]++] = (u16)i;
    return 1;
}

static int decode(Inf *s, const Huff *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= bits(s, 1);
        if (s->err) return -1;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->err = 1;
    return -1;
}

static const u16 g_lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const u8  g_lext[29]  = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const u16 g_dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const u8  g_dext[30]  = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static int codes(Inf *s, const Huff *lc, const Huff *dc)
{
    for (;;) {
        int sym = decode(s, lc);
        if (sym < 0) return -1;
        if (sym < 256) {
            if (!need_out(s, 1)) return -1;
            s->out[s->outlen++] = (u8)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) { s->err = 1; return -1; }
            int len = g_lbase[sym] + bits(s, g_lext[sym]);
            int ds = decode(s, dc);
            if (ds < 0 || ds >= 30) { s->err = 1; return -1; }
            u64 dist = g_dbase[ds] + (u64)bits(s, g_dext[ds]);
            if (s->err || dist > s->outlen) { s->err = 1; return -1; }
            if (!need_out(s, (u64)len)) return -1;
            u8 *o = s->out + s->outlen;
            const u8 *from = o - dist;
            for (int i = 0; i < len; i++) o[i] = from[i];
            s->outlen += (u64)len;
        }
    }
}

static int stored(Inf *s)
{
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->inpos + 4 > s->inlen) { s->err = 1; return -1; }
    u32 len = s->in[s->inpos] | ((u32)s->in[s->inpos + 1] << 8);
    s->inpos += 4;
    if (s->inpos + len > s->inlen) len = (u32)(s->inlen - s->inpos);
    if (!need_out(s, len)) return -1;
    memcpy(s->out + s->outlen, s->in + s->inpos, len);
    s->outlen += len;
    s->inpos += len;
    return 0;
}

static int fixed(Inf *s)
{
    static Huff lc, dc;
    static int ready;
    if (!ready) {
        u8 len[288];
        int i = 0;
        for (; i < 144; i++) len[i] = 8;
        for (; i < 256; i++) len[i] = 9;
        for (; i < 280; i++) len[i] = 7;
        for (; i < 288; i++) len[i] = 8;
        build(&lc, len, 288);
        for (i = 0; i < 30; i++) len[i] = 5;
        build(&dc, len, 30);
        ready = 1;
    }
    return codes(s, &lc, &dc);
}

static int dynamic(Inf *s)
{
    static const u8 order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    u8 len[320];
    Huff lc, dc;
    int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
    if (s->err || nlen > 286 || ndist > 30) { s->err = 1; return -1; }
    for (int i = 0; i < 19; i++) len[i] = 0;
    for (int i = 0; i < ncode; i++) len[order[i]] = (u8)bits(s, 3);
    if (!build(&lc, len, 19)) { s->err = 1; return -1; }
    int idx = 0;
    while (idx < nlen + ndist) {
        int sym = decode(s, &lc);
        if (sym < 0) return -1;
        if (sym < 16) { len[idx++] = (u8)sym; continue; }
        int rep = 0, val = 0;
        if (sym == 16) { if (!idx) { s->err = 1; return -1; } val = len[idx - 1]; rep = 3 + bits(s, 2); }
        else if (sym == 17) rep = 3 + bits(s, 3);
        else rep = 11 + bits(s, 7);
        if (idx + rep > nlen + ndist) { s->err = 1; return -1; }
        while (rep--) len[idx++] = (u8)val;
    }
    build(&lc, len, nlen);
    build(&dc, len + nlen, ndist);
    return codes(s, &lc, &dc);
}

// Inflates src (zlib-wrapped or raw deflate). Returns a kmalloc'ed buffer
// (one spare zero byte at the end) and its length, or NULL if nothing decoded.
static u8 *run(Inf *sp, u64 *outlen);

u8 *inflate_buf(const u8 *src, u64 len, u64 *outlen)
{
    Inf s;
    memset(&s, 0, sizeof(s));
    s.in = src;
    s.inlen = len;
    if (len >= 2 && (src[0] & 0x0F) == 8 && ((src[0] << 8) | src[1]) % 31 == 0)
        s.inpos = 2;                                    // zlib header
    return run(&s, outlen);
}

// Raw deflate only (a ZIP entry: no wrapper to guess at). hint: the size it
// should come to (0: unknown), the buffer made so big at once. *bad: the
// stream was damaged or cut (what was decoded still comes back).
u8 *inflate_raw(const u8 *src, u64 len, u64 hint, u64 *outlen, int *bad)
{
    Inf s;
    memset(&s, 0, sizeof(s));
    s.in = src;
    s.inlen = len;
    if (hint && hint < (1ull << 34)) need_out(&s, hint);
    if (s.err) { *outlen = 0; if (bad) *bad = 1; return 0; }
    u8 *r = run(&s, outlen);
    if (bad) *bad = s.err;
    if (!r && !hint) return 0;
    if (!r) { r = kmalloc(1); if (r) r[0] = 0; *outlen = 0; }   // an empty file is not a failure
    return r;
}

static u8 *run(Inf *sp, u64 *outlen)
{
    Inf s = *sp;
    int last;
    do {
        last = bits(&s, 1);
        int type = bits(&s, 2);
        if (s.err) break;
        int r = type == 0 ? stored(&s) : type == 1 ? fixed(&s) : type == 2 ? dynamic(&s) : (s.err = 1, -1);
        if (r < 0) break;
    } while (!last);
    sp->err = s.err;
    if (!s.outlen && !s.out) { *outlen = 0; return 0; }
    need_out(&s, 1);
    if (s.out) s.out[s.outlen] = 0;
    *outlen = s.outlen;
    return s.out;
}
