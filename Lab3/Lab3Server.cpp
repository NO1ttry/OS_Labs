#include <windows.h>
#include <string>
#include <vector>
#include "Lab3Shared.h"

static HANDLE hOut, hIn;

static std::string ToStr(unsigned long long u) {
    std::string s;
    if (u == 0) s = "0";
    while (u) { s.insert(s.begin(), char('0' + u % 10)); u /= 10; }
    return s;
}
static void Put(HANDLE h, const std::string& s) {
    DWORD w, off = 0;
    while (off < s.size()) {
        if (!WriteFile(h, s.data() + off, (DWORD)(s.size() - off), &w, NULL)) return;
        off += w;
    }
}
static void Fail(const std::string& what) {
    Put(GetStdHandle(STD_ERROR_HANDLE),
        what + " failed, GetLastError=" + ToStr(GetLastError()) + "\r\n");
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

    std::string base = "Local\\Lab_" + ToStr(GetCurrentProcessId()) + "_" + ToStr(GetTickCount());
    std::string shmName = base + "_shm";
    std::string reqName = base + "_req";
    std::string respName = base + "_resp";

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     0, sizeof(SharedData), shmName.c_str());
    if (hMap == NULL) Fail("CreateFileMapping");
    if (GetLastError() == ERROR_ALREADY_EXISTS) Fail("CreateFileMapping (name already exists)");

    SharedData* shm = (SharedData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (shm == NULL) Fail("MapViewOfFile");

    HANDLE semReq = CreateSemaphoreA(NULL, 0, 1, reqName.c_str());
    if (semReq == NULL) Fail("CreateSemaphore(req)");
    HANDLE semResp = CreateSemaphoreA(NULL, 0, 1, respName.c_str());
    if (semResp == NULL) Fail("CreateSemaphore(resp)");

    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) Fail("GetModuleFileName");
    std::string dir(path, n);
    size_t sl = dir.find_last_of("\\/");
    dir = (sl == std::string::npos) ? "" : dir.substr(0, sl + 1);
    std::string exe = dir + "Lab3Client.exe";
    std::string cmd = "\"" + exe + "\" " + base + " \"" + fileName + "\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(exe.c_str(), &cmdBuf[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        Fail("CreateProcess");

    Put(hOut, "Enter lines like \"1 2 3\". Empty line or Ctrl+Z to exit.\r\n");
    std::string line;
    HANDLE waitSet[2] = { semResp, pi.hProcess };
    for (;;) {
        Put(hOut, "> ");
        if (!ReadLine(hIn, inRest, line) || line.empty()) break;
        if (line.size() >= CMD_MAX) {
            Put(hOut, "Line is too long (max " + ToStr(CMD_MAX - 1) + ")\r\n");
            continue;
        }
        CopyMemory(shm->cmd, line.data(), line.size());
        shm->cmdLen = (LONG)line.size();
        if (!ReleaseSemaphore(semReq, 1, NULL)) Fail("ReleaseSemaphore(req)");

        DWORD r = WaitForMultipleObjects(2, waitSet, FALSE, INFINITE);
        if (r == WAIT_OBJECT_0) {
            std::string reply(shm->reply, shm->replyLen);
            Put(hOut, std::string("Client: ") + (shm->ok ? "OK " : "ERR ") + reply + "\r\n");
        } else if (r == WAIT_OBJECT_0 + 1) {
            Put(hOut, "Client process terminated unexpectedly\r\n");
            break;
        } else Fail("WaitForMultipleObjects");
    }

    // команда завершения
    shm->cmdLen = -1;
    ReleaseSemaphore(semReq, 1, NULL);
    if (WaitForSingleObject(pi.hProcess, 5000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    UnmapViewOfFile(shm);
    CloseHandle(hMap);
    CloseHandle(semReq);
    CloseHandle(semResp);
    return (int)code;
}