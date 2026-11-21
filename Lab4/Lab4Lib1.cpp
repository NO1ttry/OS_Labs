#define BUILDING_DLL
#include "Lab4Contracts.h"

int gcd(int a, int b) {
    while (b != 0) { int t = a % b; a = b; b = t; }
    return a;
}

float e(int x) {
    if (x <= 0) return 0.0f;
    double base = 1.0 + 1.0 / x, r = 1.0;
    int n = x;
    while (n > 0) {
        if (n & 1) r *= base;
        base *= base;
        n >>= 1;
    }
    return (float)r;
}
extern "C" int __stdcall DllEntry(void*, unsigned long, void*) { return 1; }