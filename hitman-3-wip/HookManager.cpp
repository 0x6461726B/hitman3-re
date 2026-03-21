#include "HookManager.h"

HookManager::HookManager() : initialized(false) {
    if (MH_Initialize() == MH_OK) {
        initialized = true;
    } else {
        Logger::instance().error("[MH] Couldn't initialize minhook");
    }
}

HookManager::~HookManager() {
    if (initialized) {
        uninitialize();
    }
}

bool HookManager::create(void *target, void *detour, void **original) {
    if (!initialized)
        return false;
    if (MH_CreateHook(target, detour, original) != MH_OK) {
        Logger::instance().error("[MH] Failed to create hook");
        return false;
    }
    return true;
}

bool HookManager::enableAll() { return initialized && MH_EnableHook(MH_ALL_HOOKS) == MH_OK; }

bool HookManager::disableAll() { return initialized && MH_DisableHook(MH_ALL_HOOKS) == MH_OK; }

bool HookManager::remove(void *target) { return initialized && MH_RemoveHook(target) == MH_OK; }

void HookManager::uninitialize() {
    if (initialized) {
        MH_Uninitialize();
        initialized = false;
    }
}
