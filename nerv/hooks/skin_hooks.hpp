#pragma once

#include "../../Backend/Misc/lazy_ptr.hpp"

class c_skin_hooks {
public:
    bool initialize();
};

inline lazy_ptr<c_skin_hooks> g_skin_hooks;
