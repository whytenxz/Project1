#include "agent_changer.h"
#include "memory.h"
#include "model_changer.h"
#include "offsets.h"
#include "../nerv/config.hpp"
#include "../nerv/features/shared/item_schema.hpp"
#include "../Frontend/Framework/MenuFramework.h"

namespace agent_changer {
    namespace {
        constexpr int kFrameRenderEnd = 6;
        constexpr int kTeamT = 2;
        constexpr int kTeamCt = 3;

        bool g_pendingApply = false;
        int g_lastCtSelection = -1;
        int g_lastTSelection = -1;
        float g_lastSpawnTime = 0.f;
        int g_lastTeam = 0;

        void* GetLocalPawn() {
            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return nullptr;

            const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
            if (!localPawn)
                return nullptr;

            return reinterpret_cast<void*>(localPawn);
        }

        const char* GetSelectedModelPath(int team) {
            if (!g_item_schema->is_initialized())
                return nullptr;

            const int selected = team == kTeamCt
                ? g_cfg->agent_changer.m_ct_agent
                : g_cfg->agent_changer.m_t_agent;

            const auto& agents = team == kTeamCt ? g_item_schema->agents_ct : g_item_schema->agents_t;
            if (selected <= 0 || selected >= static_cast<int>(agents.size()))
                return nullptr;

            return agents[selected].model_path;
        }

        bool ShouldApply(void* pawn) {
            if (!pawn)
                return false;

            if (!g_cfg->agent_changer.m_enabled)
                return false;

            if (g_cfg->model_changer.m_selected > 0)
                return false;

            if (memory::Read<int>(reinterpret_cast<uintptr_t>(pawn) + schema::C_BaseEntity::m_iHealth) <= 0)
                return false;

            const int team = memory::Read<int>(reinterpret_cast<uintptr_t>(pawn) + schema::C_BaseEntity::m_iTeamNum);
            if (team != kTeamT && team != kTeamCt)
                return false;

            const float spawnTime = memory::Read<float>(reinterpret_cast<uintptr_t>(pawn) + schema::C_CSPlayerPawn::m_flLastSpawnTimeIndex);
            const int selected = team == kTeamCt ? g_cfg->agent_changer.m_ct_agent : g_cfg->agent_changer.m_t_agent;

            if (g_pendingApply)
                return true;

            if (team != g_lastTeam)
                return true;

            if (spawnTime != g_lastSpawnTime)
                return true;

            if (team == kTeamCt && selected != g_lastCtSelection)
                return true;

            if (team == kTeamT && selected != g_lastTSelection)
                return true;

            return false;
        }

        void ApplySelectedAgent() {
            void* pawn = GetLocalPawn();
            if (!ShouldApply(pawn))
                return;

            const uintptr_t pawnAddr = reinterpret_cast<uintptr_t>(pawn);
            const int team = memory::Read<int>(pawnAddr + schema::C_BaseEntity::m_iTeamNum);
            const char* modelPath = GetSelectedModelPath(team);
            if (!modelPath || !modelPath[0]) {
                g_pendingApply = false;
                g_lastTeam = team;
                g_lastSpawnTime = memory::Read<float>(pawnAddr + schema::C_CSPlayerPawn::m_flLastSpawnTimeIndex);
                g_lastCtSelection = g_cfg->agent_changer.m_ct_agent;
                g_lastTSelection = g_cfg->agent_changer.m_t_agent;
                return;
            }

            model_changer::SetEntityModel(pawn, modelPath);

            g_pendingApply = false;
            g_lastTeam = team;
            g_lastSpawnTime = memory::Read<float>(pawnAddr + schema::C_CSPlayerPawn::m_flLastSpawnTimeIndex);
            g_lastCtSelection = g_cfg->agent_changer.m_ct_agent;
            g_lastTSelection = g_cfg->agent_changer.m_t_agent;
        }
    }

    void RequestApply() {
        g_pendingApply = true;
    }

    void OnLevelInit() {
        if (g_cfg->agent_changer.m_enabled)
            g_pendingApply = true;
    }

    void OnFrameStage(int stage) {
        if (stage != kFrameRenderEnd)
            return;

        ApplySelectedAgent();
    }

    void DrawMenu() {
        using namespace IdaLovesMe;

        static bool lastEnabled = false;
        ui::Checkbox("Agent changer", &g_cfg->agent_changer.m_enabled);
        if (g_cfg->agent_changer.m_enabled && !lastEnabled)
            RequestApply();
        lastEnabled = g_cfg->agent_changer.m_enabled;

        if (!g_cfg->agent_changer.m_enabled)
            return;

        if (!g_item_schema->is_initialized()) {
            ui::Label("Loading agents...");
            return;
        }

        if (g_cfg->model_changer.m_selected > 0)
            ui::Label("Disabled while custom model is active");

        int ctSelected = g_cfg->agent_changer.m_ct_agent;
        if (ctSelected < 0 || ctSelected >= static_cast<int>(g_item_schema->agent_ct_names_cstr.size()))
            ctSelected = 0;

        int tSelected = g_cfg->agent_changer.m_t_agent;
        if (tSelected < 0 || tSelected >= static_cast<int>(g_item_schema->agent_t_names_cstr.size()))
            tSelected = 0;

        if (!g_item_schema->agent_ct_names_cstr.empty()) {
            if (ui::SingleSelect("CT agent", &ctSelected, g_item_schema->agent_ct_names_cstr)) {
                g_cfg->agent_changer.m_ct_agent = ctSelected;
                if (ctSelected != g_lastCtSelection)
                    RequestApply();
            }
        } else {
            ui::Label("No CT agents found");
        }

        if (!g_item_schema->agent_t_names_cstr.empty()) {
            if (ui::SingleSelect("T agent", &tSelected, g_item_schema->agent_t_names_cstr)) {
                g_cfg->agent_changer.m_t_agent = tSelected;
                if (tSelected != g_lastTSelection)
                    RequestApply();
            }
        } else {
            ui::Label("No T agents found");
        }
    }
}
