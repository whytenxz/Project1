#include "SkinsTab.h"
#include "../../nerv/config.hpp"
#include "../../nerv/features/shared/item_schema.hpp"
#include "../../nerv/features/skin_changer/skin_changer.hpp"
#include "../../nerv/nerv_init.hpp"
#include "../../cs2/model_changer.h"
#include "../../cs2/agent_changer.h"
#include "../Framework/MenuFramework.h"

#include <random>
#include <vector>

using namespace IdaLovesMe;

namespace {
    std::mt19937& Rng() {
        static std::mt19937 rng{ std::random_device{}() };
        return rng;
    }

    int RandomInt(int min, int max) {
        if (min >= max)
            return min;
        std::uniform_int_distribution<int> dist(min, max);
        return dist(Rng());
    }

    void RandomizePaintKitIndex(uint16_t def_index, int& paint_kit_index) {
        const auto& kits = g_item_schema->get_paint_kit_names_for_item(def_index);
        if (kits.size() <= 1) {
            paint_kit_index = 0;
            return;
        }

        paint_kit_index = RandomInt(1, static_cast<int>(kits.size()) - 1);
    }

    void RandomizeAll() {
        if (!g_item_schema->is_initialized())
            return;

        if (!g_item_schema->knives.empty()) {
            g_cfg->knife_changer.m_knife = RandomInt(0, static_cast<int>(g_item_schema->knives.size()) - 1);
            const uint16_t knifeDef = g_item_schema->knives[g_cfg->knife_changer.m_knife].definition_index;
            RandomizePaintKitIndex(knifeDef, g_cfg->knife_changer.m_paint_kit);
        }

        if (!g_item_schema->gloves.empty()) {
            g_cfg->glove_changer.m_glove = RandomInt(0, static_cast<int>(g_item_schema->gloves.size()) - 1);
            const uint16_t gloveDef = g_item_schema->gloves[g_cfg->glove_changer.m_glove].definition_index;
            RandomizePaintKitIndex(gloveDef, g_cfg->glove_changer.m_paint_kit);
        }

        for (const auto& weapon : g_item_schema->weapons) {
            const int configIndex = c_config::skin_changer_t::get_config_index(weapon.definition_index);
            if (configIndex <= 0)
                continue;

            RandomizePaintKitIndex(weapon.definition_index, g_cfg->skin_changer.weapon_skins[configIndex].paint_kit);
        }

        if (!g_item_schema->agents_ct.empty())
            g_cfg->agent_changer.m_ct_agent = RandomInt(1, static_cast<int>(g_item_schema->agents_ct.size()) - 1);

        if (!g_item_schema->agents_t.empty())
            g_cfg->agent_changer.m_t_agent = RandomInt(1, static_cast<int>(g_item_schema->agents_t.size()) - 1);

        agent_changer::RequestApply();
        g_skin_changer->should_update = true;
    }
}

static std::vector<const char*> BuildPaintKitItems(uint16_t def_index) {
    if (!g_item_schema->is_initialized())
        return { "Default" };
    return g_item_schema->get_paint_kit_names_for_item(def_index);
}

void SkinsTab::DrawModelOptions() {
    if (!nerv::initialize())
        return;

    g_skin_changer->initialize();

    if (g_item_schema->is_initialized()) {
        if (ui::Button("Randomize all"))
            RandomizeAll();
    } else {
        ui::Label("Loading item schema...");
    }

    model_changer::DrawMenu();

    agent_changer::DrawMenu();

    ui::Checkbox("Knife changer", &g_cfg->knife_changer.m_enabled);

    if (g_cfg->knife_changer.m_enabled) {
        static int last_knife = -1;

        if (g_item_schema->is_initialized() && !g_item_schema->knife_names_cstr.empty()) {
            ui::SingleSelect("Knife model", &g_cfg->knife_changer.m_knife, g_item_schema->knife_names_cstr);
        } else {
            ui::Label("Loading knife list...");
        }

        uint16_t selected_knife = 0;
        if (g_item_schema->is_initialized() && g_cfg->knife_changer.m_knife < (int)g_item_schema->knives.size())
            selected_knife = g_item_schema->knives[g_cfg->knife_changer.m_knife].definition_index;

        if (last_knife != g_cfg->knife_changer.m_knife) {
            g_cfg->knife_changer.m_paint_kit = 0;
            last_knife = g_cfg->knife_changer.m_knife;
        }

        if (g_item_schema->is_initialized()) {
            auto kits = BuildPaintKitItems(selected_knife);
            ui::SingleSelect("Knife skin", &g_cfg->knife_changer.m_paint_kit, kits);
        }

        ui::SliderFloat("Knife wear", &g_cfg->knife_changer.m_wear, 0.f, 1.f, "%.4f", 1.f);
        ui::SliderInt("Knife seed", &g_cfg->knife_changer.m_seed, 0, 1000, "%d");
        ui::InputText("Knife name", g_cfg->knife_changer.m_custom_name);
    }

    ui::Checkbox("Glove changer", &g_cfg->glove_changer.m_enabled);

    if (g_cfg->glove_changer.m_enabled) {
        static int last_glove = -1;

        if (g_item_schema->is_initialized() && !g_item_schema->glove_names_cstr.empty()) {
            ui::SingleSelect("Glove model", &g_cfg->glove_changer.m_glove, g_item_schema->glove_names_cstr);
        } else {
            ui::Label("Loading glove list...");
        }

        uint16_t selected_glove = 0;
        if (g_item_schema->is_initialized() && g_cfg->glove_changer.m_glove < (int)g_item_schema->gloves.size())
            selected_glove = g_item_schema->gloves[g_cfg->glove_changer.m_glove].definition_index;

        if (last_glove != g_cfg->glove_changer.m_glove) {
            auto kits = BuildPaintKitItems(selected_glove);
            g_cfg->glove_changer.m_paint_kit = kits.size() > 1 ? 1 : 0;
            last_glove = g_cfg->glove_changer.m_glove;
        }

        if (g_item_schema->is_initialized()) {
            auto kits = BuildPaintKitItems(selected_glove);
            ui::SingleSelect("Glove skin", &g_cfg->glove_changer.m_paint_kit, kits);
        }

        ui::SliderFloat("Glove wear", &g_cfg->glove_changer.m_wear, 0.f, 1.f, "%.4f", 1.f);
        ui::SliderInt("Glove seed", &g_cfg->glove_changer.m_seed, 0, 1000, "%d");
    }
}

void SkinsTab::DrawWeaponSkins() {
    if (!nerv::initialize())
        return;

    g_skin_changer->initialize();

    ui::Checkbox("Skin changer", &g_cfg->skin_changer.m_enabled);

    if (!g_cfg->skin_changer.m_enabled)
        return;

    if (!g_item_schema->is_initialized()) {
        ui::Label("Loading weapons...");
        return;
    }

    if (!g_item_schema->weapon_names_cstr.empty())
        ui::SingleSelect("Weapon", &g_cfg->skin_changer.m_selected_weapon, g_item_schema->weapon_names_cstr);

    uint16_t selected_weapon_def = 0;
    if (g_cfg->skin_changer.m_selected_weapon >= 0
        && g_cfg->skin_changer.m_selected_weapon < (int)g_item_schema->weapons.size())
        selected_weapon_def = g_item_schema->weapons[g_cfg->skin_changer.m_selected_weapon].definition_index;

    if (selected_weapon_def <= 0)
        return;

    const int config_index = c_config::skin_changer_t::get_config_index(selected_weapon_def);
    auto& weapon_skin = g_cfg->skin_changer.weapon_skins[config_index];
    const auto kits = BuildPaintKitItems(selected_weapon_def);

    if (!kits.empty())
        ui::SingleSelect("Paint kit", &weapon_skin.paint_kit, kits);

    ui::SliderFloat("Weapon wear", &weapon_skin.wear, 0.f, 1.f, "%.4f", 1.f);
    ui::SliderInt("Weapon seed", &weapon_skin.seed, 0, 1000, "%d");
    ui::InputText("Weapon name", weapon_skin.custom_name);

    if (ui::Button("Apply skin"))
        g_skin_changer->should_update = true;
}
