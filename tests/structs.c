#include "test.h"
struct Point { int x, y; };
struct Big { long a, b, c; };
typedef struct Node { int val; struct Node *next; } Node;
union U { int i; char c[4]; };
struct Point make(int x, int y) { struct Point p; p.x = x; p.y = y; return p; }
struct Big make_big(long a) { struct Big b; b.a = a; b.b = a * 2; b.c = a * 3; return b; }
long sum_big(struct Big b) { b.a = 1000; return b.b + b.c; }
int dist(struct Point p) { return p.x + p.y; }
struct Point g_pt = { 7, 8 };
int main(void)
{
    struct Point p = { 1, 2 };
    ASSERT(1, p.x); ASSERT(2, p.y);
    ASSERT(8, sizeof(p));
    struct Point *pp = &p;
    pp->y = 20;
    ASSERT(20, p.y);
    struct Point q = make(3, 4);
    ASSERT(7, dist(q));
    struct Big b = make_big(5);
    ASSERT(5, b.a); ASSERT(10, b.b); ASSERT(15, b.c);
    ASSERT(25, sum_big(b));
    ASSERT(5, b.a);
    Node n3 = { 3, 0 }, n2 = { 2, &n3 }, n1 = { 1, &n2 };
    int s = 0;
    for (Node *n = &n1; n; n = n->next) s += n->val;
    ASSERT(6, s);
    union U u;
    u.i = 0x01020304;
    ASSERT(4, u.c[0]);
    ASSERT(4, sizeof(u));
    struct { char c; int i; } pad;
    ASSERT(8, sizeof(pad));
    struct Point arr[3] = { { 1, 2 }, { 3, 4 }, [2].x = 9 };
    ASSERT(9, arr[2].x); ASSERT(0, arr[2].y); ASSERT(4, arr[1].y);
    struct Point r = { .y = 5, .x = 6 };
    ASSERT(6, r.x);
    q = p;
    ASSERT(20, q.y);
    ASSERT(7, g_pt.x);
    struct { int a; struct { int b; }; } anon;
    anon.b = 3;
    ASSERT(3, anon.b);
    enum Color { RED, GREEN = 5, BLUE };
    ASSERT(6, BLUE);
    return g_fail;
}
