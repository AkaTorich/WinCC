// A leaf DLL: only exports, no imports — the Windows loader can load it.
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int square(int a, int b) { (void)b; return a * a; }
int fib(int n, int b) { (void)b; return n < 2 ? n : fib(n - 1, 0) + fib(n - 2, 0); }
