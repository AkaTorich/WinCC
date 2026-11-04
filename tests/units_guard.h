// A header of units.c and its .part sources, with an include guard.
#ifndef UNITS_GUARD_H
#define UNITS_GUARD_H
#define HDR_VERSION 3
struct Pt { int x, y; };
static int hdr_calls;
static int hdr_bump(void) { return ++hdr_calls; }
#endif
