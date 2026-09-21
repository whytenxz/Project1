#pragma once

#include "../../Backend/Misc/lazy_ptr.hpp"

class c_world_hooks {
public:
    bool initialize();
};

inline lazy_ptr<c_world_hooks> g_world_hooks;
