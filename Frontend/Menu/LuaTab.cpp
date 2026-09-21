#include "LuaTab.h"
#include "../../Backend/Lua/LuaEngine.h"
#include "../Framework/MenuFramework.h"

#include <ShlObj.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

using namespace IdaLovesMe;

namespace {
    std::string GetScriptsDirectory() {
        char base[MAX_PATH]{};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, base)))
            GetCurrentDirectoryA(MAX_PATH, base);

        std::filesystem::path dir = std::filesystem::path(base) / "gamesense-cs2" / "scripts";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir.string();
    }

    void RefreshScriptList(std::vector<std::string>& scripts) {
        scripts.clear();
        const std::string dir = GetScriptsDirectory();
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() == ".lua")
                scripts.push_back(entry.path().filename().string());
        }
        std::sort(scripts.begin(), scripts.end());
    }

    bool LoadScriptFile(const std::string& fileName, char* buffer, size_t bufferSize) {
        if (!buffer || bufferSize < 2)
            return false;

        const std::filesystem::path path = std::filesystem::path(GetScriptsDirectory()) / fileName;
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;

        std::ostringstream ss;
        ss << file.rdbuf();
        const std::string content = ss.str();
        if (content.size() >= bufferSize)
            return false;

        memcpy(buffer, content.c_str(), content.size() + 1);
        return true;
    }

    bool SaveScriptFile(const std::string& fileName, const char* buffer) {
        if (!fileName.empty() && fileName.find("..") != std::string::npos)
            return false;

        std::string name = fileName;
        if (name.empty())
            name = "untitled.lua";
        if (name.find(".lua") == std::string::npos)
            name += ".lua";

        const std::filesystem::path path = std::filesystem::path(GetScriptsDirectory()) / name;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;

        if (buffer)
            file << buffer;
        return true;
    }

    float ChildHeightUnscaled(float reserveTop, float reserveBottom) {
        const Vec2 size = ui::GetWindowSize();
        const float height = size.y - ui::Scale(reserveTop + reserveBottom);
        return (std::max)(ui::Scale(48.f), height) / ui::GetDpiScale();
    }
}

void LuaTab::Draw() {
    static std::vector<std::string> scripts;
    static int selectedIndex = -1;
    static char scriptName[64] = "untitled.lua";
    static char editorBuffer[16384]{};
    static char statusText[256] = "Ready.";
    static bool listInitialized = false;
    static bool layoutInitialized = false;

    if (!layoutInitialized) {
        if (Globals::Gui_Ctx)
            ui::ResetChildLayouts(Globals::Gui_Ctx->ChildLayoutVersion + 1);
        layoutInitialized = true;
    }

    if (!listInitialized) {
        RefreshScriptList(scripts);
        listInitialized = true;
    }

    ui::BeginChild("Scripts#Lua", { Vec2(0, 0), Vec2(3, 10) });
    {
        if (ui::Button("Refresh")) {
            RefreshScriptList(scripts);
            strcpy_s(statusText, "Refreshed script list.");
        }

        const float listHeight = ChildHeightUnscaled(34.f, 158.f);
        if (ui::BeginListbox("LuaScriptsList#Lua", Vec2(0.f, listHeight))) {
            for (int i = 0; i < static_cast<int>(scripts.size()); ++i) {
                if (ui::Selectable(scripts[i].c_str(), selectedIndex == i)) {
                    selectedIndex = i;
                    strcpy_s(scriptName, scripts[i].c_str());
                    if (LoadScriptFile(scripts[i], editorBuffer, sizeof(editorBuffer)))
                        strcpy_s(statusText, "Loaded script.");
                    else
                        strcpy_s(statusText, "Failed to load script.");
                }
            }
        }
        ui::EndListbox();

        ui::Label("Script name");
        ui::InputText("lua_script_name#Lua", scriptName, NULL, sizeof(scriptName), "name.lua");

        if (ui::Button("New")) {
            selectedIndex = -1;
            strcpy_s(scriptName, "untitled.lua");
            strcpy_s(editorBuffer, R"(-- soda.lua style example
client.set_event_callback("menu", function()
    ui.label("example.lua", true)
    ui.checkbox("example_enabled", "Enabled", true)
    ui.checkbox("example_overlay", "Show overlay", true)
end)

client.set_event_callback("paint", function()
    if not ui.get_bool("example_enabled") then return end
    if not client.in_match() or client.menu_open() then return end

    if ui.get_bool("example_overlay") then
        local w, h = client.screen_size()
        render.text("example overlay", 12, h - 28, 255, 255, 255, 220, TEXT_LEFT)
    end
end)

print("example loaded")
)");
            strcpy_s(statusText, "New example script.");
        }

        if (ui::Button("Load")) {
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(scripts.size())) {
                if (LoadScriptFile(scripts[selectedIndex], editorBuffer, sizeof(editorBuffer)))
                    strcpy_s(statusText, "Loaded script.");
                else
                    strcpy_s(statusText, "Failed to load script.");
            } else {
                strcpy_s(statusText, "Select a script first.");
            }
        }

        if (ui::Button("Save")) {
            if (SaveScriptFile(scriptName, editorBuffer)) {
                RefreshScriptList(scripts);
                strcpy_s(statusText, "Saved script.");
            } else {
                strcpy_s(statusText, "Failed to save script.");
            }
        }

        if (ui::Button("Run")) {
            if (scriptName[0] != '\0')
                LoadScriptFile(scriptName, editorBuffer, sizeof(editorBuffer));

            std::string error;
            if (lua_engine::RunString(editorBuffer, scriptName, error))
                strcpy_s(statusText, "Script executed.");
            else
                snprintf(statusText, sizeof(statusText), "Error: %s", error.c_str());
        }
    }
    ui::EndChild();

    ui::BeginChild("Options#Lua", { Vec2(6, 0), Vec2(3, 5) });
    {
        ui::Label("Script options");
        if (lua_engine::HasCallback("menu"))
            lua_engine::RunEvent("menu");
        else
            ui::Label("Run a script to see its options here.", true);
    }
    ui::EndChild();

    ui::BeginChild("Editor#Lua", { Vec2(6, 5), Vec2(3, 5) });
    {
        ui::Label("Lua editor");
        ui::Label(statusText, true);
        const float editorHeight = ChildHeightUnscaled(52.f, 10.f);
        ui::InputTextMultiline("lua_editor#Lua", editorBuffer, sizeof(editorBuffer), Vec2(0.f, editorHeight));
    }
    ui::EndChild();
}
