#include "nerv_init.hpp"
#include "main.hpp"
#include "hooks/skin_hooks.hpp"
#include "hooks/chams_hooks.hpp"
#include "hooks/world_hooks.hpp"
#include "features/shared/item_schema.hpp"
#include "features/skin_changer/skin_changer.hpp"
#include "valve/interfaces/vtables/i_mem_alloc.hpp"

namespace nerv {
    static bool g_ready = false;

    bool initialize() {
        if (g_ready)
            return true;

        InitMemAlloc();
        g_modules->m_modules.initialize();
        g_interfaces->initialize();

        if (!g_interfaces->m_entity_system || !g_interfaces->m_source2_client)
            return false;

        g_item_schema->initialize();
        g_skin_changer->initialize();

        if (!g_skin_hooks->initialize())
            return false;

        g_chams_hooks->initialize();
        g_world_hooks->initialize();

        g_ready = true;
        return true;
    }

    bool is_ready() {
        return g_ready;
    }
}
