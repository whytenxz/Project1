#pragma once

#include "../../Backend/Misc/lazy_ptr.hpp"

class c_chams_hooks {
public:
    bool initialize();
};

inline lazy_ptr<c_chams_hooks> g_chams_hooks;
