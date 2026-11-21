#pragma once
#include <windows.h>

#define CMD_MAX   65536
#define REPLY_MAX 256

struct SharedData {
    LONG cmdLen;
    char cmd[CMD_MAX];
    LONG ok;
    LONG replyLen;
    char reply[REPLY_MAX];
};