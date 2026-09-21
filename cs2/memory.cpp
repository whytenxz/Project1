#include "memory.h"

namespace memory {
    namespace detail {
        bool CopyMemorySafe(void* dst, const void* src, size_t size) {
            if (!dst || !src || !size)
                return false;

            __try {
                memcpy(dst, src, size);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        bool ReadBytes(uintptr_t address, void* out, size_t size) {
            if (!address || !out || !size)
                return false;
            return CopyMemorySafe(out, reinterpret_cast<const void*>(address), size);
        }

        bool WriteBytes(uintptr_t address, const void* src, size_t size) {
            if (!address || !src || !size)
                return false;
            return CopyMemorySafe(reinterpret_cast<void*>(address), src, size);
        }
    }

    std::string ReadString(uintptr_t address, size_t maxLen) {
        if (!address || !maxLen)
            return {};

        std::string result;
        result.reserve((std::min)(maxLen, size_t{ 128 }));

        for (size_t i = 0; i < maxLen; ++i) {
            char c{};
            if (!detail::ReadBytes(address + i, &c, sizeof(c)))
                break;
            if (!c)
                break;
            result.push_back(c);
        }

        return result;
    }
}
