#include "Logger.h"
#include "hooks.h"
#include "sdk_keys.h"
#include "utils.h"

#include <chrono>
#include <thread>
#include <windows.h>

namespace {

bool initConsole() {
    if (!AllocConsole()) {
        Logger::instance().error("Console allocation failed");
        return false;
    }

    SetConsoleTitleA("Debug Console");

    FILE *fp = nullptr;
    if (freopen_s(&fp, "CONIN$", "r", stdin) != 0) {
        Logger::instance().error("Failed to redirect stdin");
        return false;
    }
    if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0) {
        Logger::instance().error("Failed to redirect stdout");
        return false;
    }
    if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0) {
        Logger::instance().error("Failed to redirect stderr");
        return false;
    }

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        Logger::instance().error("Invalid console handle");
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hConsole, &dwMode)) {
        Logger::instance().error("Failed to get console mode");
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hConsole, dwMode)) {
        Logger::instance().warn("Failed to set console mode");
    }

    return true;
}

struct EnumData {
    DWORD processId;
    HWND consoleWindow;
    std::string windowTitle;
};

BOOL IsMainWindow(HWND hwnd) { return (GetWindow(hwnd, GW_OWNER) == NULL) && IsWindowVisible(hwnd); }

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumData *data = reinterpret_cast<EnumData *>(lParam);
    if (!data)
        return TRUE;

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (processId != data->processId)
        return TRUE;
    if (hwnd == data->consoleWindow)
        return TRUE;
    if (!IsMainWindow(hwnd))
        return TRUE;

    char windowTitle[512] = {0};
    int length = GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));

    if (length > 0) {
        data->windowTitle = std::string(windowTitle, length);
        Logger::instance().info("[DX12] Found main window title: %s", data->windowTitle.c_str());
        return FALSE;
    }

    return TRUE;
}

std::string getMainWindowTitle() {
    EnumData data{.processId = GetCurrentProcessId(), .consoleWindow = GetConsoleWindow(), .windowTitle = ""};
    const BOOL enumResult = EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

    if (!enumResult && !data.windowTitle.empty()) {
        return data.windowTitle;
    }

    if (!enumResult) {
        Logger::instance().warn("[DX12] Main window not found during enumeration");
        return "Unknown";
    }

    if (data.windowTitle.empty()) {
        Logger::instance().warn("[DX12] Main window title not found");
        return "Unknown";
    }

    return data.windowTitle;
}

bool initializeHook(DX12Hook &dxInstance) {
    Logger::instance().info("[DX12] Attempting to hook...");
    if (!dxInstance.hook()) {
        Logger::instance().error("[DX12] Failed to initialize hook");
        dxInstance.unHook();
        return false;
    }
    return true;
}

void runHookLoop(DX12Hook &dxInstance) {
    Logger::instance().info("[DX12] Hook successful. Press END to unload");

    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            Logger::instance().info("[DX12] Unhook requested");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Logger::instance().info("[DX12] Unhooking...");
    dxInstance.unHook();
}

void mainThread() {
    try {
        if (!initConsole())
            return;

        myProcess.windowTitle = getMainWindowTitle();

        const auto antiTamper = MAKE_RVA(0x39BDD70); // C6 05 ? ? ? ? ? C7 44 24 ? ? ? ? ? 41 8B C0
        if (antiTamper) {
            auto decryptValue = EncryptedField<bool>(antiTamper);

            Logger::instance().info("Decrypted value: %i", (bool)decryptValue);
        }

        auto &dxInstance = DX12Hook::getInstance();
        if (!initializeHook(dxInstance)) {
            return;
        }

        runHookLoop(dxInstance);

    } catch (const std::exception &ex) {
        Logger::instance().critical("Fatal error: {}", ex.what());
        DX12Hook::getInstance().unHook();
    }
}

DWORD WINAPI threadEntry(LPVOID) {
    mainThread();
    return 0;
}

void startMainThread() {
    if (HANDLE hThread = CreateThread(nullptr, 0, threadEntry, nullptr, 0, nullptr)) {
        CloseHandle(hThread);
    } else {
        Logger::instance().error("Failed to create main thread");
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        myProcess.hModule = hModule;
        DisableThreadLibraryCalls(hModule);

        startMainThread();
        break;
    }
    case DLL_PROCESS_DETACH:
        Logger::instance().info("[DX12] DLL unloaded");
        break;
    }
    return TRUE;
}