#pragma once
#include <string>
#include <wtypes.h>

struct Process {
    HMODULE hModule = NULL;
    DWORD baseAddress = 0;
    HWND targetHwnd = NULL;
    WNDPROC targetWndProc = NULL;
    std::string windowTitle;
};