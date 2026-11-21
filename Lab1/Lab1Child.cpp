#include <windows.h>
#include <string>
#include <vector>

static std::string ToStr(long long v) {
    bool neg = v < 0;
    unsigned long long u = neg ? 0ULL - (unsigned long long)v : (unsigned long long)v;
    std::string s;
    if (u == 0) s = "0";
    while (u) { s.insert(s.begin(), char('0' + u % 10)); u /= 10; }
    return neg ? "-" + s : s;
}
static bool Put(HANDLE h, const std::string& s) {
    DWORD w, off = 0;
    while (off < s.size()) {
        if (!WriteFile(h, s.data() + off, (DWORD)(s.size() - off), &w, NULL)) return false;
        off += w;
    }
    return true;
}
static bool ReadLine(HANDLE h, std::string& rest, std::string& line) {
    for (;;) {
        size_t p = rest.find('\n');
        if (p != std::string::npos) {
            line = rest.substr(0, p);
            rest.erase(0, p + 1);
            while (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }
        char buf[512]; DWORD r = 0;
        if (!ReadFile(h, buf, sizeof(buf), &r, NULL) || r == 0) {
            if (rest.empty()) return false;
            line = rest; rest.clear(); return true;
        }
        rest.append(buf, r);
    }
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

int main() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) return 2;

    std::string cl = GetCommandLineA();
    std::string fname;
    size_t i = 0;
    if (!cl.empty() && cl[0] == '"') {
        i = cl.find('"', 1);
        i = (i == std::string::npos) ? cl.size() : i + 1;
    } else {
        i = cl.find(' ');
        if (i == std::string::npos) i = cl.size();
    }
    while (i < cl.size() && cl[i] == ' ') i++;
    if (i < cl.size() && cl[i] == '"') {
        size_t e = cl.find('"', i + 1);
        fname = cl.substr(i + 1, e == std::string::npos ? std::string::npos : e - i - 1);
    } else fname = cl.substr(i);
    if (fname.empty()) { Put(hOut, "ERR no file name\n"); return 3; }

    HANDLE hFile = CreateFileA(fname.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Put(hOut, "ERR cannot open file, code " + ToStr(GetLastError()) + "\n");
        return 4;
    }
    SetFilePointer(hFile, 0, NULL, FILE_END);

    std::string rest, line, err;
    while (ReadLine(hIn, rest, line)) {
        std::vector<int> nums;
        err.clear();
        if (!Parse(line, nums, err)) {
            if (!Put(hOut, "ERR " + err + "\n")) break;
            continue;
        }
        long long sum = 0;
        for (size_t k = 0; k < nums.size(); k++) sum += nums[k];
        std::string res = ToStr(sum);
        if (!Put(hFile, res + "\r\n")) {
            Put(hOut, "ERR write to file, code " + ToStr(GetLastError()) + "\n");
            continue;
        }
        if (!Put(hOut, "OK " + res + "\n")) break;
    }
    CloseHandle(hFile);
    return 0;
}