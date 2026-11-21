#include <windows.h>

static HANDLE g_out;
static volatile LONG g_active = 1;
static volatile LONG g_peak = 1;
static LONG g_limit = 1;
static const int THRESH = 4096;

static void Print(const char* s) {
    DWORD w; int n = 0; while (s[n]) n++;
    WriteFile(g_out, s, n, &w, 0);
}

static void FmtMs(char* buf, double ms) {
    LONGLONG us = (LONGLONG)(ms * 1000.0 + 0.5);
    wsprintfA(buf, "%d.%03d", (int)(us / 1000), (int)(us % 1000));
}
static void Fmt3(char* buf, double v) {
    LONGLONG m = (LONGLONG)(v * 1000.0 + 0.5);
    wsprintfA(buf, "%d.%03d", (int)(m / 1000), (int)(m % 1000));
}

static void Merge(int* a, int* t, int lo, int mid, int hi) {
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) t[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i < mid) t[k++] = a[i++];
    while (j < hi)  t[k++] = a[j++];
    for (k = lo; k < hi; k++) a[k] = t[k];
}

static void SeqSort(int* a, int* t, int lo, int hi) {
    if (hi - lo < 2) return;
    int mid = lo + (hi - lo) / 2;
    SeqSort(a, t, lo, mid);
    SeqSort(a, t, mid, hi);
    Merge(a, t, lo, mid, hi);
}

struct Task { int* a; int* t; int lo, hi; };

static void ParSort(int* a, int* t, int lo, int hi);

static DWORD WINAPI Worker(LPVOID p) {
    Task* k = (Task*)p;
    ParSort(k->a, k->t, k->lo, k->hi);
    InterlockedDecrement(&g_active);
    return 0;
}

static void ParSort(int* a, int* t, int lo, int hi) {
    if (hi - lo < 2) return;
    if (hi - lo < THRESH) { SeqSort(a, t, lo, hi); return; }
    int mid = lo + (hi - lo) / 2;

    HANDLE h = 0;
    Task task = { a, t, lo, mid };
    LONG cur = InterlockedIncrement(&g_active);
    if (cur <= g_limit) {
        LONG pk;
        while (cur > (pk = g_peak))
            InterlockedCompareExchange(&g_peak, cur, pk);
        h = CreateThread(0, 256 * 1024, Worker, &task, STACK_SIZE_PARAM_IS_A_RESERVATION, 0);
        if (!h) InterlockedDecrement(&g_active);
    } else {
        InterlockedDecrement(&g_active);
    }

    if (h) {
        ParSort(a, t, mid, hi);
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
    } else {
        ParSort(a, t, lo, mid);
        ParSort(a, t, mid, hi);
    }
    Merge(a, t, lo, mid, hi);
}

static double Now() {
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

static void Fill(int* a, int n) {
    unsigned s = 2463534242u;
    for (int i = 0; i < n; i++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        a[i] = (int)(s & 0x7FFFFFFF);
    }
}
static bool IsSorted(const int* a, int n) {
    for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return false;
    return true;
}
static int ToInt(const wchar_t* s) {
    int v = 0; while (*s >= L'0' && *s <= L'9') v = v * 10 + (*s++ - L'0'); return v;
}
static bool Eq(const wchar_t* a, const wchar_t* b) {
    while (*a && *a == *b) { a++; b++; } return *a == *b;
}

static double Run(int* src, int* a, int* t, int n, int threads, bool* ok) {
    for (int i = 0; i < n; i++) a[i] = src[i];
    g_limit = threads; g_active = 1; g_peak = 1;
    double t0 = Now();
    if (threads == 0) SeqSort(a, t, 0, n); else ParSort(a, t, 0, n);
    double t1 = Now();
    *ok = IsSorted(a, n);
    return t1 - t0;
}

int main() {
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int argc; wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    int n = 20000000, threads = 4;
    bool bench = false, pause = false;
    for (int i = 1; i < argc; i++) {
        if (Eq(argv[i], L"-n") && i + 1 < argc) n = ToInt(argv[++i]);
        else if (Eq(argv[i], L"-t") && i + 1 < argc) threads = ToInt(argv[++i]);
        else if (Eq(argv[i], L"-bench")) bench = true;
        else if (Eq(argv[i], L"-pause")) pause = true;
    }
    if (threads < 1) threads = 1;

    SYSTEM_INFO si; GetSystemInfo(&si);
    char b[256];
    wsprintfA(b, "PID=%u, logical CPUs=%u, N=%d\r\n", GetCurrentProcessId(), si.dwNumberOfProcessors, n);
    Print(b);

    HANDLE heap = GetProcessHeap();
    int* src = (int*)HeapAlloc(heap, 0, (SIZE_T)n * 4);
    int* a   = (int*)HeapAlloc(heap, 0, (SIZE_T)n * 4);
    int* t   = (int*)HeapAlloc(heap, 0, (SIZE_T)n * 4);
    if (!src || !a || !t) { Print("alloc failed\r\n"); return 1; }
    Fill(src, n);

    bool ok;
    double tseq = Run(src, a, t, n, 0, &ok);
    char ms[32]; FmtMs(ms, tseq);
    wsprintfA(b, "Sequential: %s ms, sorted=%d\r\n", ms, (int)ok); Print(b);

    int list[16]; int cnt = 0;
    if (bench) {
        int v[] = { 2, 4, 6, 8, 12, 16, 128, 1024 };
        for (int i = 0; i < 8; i++) list[cnt++] = v[i];
    } else list[cnt++] = threads;

    if (pause) {
        Print("Pause 15 sec\r\n");
        Sleep(15000);
    }

    Print("Threads | Time(ms) | Speedup | Efficiency | PeakThreads | OK\r\n");
    for (int i = 0; i < cnt; i++) {
        int p = list[i];
        double tp = Run(src, a, t, n, p, &ok);
        char s1[32], s2[32], s3[32];
        FmtMs(s1, tp); Fmt3(s2, tseq / tp); Fmt3(s3, tseq / tp / p);
        wsprintfA(b, "%7d | %8s | %7s | %10s | %11d | %d\r\n", p, s1, s2, s3, (int)g_peak, (int)ok);
        Print(b);
    }
    if (pause) { Print("Done. Pause 15 sec\r\n"); Sleep(15000); }

    HeapFree(heap, 0, src); HeapFree(heap, 0, a); HeapFree(heap, 0, t);
    return 0;
}