#include "PlayersTab.h"
#include "../../cs2/players.h"
#include "../../cs2/game_state.h"
#include "../../cs2/memory.h"
#include "../../cs2/offsets.h"
#include "../../Backend/Config/Config.h"
#include "../../Frontend/Steam/SteamAvatars.h"

#include <chrono>
#include <cstdio>
#include <vector>

using namespace IdaLovesMe;

namespace {
    int g_selectedPlayerIndex = -1;
    char g_listLabels[64][96]{};
    std::vector<players::PlayerInfo> g_cachedPlayers;
    std::chrono::steady_clock::time_point g_lastPlayerRefresh{};
    uint64_t g_lastAvatarRequestId = 0;

    const char* TeamName(int team, int localTeam) {
        if (team == 2)
            return "T";
        if (team == 3)
            return "CT";
        if (team == localTeam)
            return "Team";
        return "?";
    }

    void RefreshPlayersIfNeeded() {
        if (!game_state::IsInMatch()) {
            g_cachedPlayers.clear();
            g_selectedPlayerIndex = -1;
            g_lastAvatarRequestId = 0;
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!g_cachedPlayers.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastPlayerRefresh).count() < 500)
            return;

        players::Gather(g_cachedPlayers);
        g_lastPlayerRefresh = now;
    }
}

void PlayersTab::DrawPlayers() {
    RefreshPlayersIfNeeded();

    if (!game_state::IsInMatch()) {
        ui::Label("Join a match to view players");
        return;
    }

    if (g_cachedPlayers.empty()) {
        ui::Label("No players found");
        return;
    }

    bool selectionValid = false;
    for (const auto& player : g_cachedPlayers) {
        if (player.index == g_selectedPlayerIndex) {
            selectionValid = true;
            break;
        }
    }
    if (!selectionValid)
        g_selectedPlayerIndex = g_cachedPlayers.front().index;

    if (ui::BeginListbox("PlayersList")) {
        int labelIndex = 0;
        for (const auto& player : g_cachedPlayers) {
            if (labelIndex >= 64)
                break;

            snprintf(
                g_listLabels[labelIndex],
                sizeof(g_listLabels[labelIndex]),
                "%s%s",
                player.alive ? "" : "[DEAD] ",
                player.name.c_str());

            if (ui::Selectable(g_listLabels[labelIndex], g_selectedPlayerIndex == player.index))
                g_selectedPlayerIndex = player.index;

            ++labelIndex;
        }
        ui::EndListbox();
    }
}

void PlayersTab::DrawAdjustments() {
    RefreshPlayersIfNeeded();

    if (!game_state::IsInMatch()) {
        ui::Label("Join a match to view player details");
        return;
    }

    const players::PlayerInfo* selected = nullptr;
    for (const auto& player : g_cachedPlayers) {
        if (player.index == g_selectedPlayerIndex) {
            selected = &player;
            break;
        }
    }

    if (!selected) {
        ui::Label("Select a player");
        return;
    }

    CConfig* cfg = CConfig::get();

    int localTeam = 0;
    const uintptr_t client = memory::GetModuleBase("client.dll");
    if (client) {
        const uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
        if (localPawn)
            localTeam = memory::Read<uint8_t>(localPawn + schema::C_BaseEntity::m_iTeamNum);
    }

    if (selected->steamId64 >= 76561197960265728ull && selected->steamId64 != g_lastAvatarRequestId) {
        SteamAvatars::Request(selected->steamId64);
        g_lastAvatarRequestId = selected->steamId64;
    }

    const LPDIRECT3DTEXTURE9 avatar = selected->steamId64 >= 76561197960265728ull
        ? SteamAvatars::GetTexture(selected->steamId64)
        : nullptr;

    ui::Image(avatar, Vec2(64.f, 64.f));

    if (selected->steamId64 >= 76561197960265728ull) {
        const auto state = SteamAvatars::GetState(selected->steamId64);
        if (state == SteamAvatars::AvatarState::Loading)
            ui::Label("Loading avatar...");
        else if (state == SteamAvatars::AvatarState::Failed)
            ui::Label("Avatar unavailable");
    }

    char buffer[128]{};
    snprintf(buffer, sizeof(buffer), "Name: %s", selected->name.c_str());
    ui::Label(buffer);

    snprintf(buffer, sizeof(buffer), "Team: %s", TeamName(selected->team, localTeam));
    ui::Label(buffer);

    snprintf(buffer, sizeof(buffer), "Status: %s%s", selected->alive ? "Alive" : "Dead", selected->isLocal ? " (you)" : "");
    ui::Label(buffer);

    if (selected->alive) {
        snprintf(buffer, sizeof(buffer), "Health: %d", selected->health);
        ui::Label(buffer);

        snprintf(buffer, sizeof(buffer), "Armor: %d", selected->armor);
        ui::Label(buffer);

        if (!selected->weapon.empty()) {
            snprintf(buffer, sizeof(buffer), "Weapon: %s", selected->weapon.c_str());
            ui::Label(buffer);
        }

        snprintf(buffer, sizeof(buffer), "Distance: %dm", selected->distance);
        ui::Label(buffer);
    }

    if (selected->hasHelmet)
        ui::Label("Helmet: yes");
    if (selected->hasDefuser)
        ui::Label("Defuser: yes");

    ui::SliderInt("Priority", &cfg->i["players_priority"], -100, 100, "%d");
    ui::Checkbox("Add to whitelist", &cfg->b["players_whitelist"]);
    ui::Checkbox("Add to blacklist", &cfg->b["players_blacklist"]);
}
