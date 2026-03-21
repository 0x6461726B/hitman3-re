#pragma once
#include "Logger.h"
#include "Minhook/MinHook.h"

class HookManager {
  public:
    HookManager();
    ~HookManager();

    bool create(void *target, void *detour, void **original);
    bool enableAll();
    bool disableAll();
    bool remove(void *target);
    void uninitialize();

  private:
    bool initialized;
};
