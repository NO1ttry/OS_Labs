#include <windows.h>
#include <string>
#include <vector>
#include "Lab3Shared.h"

static std::string ToStr(long long v) {
    bool neg = v < 0;
    unsigned long long u = neg ? 0ULL - (unsigned long long)v : (unsigned long long)v;
    std::string s;
    if (u == 0) s = "0";
    while (u) { s.insert(s.begin(), char('0' + u % 10)); u /= 10; }
    return neg ? "-" + s : s;
}
static void Err(const std::string& s) {
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD w;
    if (h != INVALID_HANDLE_VALUE) WriteFile(h, s.data(), (DWORD)s.size(), &w, NULL);
}
static bool Put(HANDLE h, const std::string& s) {
    DWORD w, off = 0;
    while (off < s.size()) {
        if (!WriteFile(h, s.data() + off, (DWORD)(s.size() - off), &w, NULL)) return false;
        off += w;
    }
    return true;
}

static bool NextArg(const std::string& cl, size_t& i, std::string& out) {
    out.clear();
    while (i < cl.size() && cl[i] == ' ') i++;
    if (i >= cl.size()) return false;
    if (cl[i] == '"') {
        i++;
        while (i < cl.size() && cl[i] != '"') out += cl[i++];
        if (i < cl.size()) i++;
    } else {
        while (i < cl.size() && cl[i] != ' ') out += cl[i++];
    }
    return true;
}
static bool Parse(const std::string& line, std::vector<int>& nums, std::string& err) {
    size_t i = 0, n = line.size();
    while (i < n) {
        while (i < n && (line[i] == ' ' || line[i] == '\t')) i++;
        if (i >= n) break;
        bool neg = false;
        if (line[i] == '-' || line[i] == '+') { neg = line[i] == '-'; i++; }
        if (i >= n || line[i] < '0' || line[i] > '9') { err = "invalid token"; return false; }
        long long v = 0;
        while (i < n && line[i] >= '0' && line[i] <= '9') {
            v = v * 10 + (line[i] - '0');
            if (v > 2147483648LL) { err = "number out of int range"; return false; }
            i++;
        }
        if (i < n && line[i] != ' ' && line[i] != '\t') { err = "invalid token"; return false; }
        if (neg) v = -v;
        if (v > 2147483647LL || v < -2147483648LL) { err = "number out of int range"; return false; }
        nums.push_back((int)v);
    }
    if (nums.empty()) { err = "no numbers"; return false; }
    return true;
}
static void SetReply(SharedData* shm, bool ok, const std::string& text) {
    size_t len = text.size() < REPLY_MAX ? text.size() : REPLY_MAX;
    CopyMemory(shm->reply, text.data(), len);
    shm->replyLen = (LONG)len;
    shm->ok = ok ? 1 : 0;
}

int main() {
    std::string cl = GetCommandLineA(), prog, base, fname;
    size_t pos = 0;
    NextArg(cl, pos, prog);
    if (!NextArg(cl, pos, base) || !NextArg(cl, pos, fname)) {
        Err("usage: client <base_name> <file>\r\n"); return 3;
    }

    HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, (base + "_shm").c_str());
    if (hMap == NULL) { Err("OpenFileMapping failed, code " + ToStr(GetLastError()) + "\r\n"); return 4; }
    SharedData* shm = (SharedData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (shm == NULL) { Err("MapViewOfFile failed, code " + ToStr(GetLastError()) + "\r\n"); return 5; }
    HANDLE semReq = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, (base + "_req").c_str());
    HANDLE semResp = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, (base + "_resp").c_str());
    if (semReq == NULL || semResp == NULL) {
        Err("OpenSemaphore failed, code " + ToStr(GetLastError()) + "\r\n"); return 6;
    }

    HANDLE hFile = CreateFileA(fname.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    bool fileOk = hFile != INVALID_HANDLE_VALUE;
    std::string fileErr;
    if (fileOk) SetFilePointer(hFile, 0, NULL, FILE_END);
    else fileErr = "cannot open file, code " + ToStr(GetLastError());

    int result = 0;
    for (;;) {
        DWORD r = WaitForSingleObject(semReq, INFINITE);
        if (r != WAIT_OBJECT_0) { Err("WaitForSingleObject failed\r\n"); result = 7; break; }
        if (shm->cmdLen < 0) break;

        std::string line(shm->cmd, shm->cmdLen);
        std::vector<int> nums;
        std::string err;
        if (!fileOk) {
            SetReply(shm, false, fileErr);
        } else if (!Parse(line, nums, err)) {
            SetReply(shm, false, err);
        } else {
            long long sum = 0;
            for (size_t k = 0; k < nums.size(); k++) sum += nums[k];
            std::string res = ToStr(sum);
            if (Put(hFile, res + "\r\n")) SetReply(shm, true, res);
            else SetReply(shm, false, "write to file failed, code " + ToStr(GetLastError()));
        }
        if (!ReleaseSemaphore(semResp, 1, NULL)) { result = 8; break; }
    }

    if (fileOk) CloseHandle(hFile);
    UnmapViewOfFile(shm);
    CloseHandle(hMap);
    CloseHandle(semReq);
    CloseHandle(semResp);
    return result;
}