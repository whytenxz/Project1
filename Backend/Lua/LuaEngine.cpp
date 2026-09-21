#include "LuaEngine.h"
#include "../Config/Config.h"
#include "../Features/Visuals/EventLogger.h"
#include "../Globalincludes.h"
#include "../Utilities/Utilities.h"
#include "../../cs2/game_state.h"
#include "../../cs2/math.h"
#include "../../cs2/memory.h"
#include "../../cs2/offsets.h"
#include "../../cs2/players.h"
#include "../../Frontend/Menu/Menu.h"
#include "../../Frontend/Framework/MenuFramework.h"
#include "../../Frontend/Renderer/Renderer.h"
#include "../../Frontend/Renderer/color.h"
#include "../GameData/cs2_game_data.h"
#include "imgui/imgui.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <ShlObj.h>

#pragma comment(lib, "shell32.lib")

namespace lua_engine {
    namespace {
        lua_State* g_lua = nullptr;
        thread_local std::string g_lastError;
        std::unordered_map<std::string, int> g_callbacks;
        double g_realtime = 0.0;

        struct UiBoolState {
            bool value = false;
            bool initialized = false;
        };

        std::unordered_map<std::string, UiBoolState> g_uiBools;
        bool g_uiMenuDrawing = false;

        void SetError(const char* message) {
            g_lastError = message ? message : "unknown lua error";
        }

        D3DCOLOR ReadColor(lua_State* L, int rIdx, int gIdx, int bIdx, int aIdx) {
            const int r = static_cast<int>(luaL_checkinteger(L, rIdx));
            const int g = static_cast<int>(luaL_checkinteger(L, gIdx));
            const int b = static_cast<int>(luaL_checkinteger(L, bIdx));
            const int a = lua_gettop(L) >= aIdx ? static_cast<int>(luaL_checkinteger(L, aIdx)) : 255;
            return D3DCOLOR_RGBA(
                std::clamp(r, 0, 255),
                std::clamp(g, 0, 255),
                std::clamp(b, 0, 255),
                std::clamp(a, 0, 255));
        }

        bool ReadViewMatrix(float out[16]) {
            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return false;

            for (int i = 0; i < 16; ++i)
                out[i] = memory::Read<float>(client + offsets::client_dll::dwViewMatrix + i * sizeof(float));
            return true;
        }

        void PushPlayerTable(lua_State* L, const players::PlayerInfo& player) {
            lua_newtable(L);
            lua_pushinteger(L, player.index);
            lua_setfield(L, -2, "index");
            lua_pushstring(L, player.name.c_str());
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, player.team);
            lua_setfield(L, -2, "team");
            lua_pushinteger(L, player.health);
            lua_setfield(L, -2, "health");
            lua_pushinteger(L, player.armor);
            lua_setfield(L, -2, "armor");
            lua_pushboolean(L, player.alive);
            lua_setfield(L, -2, "alive");
            lua_pushboolean(L, player.isLocal);
            lua_setfield(L, -2, "local");
            lua_pushboolean(L, player.hasHelmet);
            lua_setfield(L, -2, "helmet");
            lua_pushboolean(L, player.hasDefuser);
            lua_setfield(L, -2, "defuser");
            lua_pushstring(L, player.weapon.c_str());
            lua_setfield(L, -2, "weapon");
            lua_pushinteger(L, player.distance);
            lua_setfield(L, -2, "distance");
            lua_pushinteger(L, static_cast<lua_Integer>(player.steamId64));
            lua_setfield(L, -2, "steam_id");

            if (player.hasPosition) {
                lua_pushnumber(L, player.x);
                lua_setfield(L, -2, "x");
                lua_pushnumber(L, player.y);
                lua_setfield(L, -2, "y");
                lua_pushnumber(L, player.z);
                lua_setfield(L, -2, "z");
                lua_pushnumber(L, player.x);
                lua_setfield(L, -2, "head_x");
                lua_pushnumber(L, player.y);
                lua_setfield(L, -2, "head_y");
                lua_pushnumber(L, player.z + 72.f);
                lua_setfield(L, -2, "head_z");
            }
        }

        void ClearUiState() {
            g_uiBools.clear();
            g_uiMenuDrawing = false;
        }

        void ClearCallbacks() {
            if (!g_lua) {
                g_callbacks.clear();
                ClearUiState();
                return;
            }

            for (const auto& entry : g_callbacks)
                luaL_unref(g_lua, LUA_REGISTRYINDEX, entry.second);
            g_callbacks.clear();
            ClearUiState();
        }

        UiBoolState& GetUiBool(const char* id) {
            return g_uiBools[id];
        }

        void InitUiBool(UiBoolState& state, lua_State* L, int defaultIndex) {
            if (state.initialized)
                return;
            state.value = lua_gettop(L) >= defaultIndex ? lua_toboolean(L, defaultIndex) != 0 : false;
            state.initialized = true;
        }

        bool CallFunction(int ref) {
            if (!g_lua || ref == LUA_NOREF)
                return false;

            lua_rawgeti(g_lua, LUA_REGISTRYINDEX, ref);
            if (lua_pcall(g_lua, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(g_lua, -1);
                if (err)
                    Features::EventLogger->AddLog("[lua] %s", err);
                lua_pop(g_lua, 1);
                return false;
            }
            return true;
        }

        int LuaPrint(lua_State* L) {
            const int count = lua_gettop(L);
            std::string message;
            for (int i = 1; i <= count; ++i) {
                if (i > 1)
                    message += '\t';
                size_t len = 0;
                const char* text = luaL_tolstring(L, i, &len);
                if (text)
                    message.append(text, len);
                lua_pop(L, 1);
            }

            if (!message.empty())
                Features::EventLogger->AddLog("[lua] %s", message.c_str());
            return 0;
        }

        int LuaError(lua_State* L) {
            return LuaPrint(L);
        }

        int LuaConfigGetBool(lua_State* L) {
            lua_pushboolean(L, CConfig::get()->b[std::string(luaL_checkstring(L, 1))]);
            return 1;
        }

        int LuaConfigSetBool(lua_State* L) {
            CConfig::get()->b[std::string(luaL_checkstring(L, 1))] = lua_toboolean(L, 2) != 0;
            return 0;
        }

        int LuaConfigGetInt(lua_State* L) {
            lua_pushinteger(L, CConfig::get()->i[std::string(luaL_checkstring(L, 1))]);
            return 1;
        }

        int LuaConfigSetInt(lua_State* L) {
            CConfig::get()->i[std::string(luaL_checkstring(L, 1))] = static_cast<int>(luaL_checkinteger(L, 2));
            return 0;
        }

        int LuaConfigGetFloat(lua_State* L) {
            lua_pushnumber(L, CConfig::get()->f[std::string(luaL_checkstring(L, 1))]);
            return 1;
        }

        int LuaConfigSetFloat(lua_State* L) {
            CConfig::get()->f[std::string(luaL_checkstring(L, 1))] = static_cast<float>(luaL_checknumber(L, 2));
            return 0;
        }

        int LuaConfigGetString(lua_State* L) {
            lua_pushstring(L, CConfig::get()->s[std::string(luaL_checkstring(L, 1))]);
            return 1;
        }

        int LuaConfigSetString(lua_State* L) {
            const char* key = luaL_checkstring(L, 1);
            const char* value = luaL_checkstring(L, 2);
            strcpy_s(CConfig::get()->s[std::string(key)], value);
            return 0;
        }

        int LuaConfigGetColor(lua_State* L) {
            const auto& color = CConfig::get()->c[std::string(luaL_checkstring(L, 1))];
            lua_newtable(L);
            lua_pushinteger(L, color[0]);
            lua_setfield(L, -2, "r");
            lua_pushinteger(L, color[1]);
            lua_setfield(L, -2, "g");
            lua_pushinteger(L, color[2]);
            lua_setfield(L, -2, "b");
            lua_pushinteger(L, color[3]);
            lua_setfield(L, -2, "a");
            return 1;
        }

        int LuaConfigSetColor(lua_State* L) {
            const char* key = luaL_checkstring(L, 1);
            auto& color = CConfig::get()->c[std::string(key)];
            if (lua_istable(L, 2)) {
                lua_getfield(L, 2, "r");
                color[0] = static_cast<int>(luaL_checkinteger(L, -1));
                lua_pop(L, 1);
                lua_getfield(L, 2, "g");
                color[1] = static_cast<int>(luaL_checkinteger(L, -1));
                lua_pop(L, 1);
                lua_getfield(L, 2, "b");
                color[2] = static_cast<int>(luaL_checkinteger(L, -1));
                lua_pop(L, 1);
                lua_getfield(L, 2, "a");
                color[3] = static_cast<int>(luaL_optinteger(L, -1, 255));
                lua_pop(L, 1);
            } else {
                color[0] = static_cast<int>(luaL_checkinteger(L, 2));
                color[1] = static_cast<int>(luaL_checkinteger(L, 3));
                color[2] = static_cast<int>(luaL_checkinteger(L, 4));
                color[3] = static_cast<int>(luaL_optinteger(L, 5, 255));
            }
            return 0;
        }

        int LuaConfigBindActive(lua_State* L) {
            lua_pushboolean(L, CConfig::get()->IsBindActive(luaL_checkstring(L, 1)));
            return 1;
        }

        int LuaConfigLoad(lua_State* L) {
            (void)L;
            CConfig::get()->Load();
            return 0;
        }

        int LuaConfigSave(lua_State* L) {
            (void)L;
            CConfig::get()->Save();
            return 0;
        }

        int LuaClientNotify(lua_State* L) {
            const char* message = luaL_checkstring(L, 1);
            if (message && message[0])
                Features::EventLogger->AddLog("[lua] %s", message);
            return 0;
        }

        int LuaClientMousePosition(lua_State* L) {
            (void)L;
            POINT point{};
            if (GetCursorPos(&point) && g_GameWindow)
                ScreenToClient(g_GameWindow, &point);

            lua_pushnumber(L, static_cast<lua_Number>(point.x));
            lua_pushnumber(L, static_cast<lua_Number>(point.y));
            return 2;
        }

        int LuaClientScriptDir(lua_State* L) {
            char base[MAX_PATH]{};
            if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, base)))
                GetCurrentDirectoryA(MAX_PATH, base);

            const std::filesystem::path dir = std::filesystem::path(base) / "gamesense-cs2" / "scripts";
            lua_pushstring(L, dir.string().c_str());
            return 1;
        }

        int LuaClientHasCallback(lua_State* L) {
            const char* event = luaL_checkstring(L, 1);
            lua_pushboolean(L, g_callbacks.find(event) != g_callbacks.end());
            return 1;
        }

        int LuaClientScreenSize(lua_State* L) {
            (void)L;
            if (Render::Draw) {
                lua_pushnumber(L, Render::Draw->Screen.Width);
                lua_pushnumber(L, Render::Draw->Screen.Height);
            } else {
                lua_pushnumber(L, 1920);
                lua_pushnumber(L, 1080);
            }
            return 2;
        }

        int LuaClientDeltaTime(lua_State* L) {
            lua_pushnumber(L, Misc::Utilities ? Misc::Utilities->GetDeltaTime() : 0.0);
            return 1;
        }

        int LuaClientRealtime(lua_State* L) {
            lua_pushnumber(L, g_realtime);
            return 1;
        }

        int LuaClientInMatch(lua_State* L) {
            (void)L;
            lua_pushboolean(L, game_state::IsInMatch());
            return 1;
        }

        int LuaClientMenuOpen(lua_State* L) {
            (void)L;
            lua_pushboolean(L, CMenu::get()->IsMenuOpened());
            return 1;
        }

        int LuaClientSetCallback(lua_State* L) {
            const char* event = luaL_checkstring(L, 1);
            luaL_checktype(L, 2, LUA_TFUNCTION);

            auto it = g_callbacks.find(event);
            if (it != g_callbacks.end()) {
                luaL_unref(L, LUA_REGISTRYINDEX, it->second);
                g_callbacks.erase(it);
            }

            lua_pushvalue(L, 2);
            g_callbacks[event] = luaL_ref(L, LUA_REGISTRYINDEX);
            return 0;
        }

        int LuaClientClearCallbacks(lua_State* L) {
            (void)L;
            ClearCallbacks();
            return 0;
        }

        int LuaEntityGetLocal(lua_State* L) {
            std::vector<players::PlayerInfo> list;
            players::Gather(list);
            for (const auto& player : list) {
                if (player.isLocal) {
                    PushPlayerTable(L, player);
                    return 1;
                }
            }
            lua_pushnil(L);
            return 1;
        }

        int LuaEntityGetPlayers(lua_State* L) {
            const bool enemiesOnly = lua_toboolean(L, 1) != 0;
            int localTeam = 0;

            std::vector<players::PlayerInfo> list;
            players::Gather(list);

            lua_newtable(L);
            int outIndex = 1;
            for (const auto& player : list) {
                if (player.isLocal) {
                    localTeam = player.team;
                    continue;
                }
                if (!player.alive)
                    continue;
                if (enemiesOnly && localTeam != 0 && player.team == localTeam)
                    continue;

                PushPlayerTable(L, player);
                lua_rawseti(L, -2, outIndex++);
            }
            return 1;
        }

        int LuaEntityGetSpectators(lua_State* L) {
            std::vector<std::string> spectators;
            players::GatherSpectators(spectators);

            lua_newtable(L);
            int index = 1;
            for (const auto& name : spectators) {
                lua_pushstring(L, name.c_str());
                lua_rawseti(L, -2, index++);
            }
            return 1;
        }

        int LuaEntityGetByIndex(lua_State* L) {
            const int index = static_cast<int>(luaL_checkinteger(L, 1));
            std::vector<players::PlayerInfo> list;
            players::Gather(list);
            for (const auto& player : list) {
                if (player.index == index) {
                    PushPlayerTable(L, player);
                    return 1;
                }
            }
            lua_pushnil(L);
            return 1;
        }

        int LuaEntityCount(lua_State* L) {
            std::vector<players::PlayerInfo> list;
            players::Gather(list);
            lua_pushinteger(L, static_cast<lua_Integer>(list.size()));
            return 1;
        }

        int LuaInputKeyDown(lua_State* L) {
            const int vk = static_cast<int>(luaL_checkinteger(L, 1));
            lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) != 0);
            return 1;
        }

        int LuaInputKeyPressed(lua_State* L) {
            static std::unordered_map<int, bool> previous;
            const int vk = static_cast<int>(luaL_checkinteger(L, 1));
            const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            const bool pressed = down && !previous[vk];
            previous[vk] = down;
            lua_pushboolean(L, pressed);
            return 1;
        }

        int LuaRenderLine(lua_State* L) {
            const float x1 = static_cast<float>(luaL_checknumber(L, 1));
            const float y1 = static_cast<float>(luaL_checknumber(L, 2));
            const float x2 = static_cast<float>(luaL_checknumber(L, 3));
            const float y2 = static_cast<float>(luaL_checknumber(L, 4));
            const D3DCOLOR color = ReadColor(L, 5, 6, 7, 8);
            if (Render::Draw)
                Render::Draw->Line({ x1, y1 }, { x2, y2 }, color);
            return 0;
        }

        int LuaRenderRect(lua_State* L) {
            const float x = static_cast<float>(luaL_checknumber(L, 1));
            const float y = static_cast<float>(luaL_checknumber(L, 2));
            const float w = static_cast<float>(luaL_checknumber(L, 3));
            const float h = static_cast<float>(luaL_checknumber(L, 4));
            const D3DCOLOR color = ReadColor(L, 5, 6, 7, 8);
            const float thickness = lua_gettop(L) >= 9 ? static_cast<float>(luaL_checknumber(L, 9)) : 1.f;
            if (Render::Draw)
                Render::Draw->Rect({ x, y }, { w, h }, thickness, color);
            return 0;
        }

        int LuaRenderFilledRect(lua_State* L) {
            const float x = static_cast<float>(luaL_checknumber(L, 1));
            const float y = static_cast<float>(luaL_checknumber(L, 2));
            const float w = static_cast<float>(luaL_checknumber(L, 3));
            const float h = static_cast<float>(luaL_checknumber(L, 4));
            const D3DCOLOR color = ReadColor(L, 5, 6, 7, 8);
            if (Render::Draw)
                Render::Draw->FilledRect({ x, y }, { w, h }, color);
            return 0;
        }

        int LuaRenderBorderedRect(lua_State* L) {
            const float x = static_cast<float>(luaL_checknumber(L, 1));
            const float y = static_cast<float>(luaL_checknumber(L, 2));
            const float w = static_cast<float>(luaL_checknumber(L, 3));
            const float h = static_cast<float>(luaL_checknumber(L, 4));
            const D3DCOLOR fill = ReadColor(L, 5, 6, 7, 8);
            const D3DCOLOR border = ReadColor(L, 9, 10, 11, 12);
            const float borderWidth = lua_gettop(L) >= 13 ? static_cast<float>(luaL_checknumber(L, 13)) : 1.f;
            if (Render::Draw)
                Render::Draw->BorderedRect({ x, y }, { w, h }, borderWidth, fill, border);
            return 0;
        }

        int LuaRenderGradient(lua_State* L) {
            const float x = static_cast<float>(luaL_checknumber(L, 1));
            const float y = static_cast<float>(luaL_checknumber(L, 2));
            const float w = static_cast<float>(luaL_checknumber(L, 3));
            const float h = static_cast<float>(luaL_checknumber(L, 4));
            const D3DCOLOR left = ReadColor(L, 5, 6, 7, 8);
            const D3DCOLOR right = ReadColor(L, 9, 10, 11, 12);
            const bool vertical = lua_gettop(L) >= 13 ? lua_toboolean(L, 13) != 0 : false;
            if (Render::Draw)
                Render::Draw->Gradient({ x, y }, { w, h }, left, right, vertical);
            return 0;
        }

        int LuaRenderTriangle(lua_State* L) {
            const float x1 = static_cast<float>(luaL_checknumber(L, 1));
            const float y1 = static_cast<float>(luaL_checknumber(L, 2));
            const float x2 = static_cast<float>(luaL_checknumber(L, 3));
            const float y2 = static_cast<float>(luaL_checknumber(L, 4));
            const float x3 = static_cast<float>(luaL_checknumber(L, 5));
            const float y3 = static_cast<float>(luaL_checknumber(L, 6));
            const D3DCOLOR color = ReadColor(L, 7, 8, 9, 10);
            if (Render::Draw)
                Render::Draw->Triangle({ x1, y1 }, { x2, y2 }, { x3, y3 }, color);
            return 0;
        }

        int LuaRenderText(lua_State* L) {
            const char* text = luaL_checkstring(L, 1);
            const float x = static_cast<float>(luaL_checknumber(L, 2));
            const float y = static_cast<float>(luaL_checknumber(L, 3));
            const D3DCOLOR color = ReadColor(L, 4, 5, 6, 7);
            const int align = lua_gettop(L) >= 8 ? static_cast<int>(luaL_checkinteger(L, 8)) : LEFT;
            if (Render::Draw)
                Render::Draw->Text(text, x, y, align, Render::Fonts::Verdana, true, color);
            return 0;
        }

        int LuaRenderCircle(lua_State* L) {
            const float x = static_cast<float>(luaL_checknumber(L, 1));
            const float y = static_cast<float>(luaL_checknumber(L, 2));
            const float radius = static_cast<float>(luaL_checknumber(L, 3));
            const D3DCOLOR color = ReadColor(L, 4, 5, 6, 7);
            const int segments = lua_gettop(L) >= 8 ? static_cast<int>(luaL_checkinteger(L, 8)) : 24;
            const bool filled = lua_gettop(L) >= 9 ? lua_toboolean(L, 9) != 0 : false;

            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            if (!drawList)
                return 0;

            const ImU32 imColor = IM_COL32(get_r(color), get_g(color), get_b(color), get_a(color));
            if (filled)
                drawList->AddCircleFilled(ImVec2(x, y), radius, imColor, segments);
            else
                drawList->AddCircle(ImVec2(x, y), radius, imColor, segments, 1.5f);
            return 0;
        }

        int LuaRenderMeasureText(lua_State* L) {
            const char* text = luaL_checkstring(L, 1);
            if (!Render::Draw) {
                lua_pushnumber(L, 0);
                lua_pushnumber(L, 0);
                return 2;
            }
            const Render::Vec2 textSize = Render::Draw->GetTextSize(Render::Fonts::Verdana, text);
            lua_pushnumber(L, textSize.x);
            lua_pushnumber(L, textSize.y);
            return 2;
        }

        int LuaRenderWorldToScreen(lua_State* L) {
            const Vector3 world{
                static_cast<float>(luaL_checknumber(L, 1)),
                static_cast<float>(luaL_checknumber(L, 2)),
                static_cast<float>(luaL_checknumber(L, 3))
            };

            float matrix[16]{};
            if (!ReadViewMatrix(matrix)) {
                lua_pushnil(L);
                return 1;
            }

            const int width = Render::Draw ? static_cast<int>(Render::Draw->Screen.Width) : 1920;
            const int height = Render::Draw ? static_cast<int>(Render::Draw->Screen.Height) : 1080;

            Vector2 screen{};
            if (!WorldToScreen(world, screen, matrix, width, height)) {
                lua_pushnil(L);
                return 1;
            }

            lua_pushnumber(L, screen.x);
            lua_pushnumber(L, screen.y);
            return 2;
        }

        int LuaUtilsDistance(lua_State* L) {
            const float x1 = static_cast<float>(luaL_checknumber(L, 1));
            const float y1 = static_cast<float>(luaL_checknumber(L, 2));
            const float z1 = static_cast<float>(luaL_checknumber(L, 3));
            const float x2 = static_cast<float>(luaL_checknumber(L, 4));
            const float y2 = static_cast<float>(luaL_checknumber(L, 5));
            const float z2 = static_cast<float>(luaL_checknumber(L, 6));
            const float dx = x2 - x1;
            const float dy = y2 - y1;
            const float dz = z2 - z1;
            lua_pushnumber(L, std::sqrt(dx * dx + dy * dy + dz * dz));
            return 1;
        }

        int LuaUtilsClamp(lua_State* L) {
            const double value = luaL_checknumber(L, 1);
            const double minValue = luaL_checknumber(L, 2);
            const double maxValue = luaL_checknumber(L, 3);
            lua_pushnumber(L, std::clamp(value, minValue, maxValue));
            return 1;
        }

        int LuaUtilsLerp(lua_State* L) {
            const double a = luaL_checknumber(L, 1);
            const double b = luaL_checknumber(L, 2);
            const double t = luaL_checknumber(L, 3);
            lua_pushnumber(L, a + (b - a) * t);
            return 1;
        }

        int LuaMathDeg2Rad(lua_State* L) {
            lua_pushnumber(L, static_cast<lua_Number>(luaL_checknumber(L, 1) * (3.141592653589793 / 180.0)));
            return 1;
        }

        int LuaMathRad2Deg(lua_State* L) {
            lua_pushnumber(L, static_cast<lua_Number>(luaL_checknumber(L, 1) * (180.0 / 3.141592653589793)));
            return 1;
        }

        int LuaMathNormalizeYaw(lua_State* L) {
            double yaw = luaL_checknumber(L, 1);
            while (yaw > 180.0) yaw -= 360.0;
            while (yaw < -180.0) yaw += 360.0;
            lua_pushnumber(L, yaw);
            return 1;
        }

        int LuaMathAngleDiff(lua_State* L) {
            double a = luaL_checknumber(L, 1);
            double b = luaL_checknumber(L, 2);
            double diff = b - a;
            while (diff > 180.0) diff -= 360.0;
            while (diff < -180.0) diff += 360.0;
            lua_pushnumber(L, diff);
            return 1;
        }

        int LuaUiCheckbox(lua_State* L) {
            const char* id = luaL_checkstring(L, 1);
            const char* label = luaL_checkstring(L, 2);
            auto& state = GetUiBool(id);
            InitUiBool(state, L, 3);

            if (g_uiMenuDrawing && IdaLovesMe::Globals::Gui_Ctx)
                IdaLovesMe::ui::Checkbox(label, &state.value);

            lua_pushboolean(L, state.value);
            return 1;
        }

        int LuaUiLabel(lua_State* L) {
            const char* text = luaL_checkstring(L, 1);
            const bool special = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) != 0 : false;
            if (g_uiMenuDrawing && IdaLovesMe::Globals::Gui_Ctx)
                IdaLovesMe::ui::Label(text, special);
            return 0;
        }

        int LuaUiButton(lua_State* L) {
            const char* label = luaL_checkstring(L, 2);
            (void)luaL_checkstring(L, 1);

            bool pressed = false;
            if (g_uiMenuDrawing && IdaLovesMe::Globals::Gui_Ctx)
                pressed = IdaLovesMe::ui::Button(label);

            lua_pushboolean(L, pressed);
            return 1;
        }

        int LuaUiGetBool(lua_State* L) {
            const char* id = luaL_checkstring(L, 1);
            auto& state = GetUiBool(id);
            InitUiBool(state, L, 2);
            lua_pushboolean(L, state.value);
            return 1;
        }

        int LuaUiSetBool(lua_State* L) {
            const char* id = luaL_checkstring(L, 1);
            auto& state = GetUiBool(id);
            state.value = lua_toboolean(L, 2) != 0;
            state.initialized = true;
            return 0;
        }

        int LuaUiRenderCheckbox(lua_State* L) {
            const char* id = luaL_checkstring(L, 1);
            const float x = static_cast<float>(luaL_checknumber(L, 2));
            const float y = static_cast<float>(luaL_checknumber(L, 3));
            const char* label = luaL_checkstring(L, 4);
            auto& state = GetUiBool(id);
            InitUiBool(state, L, 5);

            const float box = 8.f;
            const Render::Vec2 labelSize = Render::Draw
                ? Render::Draw->GetTextSize(Render::Fonts::Tahombd, label)
                : Render::Vec2{ 0.f, 0.f };
            const float totalW = box + 6.f + labelSize.x;
            const float totalH = (std::max)(box, labelSize.y);

            CConfig* cfg = CConfig::get();
            const int ar = cfg->c["MenuColor"][0];
            const int ag = cfg->c["MenuColor"][1];
            const int ab = cfg->c["MenuColor"][2];

            if (Render::Draw) {
                if (state.value)
                    Render::Draw->FilledRect({ x, y }, { box, box }, D3DCOLOR_RGBA(ar, ag, ab, 220));
                else
                    Render::Draw->FilledRect({ x, y }, { box, box }, D3DCOLOR_RGBA(55, 55, 55, 220));

                Render::Draw->Rect({ x, y }, { box, box }, 1.f, D3DCOLOR_RGBA(12, 12, 12, 255));
                Render::Draw->Text(label, x + box + 6.f, y - 2.f, LEFT, Render::Fonts::Tahombd, false,
                    D3DCOLOR_RGBA(205, 205, 205, 255));
            }

            static std::unordered_map<int, bool> prevMouse;
            const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool mousePressed = mouseDown && !prevMouse[VK_LBUTTON];
            prevMouse[VK_LBUTTON] = mouseDown;

            if (mousePressed && g_GameWindow) {
                POINT point{};
                if (GetCursorPos(&point) && ScreenToClient(g_GameWindow, &point)) {
                    const float mx = static_cast<float>(point.x);
                    const float my = static_cast<float>(point.y);
                    if (mx >= x && mx <= x + totalW && my >= y && my <= y + totalH)
                        state.value = !state.value;
                }
            }

            lua_pushboolean(L, state.value);
            return 1;
        }

        void RegisterKeyConstants(lua_State* L) {
            static const struct { const char* name; int value; } keys[] = {
                { "LBUTTON", VK_LBUTTON },
                { "RBUTTON", VK_RBUTTON },
                { "MBUTTON", VK_MBUTTON },
                { "XBUTTON1", VK_XBUTTON1 },
                { "XBUTTON2", VK_XBUTTON2 },
                { "SHIFT", VK_SHIFT },
                { "CONTROL", VK_CONTROL },
                { "ALT", VK_MENU },
                { "SPACE", VK_SPACE },
                { "TAB", VK_TAB },
                { "ESCAPE", VK_ESCAPE },
                { "INSERT", VK_INSERT },
                { "DELETE", VK_DELETE },
                { "HOME", VK_HOME },
                { "END", VK_END },
                { "LEFT", VK_LEFT },
                { "UP", VK_UP },
                { "RIGHT", VK_RIGHT },
                { "DOWN", VK_DOWN },
                { "F1", VK_F1 },
                { "F2", VK_F2 },
                { "F3", VK_F3 },
                { "F4", VK_F4 },
                { "F5", VK_F5 },
                { "F6", VK_F6 },
                { "F7", VK_F7 },
                { "F8", VK_F8 },
                { "F9", VK_F9 },
                { "F10", VK_F10 },
                { "F11", VK_F11 },
                { "F12", VK_F12 },
            };

            lua_newtable(L);
            for (const auto& key : keys) {
                lua_pushinteger(L, key.value);
                lua_setfield(L, -2, key.name);
            }
            lua_setglobal(L, "keys");
        }

        void RegisterTable(lua_State* L, const char* name, const luaL_Reg* funcs) {
            lua_newtable(L);
            luaL_setfuncs(L, funcs, 0);
            lua_setglobal(L, name);
        }

        // ------------------------------------------------------------------
        // game_data.* -- generated CS2 reference data (Backend/GameData/cs2_game_data.h)
        // ------------------------------------------------------------------
        void PushEventField(lua_State* L, const game_data::EventField& field) {
            lua_newtable(L);
            lua_pushstring(L, field.name);
            lua_setfield(L, -2, "name");
            lua_pushstring(L, field.type);
            lua_setfield(L, -2, "type");
        }

        void PushEvent(lua_State* L, const game_data::GameEvent& ev) {
            lua_newtable(L);
            lua_pushstring(L, ev.name);
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, static_cast<lua_Integer>(ev.id));
            lua_setfield(L, -2, "id");
            lua_pushboolean(L, ev.local ? 1 : 0);
            lua_setfield(L, -2, "local");
            lua_newtable(L);
            for (int i = 0; i < ev.field_count; ++i) {
                PushEventField(L, ev.fields[i]);
                lua_rawseti(L, -2, i + 1);
            }
            lua_setfield(L, -2, "fields");
        }

        int LuaGameDataBuildNumber(lua_State* L) {
            lua_pushinteger(L, game_data::k_build_number);
            return 1;
        }

        int LuaGameDataGetEvent(lua_State* L) {
            const char* name = luaL_checkstring(L, 1);
            const game_data::GameEvent* ev = game_data::find_event(name);
            if (!ev)
                lua_pushnil(L);
            else
                PushEvent(L, *ev);
            return 1;
        }

        int LuaGameDataGetEventById(lua_State* L) {
            const lua_Integer id = luaL_checkinteger(L, 1);
            const game_data::GameEvent* ev = game_data::find_event_by_id(static_cast<uint32_t>(id));
            if (!ev)
                lua_pushnil(L);
            else
                PushEvent(L, *ev);
            return 1;
        }

        int LuaGameDataEventNames(lua_State* L) {
            lua_newtable(L);
            for (int i = 0; i < game_data::k_game_events_count; ++i) {
                lua_pushstring(L, game_data::k_game_events[i].name);
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }

        int LuaGameDataGetModules(lua_State* L) {
            lua_newtable(L);
            int count = 0;
            for (const auto& mod : game_data::k_vtable_modules) {
                lua_pushstring(L, mod.module_name);
                lua_rawseti(L, -2, ++count);
            }
            return 1;
        }

        int LuaGameDataGetInterface(lua_State* L) {
            const char* module_name = luaL_checkstring(L, 1);
            const char* iface_name = luaL_checkstring(L, 2);
            const game_data::VtableInterface* iface = game_data::find_interface(module_name, iface_name);
            if (!iface) {
                lua_pushnil(L);
                return 1;
            }
            lua_newtable(L);
            lua_pushstring(L, iface->interface_name);
            lua_setfield(L, -2, "name");
            lua_pushstring(L, iface->rtti_class);
            lua_setfield(L, -2, "rtti_class");
            lua_pushstring(L, module_name);
            lua_setfield(L, -2, "module");
            lua_pushinteger(L, static_cast<lua_Integer>(iface->vtable_rva));
            lua_setfield(L, -2, "vtable_rva");
            lua_newtable(L);
            for (int i = 0; i < iface->method_count; ++i) {
                const game_data::VtableMethod& m = iface->methods[i];
                lua_newtable(L);
                lua_pushinteger(L, m.index);
                lua_setfield(L, -2, "index");
                if (m.name)
                    lua_pushstring(L, m.name);
                else
                    lua_pushnil(L);
                lua_setfield(L, -2, "name");
                lua_pushinteger(L, static_cast<lua_Integer>(m.rva));
                lua_setfield(L, -2, "rva");
                lua_rawseti(L, -2, m.index + 1);
            }
            lua_setfield(L, -2, "methods");
            return 1;
        }

        int LuaGameDataEntityClasses(lua_State* L) {
            lua_newtable(L);
            for (int i = 0; i < game_data::k_entity_classes_count; ++i) {
                const game_data::EntityClass& ec = game_data::k_entity_classes[i];
                lua_newtable(L);
                lua_pushstring(L, ec.classname);
                lua_setfield(L, -2, "classname");
                lua_pushinteger(L, ec.count);
                lua_setfield(L, -2, "count");
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }

        int LuaGameDataEntitySnapshot(lua_State* L) {
            lua_newtable(L);
            for (int i = 0; i < game_data::k_entity_snapshot_count; ++i) {
                const game_data::EntitySnapshot& ent = game_data::k_entity_snapshot[i];
                lua_newtable(L);
                lua_pushstring(L, ent.classname);
                lua_setfield(L, -2, "classname");
                lua_pushinteger(L, ent.index);
                lua_setfield(L, -2, "index");
                lua_pushinteger(L, ent.health);
                lua_setfield(L, -2, "health");
                lua_pushinteger(L, ent.max_health);
                lua_setfield(L, -2, "max_health");
                lua_newtable(L);
                lua_pushnumber(L, ent.origin[0]);
                lua_rawseti(L, -2, 1);
                lua_pushnumber(L, ent.origin[1]);
                lua_rawseti(L, -2, 2);
                lua_pushnumber(L, ent.origin[2]);
                lua_rawseti(L, -2, 3);
                lua_setfield(L, -2, "origin");
                lua_pushinteger(L, ent.team);
                lua_setfield(L, -2, "team");
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }

        int LuaGameDataGetProtoMessages(lua_State* L) {
            const char* module_name = luaL_checkstring(L, 1);
            const game_data::ProtoModule* mod = game_data::find_proto_module(module_name);
            if (!mod) {
                lua_pushnil(L);
                return 1;
            }
            lua_newtable(L);
            for (int i = 0; i < mod->message_count; ++i) {
                lua_pushstring(L, mod->messages[i].name);
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }

        int LuaGameDataGetProto(lua_State* L) {
            const char* module_name = luaL_checkstring(L, 1);
            const char* message_name = luaL_checkstring(L, 2);
            const game_data::ProtoMessage* msg = game_data::find_proto_message(module_name, message_name);
            if (!msg) {
                lua_pushnil(L);
                return 1;
            }
            lua_newtable(L);
            lua_pushstring(L, msg->name);
            lua_setfield(L, -2, "name");
            lua_pushstring(L, module_name);
            lua_setfield(L, -2, "module");
            lua_pushinteger(L, msg->size);
            lua_setfield(L, -2, "size");
            lua_pushinteger(L, msg->has_bits_offset);
            lua_setfield(L, -2, "has_bits_offset");
            lua_newtable(L);
            for (int i = 0; i < msg->field_count; ++i) {
                const game_data::ProtoField& f = msg->fields[i];
                if (!f.name)
                    continue; // dummy entry for empty messages
                lua_newtable(L);
                lua_pushstring(L, f.name);
                lua_setfield(L, -2, "name");
                lua_pushinteger(L, f.offset);
                lua_setfield(L, -2, "offset");
                lua_pushinteger(L, f.number);
                lua_setfield(L, -2, "number");
                lua_pushinteger(L, f.has_bit);
                lua_setfield(L, -2, "has_bit");
                lua_pushinteger(L, static_cast<lua_Integer>(f.type));
                lua_setfield(L, -2, "type");
                lua_pushstring(L, game_data::proto_type_name(f.type));
                lua_setfield(L, -2, "type_name");
                lua_pushinteger(L, static_cast<lua_Integer>(f.label));
                lua_setfield(L, -2, "label");
                lua_setfield(L, -2, f.name);
            }
            lua_setfield(L, -2, "fields");
            return 1;
        }

        int LuaGameDataGetPattern(lua_State* L) {
            const game_data::PatternEntry* entry = nullptr;
            if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
                entry = game_data::find_pattern(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
            else
                entry = game_data::find_pattern(luaL_checkstring(L, 1));
            if (!entry) {
                lua_pushnil(L);
                return 1;
            }
            lua_newtable(L);
            lua_pushstring(L, entry->name);
            lua_setfield(L, -2, "name");
            lua_pushstring(L, entry->module);
            lua_setfield(L, -2, "module");
            lua_pushstring(L, entry->resolve);
            lua_setfield(L, -2, "resolve");
            lua_pushstring(L, entry->pattern);
            lua_setfield(L, -2, "pattern");
            if (entry->prototype && entry->prototype[0]) {
                lua_pushstring(L, entry->prototype);
                lua_setfield(L, -2, "prototype");
            }
            lua_pushinteger(L, static_cast<lua_Integer>(entry->rva));
            lua_setfield(L, -2, "rva");
            return 1;
        }

        void RegisterApi(lua_State* L) {
            lua_register(L, "print", LuaPrint);
            lua_register(L, "error", LuaError);

            static const luaL_Reg configFuncs[] = {
                { "get_bool", LuaConfigGetBool },
                { "set_bool", LuaConfigSetBool },
                { "get_int", LuaConfigGetInt },
                { "set_int", LuaConfigSetInt },
                { "get_float", LuaConfigGetFloat },
                { "set_float", LuaConfigSetFloat },
                { "get_string", LuaConfigGetString },
                { "set_string", LuaConfigSetString },
                { "get_color", LuaConfigGetColor },
                { "set_color", LuaConfigSetColor },
                { "bind_active", LuaConfigBindActive },
                { "load", LuaConfigLoad },
                { "save", LuaConfigSave },
                { nullptr, nullptr }
            };

            static const luaL_Reg clientFuncs[] = {
                { "screen_size", LuaClientScreenSize },
                { "delta_time", LuaClientDeltaTime },
                { "realtime", LuaClientRealtime },
                { "in_match", LuaClientInMatch },
                { "menu_open", LuaClientMenuOpen },
                { "notify", LuaClientNotify },
                { "mouse_position", LuaClientMousePosition },
                { "script_dir", LuaClientScriptDir },
                { "has_callback", LuaClientHasCallback },
                { "set_event_callback", LuaClientSetCallback },
                { "clear_callbacks", LuaClientClearCallbacks },
                { nullptr, nullptr }
            };

            static const luaL_Reg entityFuncs[] = {
                { "get_local", LuaEntityGetLocal },
                { "get_players", LuaEntityGetPlayers },
                { "get_spectators", LuaEntityGetSpectators },
                { "get_by_index", LuaEntityGetByIndex },
                { "count", LuaEntityCount },
                { nullptr, nullptr }
            };

            static const luaL_Reg inputFuncs[] = {
                { "key_down", LuaInputKeyDown },
                { "key_pressed", LuaInputKeyPressed },
                { nullptr, nullptr }
            };

            static const luaL_Reg renderFuncs[] = {
                { "line", LuaRenderLine },
                { "rect", LuaRenderRect },
                { "filled_rect", LuaRenderFilledRect },
                { "bordered_rect", LuaRenderBorderedRect },
                { "gradient", LuaRenderGradient },
                { "triangle", LuaRenderTriangle },
                { "text", LuaRenderText },
                { "circle", LuaRenderCircle },
                { "measure_text", LuaRenderMeasureText },
                { "world_to_screen", LuaRenderWorldToScreen },
                { nullptr, nullptr }
            };

            static const luaL_Reg utilsFuncs[] = {
                { "distance", LuaUtilsDistance },
                { "clamp", LuaUtilsClamp },
                { "lerp", LuaUtilsLerp },
                { nullptr, nullptr }
            };

            static const luaL_Reg mathFuncs[] = {
                { "deg2rad", LuaMathDeg2Rad },
                { "rad2deg", LuaMathRad2Deg },
                { "normalize_yaw", LuaMathNormalizeYaw },
                { "angle_diff", LuaMathAngleDiff },
                { nullptr, nullptr }
            };

            static const luaL_Reg uiFuncs[] = {
                { "checkbox", LuaUiCheckbox },
                { "label", LuaUiLabel },
                { "button", LuaUiButton },
                { "get_bool", LuaUiGetBool },
                { "set_bool", LuaUiSetBool },
                { "render_checkbox", LuaUiRenderCheckbox },
                { nullptr, nullptr }
            };

            static const luaL_Reg gameDataFuncs[] = {
                { "build_number", LuaGameDataBuildNumber },
                { "get_event", LuaGameDataGetEvent },
                { "get_event_by_id", LuaGameDataGetEventById },
                { "event_names", LuaGameDataEventNames },
                { "get_modules", LuaGameDataGetModules },
                { "get_interface", LuaGameDataGetInterface },
                { "entity_classes", LuaGameDataEntityClasses },
                { "entity_snapshot", LuaGameDataEntitySnapshot },
                { "get_proto_messages", LuaGameDataGetProtoMessages },
                { "get_proto", LuaGameDataGetProto },
                { "get_pattern", LuaGameDataGetPattern },
                { nullptr, nullptr }
            };

            RegisterTable(L, "config", configFuncs);
            RegisterTable(L, "client", clientFuncs);
            RegisterTable(L, "entity", entityFuncs);
            RegisterTable(L, "input", inputFuncs);
            RegisterTable(L, "render", renderFuncs);
            RegisterTable(L, "utils", utilsFuncs);
            RegisterTable(L, "math", mathFuncs);
            RegisterTable(L, "ui", uiFuncs);
            RegisterTable(L, "game_data", gameDataFuncs);

            RegisterKeyConstants(L);

            lua_pushinteger(L, 1);
            lua_setglobal(L, "TEAM_SPEC");
            lua_pushinteger(L, 2);
            lua_setglobal(L, "TEAM_T");
            lua_pushinteger(L, 3);
            lua_setglobal(L, "TEAM_CT");

            lua_pushinteger(L, LEFT);
            lua_setglobal(L, "TEXT_LEFT");
            lua_pushinteger(L, CENTER);
            lua_setglobal(L, "TEXT_CENTER");
            lua_pushinteger(L, RIGHT);
            lua_setglobal(L, "TEXT_RIGHT");
        }

        bool ExecuteLoadedChunk(std::string& errorOut) {
            const int status = lua_pcall(g_lua, 0, LUA_MULTRET, 0);
            if (status == LUA_OK)
                return true;

            const char* err = lua_tostring(g_lua, -1);
            errorOut = err ? err : "lua runtime error";
            SetError(errorOut.c_str());
            lua_pop(g_lua, 1);
            return false;
        }
    }

    void Initialize() {
        if (g_lua)
            return;

        g_lua = luaL_newstate();
        if (!g_lua) {
            SetError("failed to create lua state");
            return;
        }

        luaL_openlibs(g_lua);
        RegisterApi(g_lua);
    }

    void Shutdown() {
        ClearCallbacks();
        if (!g_lua)
            return;

        lua_close(g_lua);
        g_lua = nullptr;
    }

    bool RunString(const char* source, const char* chunkName, std::string& errorOut) {
        errorOut.clear();
        if (!source || !source[0]) {
            errorOut = "empty script";
            SetError(errorOut.c_str());
            return false;
        }

        Initialize();
        if (!g_lua) {
            errorOut = g_lastError;
            return false;
        }

        ClearCallbacks();

        const int status = luaL_loadbuffer(g_lua, source, strlen(source), chunkName ? chunkName : "script");
        if (status != LUA_OK) {
            const char* err = lua_tostring(g_lua, -1);
            errorOut = err ? err : "lua load error";
            SetError(errorOut.c_str());
            lua_pop(g_lua, 1);
            return false;
        }

        return ExecuteLoadedChunk(errorOut);
    }

    bool RunFile(const char* filePath, std::string& errorOut) {
        errorOut.clear();
        if (!filePath || !filePath[0]) {
            errorOut = "invalid script path";
            SetError(errorOut.c_str());
            return false;
        }

        Initialize();
        if (!g_lua) {
            errorOut = g_lastError;
            return false;
        }

        ClearCallbacks();

        const int status = luaL_loadfile(g_lua, filePath);
        if (status != LUA_OK) {
            const char* err = lua_tostring(g_lua, -1);
            errorOut = err ? err : "lua load error";
            SetError(errorOut.c_str());
            lua_pop(g_lua, 1);
            return false;
        }

        return ExecuteLoadedChunk(errorOut);
    }

    void TickFrame() {
        if (Misc::Utilities)
            g_realtime += static_cast<double>(Misc::Utilities->GetDeltaTime());
    }

    void RunEvent(const char* eventName) {
        if (!g_lua || !eventName)
            return;

        const bool isMenu = std::strcmp(eventName, "menu") == 0;
        if (isMenu)
            g_uiMenuDrawing = true;

        const auto it = g_callbacks.find(eventName);
        if (it != g_callbacks.end())
            CallFunction(it->second);

        if (isMenu)
            g_uiMenuDrawing = false;
    }

    bool HasCallback(const char* eventName) {
        if (!eventName)
            return false;
        return g_callbacks.find(eventName) != g_callbacks.end();
    }

    const char* GetLastError() {
        return g_lastError.c_str();
    }
}
