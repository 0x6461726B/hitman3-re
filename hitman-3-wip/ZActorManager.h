#pragma once
#include <wtypes.h>
class ZActorManager {
  public:
    DWORD getSize() { return *reinterpret_cast<int *>(this + 0x65C8); }

    uintptr_t getActor(int index) {
        return *reinterpret_cast<uintptr_t *>(this + 0x4688 + (index * 0x10) + 0x8); // ZHitman5
    }

    uintptr_t getActorList() { return *reinterpret_cast<uintptr_t *>(this + 0x4688); }

    uintptr_t getActorListEnd() { return *reinterpret_cast<uintptr_t *>(this + 0x4688 + (0x10 * getSize())); }
};