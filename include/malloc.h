// malloc.h: memory -- of the heap (malloc, free) and of the function's own
// stack (_alloca: freed when the function returns, nothing to free).
#pragma once
#include <stdlib.h>

#define alloca _alloca                  // a word of the compiler: _alloca(n)
#define _malloca(n) _alloca(n)          // here always in the stack
#define _freea(p) ((void)(p))
#define _ALLOCA_S_THRESHOLD 1024

void *_aligned_malloc(size_t size, size_t alignment);
void _aligned_free(void *p);
size_t _msize(void *p);
void *_expand(void *p, size_t size);
