#pragma once
#include <cstdint>

enum e_convar_flags : int {
    FCVAR_NONE = 0,
    FCVAR_UNREGISTERED = 1,
    FCVAR_DEVELOPMENTONLY = 2,
    FCVAR_GAMEDLL = 4,
    FCVAR_CLIENTDLL = 8,
    FCVAR_PROTECTED = 16,
    FCVAR_USERINFO = 32768,
};

struct c_convar_value_t {
    union {
        bool m_i1;
        std::int16_t m_i16;
        std::int32_t m_i32;
        std::int64_t m_i64;
        float fl;
        double m_db;
        const char* sz;
    };
};

struct c_convar_t {
    void* vfptr;
    c_convar_t* next;
    int registered;
    char pad0[4];
    const char* name;
    const char* help_string;
    int nFlags;
    char pad1[4];
    void* parent;
    const char* default_value;
    c_convar_value_t value;
};

class i_engine_cvar {
public:
    c_convar_t* find(const char* name) {
        if (!this || !name)
            return nullptr;

        using fn_t = c_convar_t * (__fastcall*)(void*, const char*);
        void** vtable = *reinterpret_cast<void***>(this);
        if (!vtable)
            return nullptr;

        constexpr int kFindVarIndex = 40;
        auto fn = reinterpret_cast<fn_t>(vtable[kFindVarIndex]);
        return fn ? fn(this, name) : nullptr;
    }
};
