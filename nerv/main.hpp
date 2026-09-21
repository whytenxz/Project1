#pragma once

#include "config.hpp"
#include "../Backend/Misc/lazy_ptr.hpp"

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>

#define WINCALL(func) func

#include "sdk/includes/lazy_importer.hpp"
#include "sdk/includes/xor.hpp"
#include "sdk/includes/hash.hpp"
#include "../kiero/minhook/include/MinHook.h"
#include "sdk/console/console.hpp"
#include "sdk/typedefs/vec_t.hpp"
#include "sdk/vfunc/vfunc.hpp"
#include "utils/utils.hpp"
#include "valve/modules/modules.hpp"
#include "valve/interfaces/interfaces.hpp"
#include "valve/schema/schema.hpp"

class c_user_cmd;

struct globals_t {
    c_user_cmd* m_user_cmd = nullptr;
    void* m_local_pawn = nullptr;
    void* m_local_controller = nullptr;
};

inline lazy_ptr<globals_t> g_ctx;
