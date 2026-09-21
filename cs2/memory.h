#pragma once
#include "offsets.h"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace memory {
    namespace detail {
        bool ReadBytes(uintptr_t address, void* out, size_t size);
        bool WriteBytes(uintptr_t address, const void* src, size_t size);
    }

    inline uintptr_t GetModuleBase(const char* moduleName) {
        return reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
    }

    template<typename T>
    inline T Read(uintptr_t address) {
        T result{};
        if (!address)
            return result;
        detail::ReadBytes(address, &result, sizeof(T));
        return result;
    }

    template<typename T>
    inline void Write(uintptr_t address, const T& value) {
        if (!address)
            return;
        detail::WriteBytes(address, &value, sizeof(T));
    }

    std::string ReadString(uintptr_t address, size_t maxLen = 128);

    inline uintptr_t GetEntityByIndex(uintptr_t entityList, int index) {
        if (!entityList)
            return 0;

        const uintptr_t listEntry = Read<uintptr_t>(entityList + 0x10 + 8 * (index >> 9));
        if (!listEntry)
            return 0;
        return Read<uintptr_t>(listEntry + 0x70 * (index & 0x1FF));
    }

    inline uintptr_t ResolveHandle(uintptr_t entityList, uint32_t handle) {
        if (!handle || handle == 0xFFFFFFFF)
            return 0;
        return GetEntityByIndex(entityList, handle & 0x7FFF);
    }

    inline int GetHighestEntityIndex(uintptr_t entityList) {
        if (!entityList)
            return 512;

        const int highest = Read<int>(entityList + offsets::client_dll::dwGameEntitySystem_highestEntityIndex);
        if (highest < 64)
            return 512;
        if (highest > 8192)
            return 8192;
        return highest;
    }

    inline uintptr_t FindPattern(const char* moduleName, const char* pattern) {
        const uintptr_t module = GetModuleBase(moduleName);
        if (!module)
            return 0;

        const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        const auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(module + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return 0;

        const auto base = reinterpret_cast<const uint8_t*>(module);
        const size_t size = nt->OptionalHeader.SizeOfImage;

        std::vector<int> bytes;
        bytes.reserve(64);
        for (const char* p = pattern; *p; ) {
            if (*p == ' ') {
                ++p;
                continue;
            }
            if (*p == '?') {
                bytes.push_back(-1);
                p += (*++p == '?') ? 2 : 1;
                continue;
            }
            bytes.push_back(static_cast<int>(strtoul(p, const_cast<char**>(&p), 16)));
        }

        if (bytes.empty())
            return 0;

        for (size_t i = 0; i + bytes.size() <= size; ++i) {
            bool found = true;
            for (size_t j = 0; j < bytes.size(); ++j) {
                if (bytes[j] != -1 && base[i + j] != static_cast<uint8_t>(bytes[j])) {
                    found = false;
                    break;
                }
            }
            if (found)
                return module + i;
        }

        return 0;
    }
}
