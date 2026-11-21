#include <windows.h>
#include <string>
#include <vector>

static HANDLE hOut, hIn;

static void Put(HANDLE h, const std::string& s) {
    DWORD w, off = 0;
    while (off < s.size()) {
        if (!WriteFile(h, s.data() + off, (DWORD)(s.size() - off), &w, NULL)) return;
        off += w;
    }
}
static void Fail(const std::string& what) {
    DWORD e = GetLastError();
    std::string n; if (!e) n = "0";
    while (e) { n.insert(n.begin(), char('0' + e % 10)); e /= 10; }
    Put(GetStdHandle(STD_ERROR_HANDLE), what + " failed, GetLastError=" + n + "\r\n");
    ExitProcess(1);
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

int main() {
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hIn == INVALID_HANDLE_VALUE) Fail("GetStdHandle");

    std::string inRest, fileName;
    Put(hOut, "Enter file name: ");
    if (!ReadLine(hIn, inRest, fileName) || fileName.empty()) {
        Put(hOut, "File name is required\r\n"); return 1;
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE p1r, p1w, p2r, p2w;
    if (!CreatePipe(&p1r, &p1w, &sa, 0)) Fail("CreatePipe1");
    if (!CreatePipe(&p2r, &p2w, &sa, 0)) Fail("CreatePipe2");
    if (!SetHandleInformation(p1w, HANDLE_FLAG_INHERIT, 0)) Fail("SetHandleInformation1");
    if (!SetHandleInformation(p2r, HANDLE_FLAG_INHERIT, 0)) Fail("SetHandleInformation2");

    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) Fail("GetModuleFileName");
    std::string dir(path, n);
    size_t sl = dir.find_last_of("\\/");
    dir = (sl == std::string::npos) ? "" : dir.substr(0, sl + 1);
    std::string exe = dir + "Lab1child.exe";
    std::string cmd = "\"" + exe + "\" \"" + fileName + "\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    STARTUPINFOA si; ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = p1r;
    si.hStdOutput = p2w;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(exe.c_str(), &cmdBuf[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        Fail("CreateProcess");

    CloseHandle(p1r);
    CloseHandle(p2w);

    Put(hOut, "Enter lines like \"1 2 3\". Empty line or Ctrl+Z to exit.\r\n");
    std::string pipeRest, line, reply;
    for (;;) {
        Put(hOut, "> ");
        if (!ReadLine(hIn, inRest, line) || line.empty()) break;
        DWORD w;
        std::string msg = line + "\n";
        if (!WriteFile(p1w, msg.data(), (DWORD)msg.size(), &w, NULL)) {
            Put(hOut, "Child is not accepting data (pipe closed)\r\n");
            break;
        }
        if (!ReadLine(p2r, pipeRest, reply)) {
            Put(hOut, "Child closed the reply pipe\r\n");
            break;
        }
        Put(hOut, "Child: " + reply + "\r\n");
    }

    CloseHandle(p1w);
    if (WaitForSingleObject(pi.hProcess, 5000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(p2r);
    return (int)code;
}