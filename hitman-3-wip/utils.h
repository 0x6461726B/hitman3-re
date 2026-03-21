#pragma once
#include "Logger.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

// --- Macros ---
#define MAKE_RVA(address) (reinterpret_cast<uintptr_t>(GetModuleHandle(NULL)) + (address))


// --- Function Invoker ---
template <typename T, typename... Args> T callFunction(uintptr_t funcAddress, Args... args) {
    if (!funcAddress)
        return T{};

    using FuncType = T(__fastcall *)(Args...);
    return reinterpret_cast<FuncType>(funcAddress)(std::forward<Args>(args)...);
}

constexpr uint32_t FNV_OFFSET_BASIS = 0x811C9DC5U;
constexpr uint32_t FNV_PRIME = 16777619U;

inline uint32_t computeFnv1a(const void *data, size_t size) {
    uint32_t hash = FNV_OFFSET_BASIS;
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * FNV_PRIME;
    }
    return hash;
}

struct Entry {
    int64_t key;
    int64_t offset;
};

inline uintptr_t getComponent(uintptr_t baseAddress, int64_t targetKey) {
    if (baseAddress == 0)
        return 0;

    uintptr_t keyAddr = MAKE_RVA(targetKey);

    uintptr_t base = *reinterpret_cast<uintptr_t *>(baseAddress);
    if (base == 0)
        return 0;

    uintptr_t *lookupTablePtr = reinterpret_cast<uintptr_t *>(base + 0x20);
    uintptr_t lookupTableValue = *lookupTablePtr;
    if (lookupTableValue == 0)
        return 0;

    int64_t *lookupTable = reinterpret_cast<int64_t *>(lookupTableValue);
    if (!lookupTable)
        return 0;

    int64_t flags = lookupTable[2];
    Entry *entries = nullptr;
    uint64_t entryCount = 0;

    constexpr int64_t SBO_FLAG = 0x4000000000000000LL;
    if (flags & SBO_FLAG) {
        // Small buffer — entries stored inline in the lookup table struct
        entries = reinterpret_cast<Entry *>(lookupTable);
        entryCount = static_cast<uint8_t>(flags & 0xFF);
    } else {
        // Large buffer — entries stored in a separate allocation
        int64_t entriesAddress = lookupTable[0];
        int64_t totalSize = lookupTable[1];

        if (entriesAddress == 0 || totalSize == 0)
            return 0;

        entries = reinterpret_cast<Entry *>(static_cast<uintptr_t>(entriesAddress));
        entryCount = static_cast<uint64_t>(totalSize) / sizeof(Entry);
    }

    if (!entries || entryCount == 0)
        return 0;

    for (uint64_t i = 0; i < entryCount; ++i) {
        if (entries[i].key == keyAddr) {
            return baseAddress + entries[i].offset;
        }
    }

    return 0;
}

// --- Encrypted Field Wrapper ---
template <typename T> class EncryptedField {
    static_assert(sizeof(T) <= 8, "EncryptedField type size cannot exceed 8 bytes.");

  private:
    struct EncryptionEntry {
        union {
            uint8_t bytes[16];
            struct {
                uint64_t value;
                uintptr_t hashPtr;
            } view;
        };

        EncryptionEntry(uintptr_t address = 0) {
            if (address)
                std::memcpy(this, (void *)address, sizeof(EncryptionEntry));
            else
                std::memset(this, 0, sizeof(EncryptionEntry));
        }

        bool decrypt() {
            // 1. Decrypt Pointer
            for (int i = 8; i < 15; i++)
                bytes[i] ^= (bytes[i + 1] + bytes[0]);
            bytes[15] ^= bytes[0];

            if (!view.hashPtr)
                return false;

            // 2. Get Key & Expected Hash
            uint8_t *ptrToHash = reinterpret_cast<uint8_t *>(view.hashPtr);
            uint8_t key = *ptrToHash;
            uint32_t expectedHash = *reinterpret_cast<uint32_t *>(ptrToHash);

            // 3. Decrypt Value
            for (int i = 0; i < 7; i++)
                bytes[i] ^= (key + bytes[i + 1]);
            bytes[7] ^= key;

            return computeFnv1a(&view.value, 8) == expectedHash;
        }

        void encrypt(T newValue) {
            // 1. Set new raw value
            view.value = 0; // Clear padding
            std::memcpy(&view.value, &newValue, sizeof(T));

            // 2. Update external hash
            uint32_t newHash = computeFnv1a(&view.value, 8);
            if (view.hashPtr)
                *reinterpret_cast<uint32_t *>(view.hashPtr) = newHash;

            // 3. Encrypt Value (Reverse Order)
            uint8_t key = static_cast<uint8_t>(newHash);
            bytes[7] ^= key;
            for (int i = 6; i >= 0; i--)
                bytes[i] ^= (key + bytes[i + 1]);

            // 4. Encrypt Pointer (Reverse Order)
            uint8_t derivedKey = bytes[0];
            bytes[15] ^= bytes[0];
            for (int i = 14; i >= 8; i--)
                bytes[i] ^= (bytes[i + 1] + derivedKey);
        }
    };

    uintptr_t m_address;

  public:
    EncryptedField(uintptr_t address = 0) : m_address(address) {}

    void bind(uintptr_t address) { m_address = address; }

    T get() const {
        if (!m_address)
            return T{};

        EncryptionEntry entry(m_address);
        if (entry.decrypt()) {
            return *reinterpret_cast<T *>(&entry.view.value);
        }
        return T{};
    }

    void set(T newValue) {
        if (!m_address)
            return;

        EncryptionEntry entry(m_address);

        if (entry.decrypt()) {
            entry.encrypt(newValue);
            std::memcpy((void *)m_address, &entry, sizeof(EncryptionEntry));
        }
    }

    operator T() const { return get(); }

    EncryptedField<T> &operator=(const T &newValue) {
        set(newValue);
        return *this;
    }
};

// --- String Wrapper ---
class HitmanString {
    DWORD size;
    char *data;

  public:
    HitmanString() : size(0x80000000), data(new char[1]{'\0'}) {}

    explicit HitmanString(const char *input) {
        if (input) {
            DWORD actualLength = static_cast<DWORD>(strlen(input));
            size = 0x80000000 | actualLength;
            data = new char[actualLength + 1];
            strcpy_s(data, actualLength + 1, input);
        } else {
            size = 0x80000000;
            data = new char[1]{'\0'};
        }
    }

    HitmanString(const HitmanString &other) : size(other.size) {
        DWORD actualLength = other.size & 0x7FFFFFFF;
        data = new char[actualLength + 1];
        strcpy_s(data, actualLength + 1, other.data);
    }

    HitmanString &operator=(const HitmanString &other) {
        if (this != &other) {
            delete[] data;
            size = other.size;
            DWORD actualLength = other.size & 0x7FFFFFFF;
            data = new char[actualLength + 1];
            strcpy_s(data, actualLength + 1, other.data);
        }
        return *this;
    }

    ~HitmanString() { delete[] data; }

    const char *getString() const { return data; }
    DWORD getLength() const { return size & 0x7FFFFFFF; }
};
