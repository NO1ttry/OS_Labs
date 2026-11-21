#include <windows.h>

static HANDLE hIn, hOut;

static void writeStr(const char* s) {
    DWORD n = 0, w;
    while (s[n]) ++n;
    WriteFile(hOut, s, n, &w, nullptr);
}

static void writeInt(long long v) {
    char buf[24]; int i = 23; buf[i] = 0;
    bool neg = v < 0; if (neg) v = -v;
    do { buf[--i] = '0' + (char)(v % 10); v /= 10; } while (v);
    if (neg) buf[--i] = '-';
    writeStr(buf + i);
}

static void writeFloat(float f) {
    double d = f;
    if (d < 0) { writeStr("-"); d = -d; }
    long long ip = (long long)d;
    long long fp = (long long)((d - (double)ip) * 1000000.0 + 0.5);
    if (fp >= 1000000) { ++ip; fp -= 1000000; }
    writeInt(ip); writeStr(".");
    char b[7]; for (int i = 5; i >= 0; --i) { b[i] = '0' + (char)(fp % 10); fp /= 10; }
    b[6] = 0; writeStr(b);
}

static int readLine(char* buf, int cap) {
    int n = 0; char c; DWORD r;
    for (;;) {
        if (!ReadFile(hIn, &c, 1, &r, nullptr) || r == 0) return n ? n : -1;
        if (c == '\n') break;
        if (c != '\r' && n < cap - 1) buf[n++] = c;
    }
    buf[n] = 0; return n;
}

static bool nextInt(const char*& p, int& out) {
    while (*p == ' ' || *p == '\t') ++p;
    if (*p < '0' || *p > '9') return false;
    long long v = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); if (v > 2147483647LL) return false; ++p; }
    out = (int)v; return true;
}

typedef int   (*GcdFn)(int, int);
typedef float (*EFn)(int);

static const char* LIBS[2] = { "Lab4Lib1.dll", "Lab4Lib2.dll" };
static HMODULE h = nullptr;
static GcdFn pGcd = nullptr;
static EFn pE = nullptr;

static bool load(int idx) {
    HMODULE nh = LoadLibraryA(LIBS[idx]);
    if (!nh) { writeStr("LoadLibrary error: "); writeInt(GetLastError()); writeStr("\r\n"); return false; }
    GcdFn g = (GcdFn)GetProcAddress(nh, "gcd");
    EFn f = (EFn)GetProcAddress(nh, "e");
    if (!g || !f) { writeStr("GetProcAddress error\r\n"); FreeLibrary(nh); return false; }
    if (h) FreeLibrary(h);
    h = nh; pGcd = g; pE = f;
    writeStr("Loaded "); writeStr(LIBS[idx]); writeStr("\r\n");
    return true;
}

extern "C" __attribute__((force_align_arg_pointer)) void mainCRTStartup() {
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    int cur = 0;
    if (!load(cur)) ExitProcess(1);
    char line[256];
    writeStr("0 - switch | 1 a b | 2 x | q - exit\r\n");
    for (;;) {
        int n = readLine(line, sizeof line);
        if (n < 0 || line[0] == 'q') break;
        const char* p = line; int cmd;
        if (!nextInt(p, cmd)) continue;
        if (cmd == 0) { if (load(1 - cur)) cur = 1 - cur; }
        else if (cmd == 1) {
            int a, b;
            if (nextInt(p, a) && nextInt(p, b) && a > 0 && b > 0) { writeInt(pGcd(a, b)); writeStr("\r\n"); }
            else writeStr("Bad args\r\n");
        } else if (cmd == 2) {
            int x;
            if (nextInt(p, x)) { writeFloat(pE(x)); writeStr("\r\n"); }
            else writeStr("Bad args\r\n");
        } else writeStr("Unknown command\r\n");
    }
    if (h) FreeLibrary(h);
    ExitProcess(0);
}