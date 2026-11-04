// Bit fields at work: reading with the sign, writing without touching the neighbours,
// op=, ++ and --, the value of an assignment, initializers (global and local, with names,
// unnamed fields passed over), through pointers and arrays, copies, unions, _Bool, 64 bits.
// How they are laid out is checked against MSVC by tests\winabi\bits.c.
#include "test.h"

struct Flags {
    unsigned ready : 1;
    unsigned mode : 3;
    int delta : 5;
    unsigned : 3;               // unnamed: initializers pass it over
    unsigned level : 4;
};

struct Wide {
    long long big : 40;
    unsigned long long ubig : 24;
    unsigned long long all : 64;
};

struct Mixed {
    char c;
    int a : 7;
    _Bool on : 1;
    short s : 9;
};

union Bits {
    unsigned u;
    struct { unsigned lo : 8, mid : 16, hi : 8; };
};

struct Flags g1 = { 1, 5, -3, 9 };
struct Flags g2 = { .level = 15, .delta = 7 };
struct Wide gw = { -5, 0xABCDEF, 0x123456789ABCDEF0ull };
union Bits gu = { 0x11223344 };

static int ready_of(struct Flags f) { return f.ready; }
static int sum(struct Flags *f) { return f->mode + f->delta + f->level; }

int main(void)
{
    ASSERT(4, sizeof(struct Flags));
    ASSERT(16, sizeof(struct Wide));
    ASSERT(12, sizeof(struct Mixed));

    // globals
    ASSERT(1, g1.ready);
    ASSERT(5, g1.mode);
    ASSERT(-3, g1.delta);
    ASSERT(9, g1.level);
    ASSERT(0, g2.ready);
    ASSERT(7, g2.delta);
    ASSERT(15, g2.level);
    ASSERT(-5, gw.big);
    ASSERT(0xABCDEF, gw.ubig);
    ASSERT(1, gw.all == 0x123456789ABCDEF0ull);
    ASSERT(0x44, gu.lo);
    ASSERT(0x2233, gu.mid);
    ASSERT(0x11, gu.hi);

    // locals
    struct Flags f = { 0, 7, 15, 3 };
    ASSERT(0, f.ready);
    ASSERT(7, f.mode);
    ASSERT(15, f.delta);
    ASSERT(3, f.level);
    struct Flags z = { 0 };
    ASSERT(0, z.mode | z.delta | z.level);

    // the neighbours stay
    f.mode = 2;
    ASSERT(2, f.mode);
    ASSERT(15, f.delta);
    ASSERT(3, f.level);
    f.delta = -16;
    ASSERT(-16, f.delta);
    ASSERT(2, f.mode);
    ASSERT(3, f.level);

    // too big: only the low bits; the value of the assignment is what the field holds
    ASSERT(0, f.mode = 8);
    ASSERT(-1, f.delta = 31);
    ASSERT(15, f.level = 0x1F);
    ASSERT(1, f.ready = 3);

    // op=, ++, --
    f.level = 3;
    f.level += 4;
    ASSERT(7, f.level);
    f.level *= 3;
    ASSERT(5, f.level);         // 21 in 4 bits
    f.delta = 10;
    f.delta -= 25;
    ASSERT(-15, f.delta);
    f.mode = 6;
    ASSERT(6, f.mode++);
    ASSERT(7, f.mode);
    ASSERT(0, ++f.mode);        // 8 in 3 bits
    ASSERT(0, f.mode--);
    ASSERT(7, f.mode);
    f.mode |= 0;
    f.mode &= 5;
    ASSERT(5, f.mode);
    f.mode <<= 1;
    ASSERT(2, f.mode);

    // through a pointer, in an array, as an argument, copied
    struct Flags arr[3] = { { 1, 1, 1, 1 }, { 0, 2, -2, 2 }, { 1, 3, 3, 3 } };
    struct Flags *p = &arr[1];
    ASSERT(2, sum(p));
    p->level += 10;
    ASSERT(12, arr[1].level);
    ASSERT(1, ready_of(arr[2]));
    struct Flags copy = arr[2];
    copy.ready = 0;
    ASSERT(1, arr[2].ready);
    ASSERT(0, copy.ready);
    ASSERT(3, copy.level);
    for (int i = 0; i < 3; i++)
        arr[i].mode = (unsigned)i + 4;
    ASSERT(4 + 5 + 6, arr[0].mode + arr[1].mode + arr[2].mode);

    // _Bool, char and short neighbours, 64 bits
    struct Mixed m = { 'x', -64, 1, 255 };
    ASSERT('x', m.c);
    ASSERT(-64, m.a);
    ASSERT(1, m.on);
    ASSERT(255, m.s);
    m.on = 5;
    ASSERT(1, m.on);
    m.s = -256;
    ASSERT(-256, m.s);
    ASSERT(-64, m.a);
    struct Wide w = gw;
    w.big = -549755813888LL;    // the smallest of 40 bits
    ASSERT(1, w.big == -549755813888LL);
    w.big -= 1;                 // round to the largest
    ASSERT(1, w.big == 549755813887LL);
    ASSERT(0xABCDEF, w.ubig);
    w.all = ~0ull;
    ASSERT(1, w.all == ~0ull);
    ASSERT(0xABCDEF, w.ubig);

    // a union: the fields share the word
    union Bits u;
    u.u = 0;
    u.mid = 0xFFFF;
    ASSERT(0x00FFFF00, u.u);
    u.hi = 0xA5;
    ASSERT(0xA5FFFF00, u.u);
    return g_fail;
}
