#define BUILDING_DLL
#include "Lab4Contracts.h"

int gcd(int a, int b) {
    if (a <= 0 || b <= 0) return 0;
    int m = a < b ? a : b;
    for (int d = m; d >= 1; --d)
        if (a % d == 0 && b % d == 0) return d;
    return 1;
}

float e(int x) {
    if (x < 0) return 0.0f;
    double sum = 1.0, term = 1.0;
    for (int n = 1; n <= x; ++n) { term /= n; sum += term; }
    return (float)sum;
}
extern "C" int __stdcall DllEntry(void*, unsigned long, void*) { return 1; }