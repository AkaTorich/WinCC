// Своя DLL на WinCC: функции с __declspec(dllexport) видны другим программам.
// Собрать:  wcc --dll examples\dll\mathlib.c
#include <windows.h>
#include <stdio.h>

static int g_calls;                 // свои данные DLL: у каждого процесса — свои

__declspec(dllexport) int add(int a, int b)
{
    g_calls++;
    return a + b;
}

__declspec(dllexport) double average(const double *v, int n)
{
    g_calls++;
    double s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return n ? s / n : 0;
}

__declspec(dllexport) void greet(const char *name)
{
    g_calls++;
    printf("Привет, %s! (из mathlib.dll)\n", name);
}

__declspec(dllexport) int calls(void)
{
    return g_calls;
}

// Необязательно: Windows зовёт её при загрузке и выгрузке DLL.
BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
        g_calls = 0;
    return TRUE;
}
