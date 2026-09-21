#include "includes.h"
#include "Frontend/Menu/Menu.h"
#include "Frontend/Renderer/Renderer.h"
#include "Backend/Config/Config.h"
#include "Backend/Config/Settings.h"
#include "Backend/Features/Visuals/EventLogger.h"
#include "cs2/esp.h"
#include "cs2/legitbot.h"
#include "cs2/thirdperson.h"
#include "cs2/tp_glitch.h"
#include "cs2/tracers.h"
#include "cs2/night_mode.h"
#include "cs2/chams.h"
#include "cs2/game_state.h"
#include "cs2/memory.h"
#include "cs2/offsets.h"
#include "cs2/streamer_mode.h"
#include "Backend/Lua/LuaEngine.h"
#include "Backend/Utilities/Utilities.h"
#include "Frontend/Steam/SteamAvatars.h"
#include "nerv/nerv_init.hpp"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static Present oPresent = nullptr;
static HWND window = nullptr;
static WNDPROC oWndProc = nullptr;
static ID3D11Device* pDevice = nullptr;
static ID3D11DeviceContext* pContext = nullptr;
static ID3D11RenderTargetView* mainRenderTargetView = nullptr;

static void InitImGui()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(pDevice, pContext);
    Render::Draw->Init(pDevice, pContext);
}

static void UpdateMenuCursor(bool menuOpen, bool justOpened)
{
    if (!menuOpen)
        return;

    ClipCursor(nullptr);
    ReleaseCapture();

    if (justOpened) {
        while (ShowCursor(TRUE) < 0) {}
        if (window)
            SetForegroundWindow(window);
    }

    SetCursor(LoadCursor(nullptr, IDC_ARROW));
}

static bool BlockGameInputWhenMenuOpen(UINT uMsg)
{
    switch (uMsg) {
    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
        return true;
    default:
        return false;
    }
}

static LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (CMenu::get()->IsMenuOpened()) {
        if (uMsg == WM_SETCURSOR) {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        }

        if (uMsg == WM_MOUSEWHEEL) {
            if (IdaLovesMe::Globals::Gui_Ctx) {
                IdaLovesMe::Globals::Gui_Ctx->MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            }
        }

        if (BlockGameInputWhenMenuOpen(uMsg))
            return 0;
    }

    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

static HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    static bool init = false;

    if (!init)
    {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice))))
        {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd{};
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;
            g_GameWindow = window;

            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&pBackBuffer));
            pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
            pBackBuffer->Release();

            oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
            InitImGui();
            chams::Initialize(pDevice, pContext);
            init = true;
        }
        else
        {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    CMenu::get()->Initialize();

    CConfig* cfg = CConfig::get();
    const bool menuOpen = CMenu::get()->IsMenuOpened();
    bool menuJustOpened = false;

    if (cfg->i["menu_key"] && (GetAsyncKeyState(cfg->i["menu_key"]) & 1)) {
        CMenu::get()->SetMenuOpened(!menuOpen);
        menuJustOpened = !menuOpen;
    }

    const bool menuOpenNow = CMenu::get()->IsMenuOpened();
    UpdateMenuCursor(menuOpenNow, menuJustOpened);

    // Rebuild the font atlas before NewFrame() if a DPI change was requested mid-frame.
    // Doing this while the atlas is locked (between NewFrame and Render) asserts in imgui.
    Render::Draw->ProcessDpiReload();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    Misc::Utilities->AdvanceFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = false;

    if (menuOpenNow)
        SteamAvatars::Tick();

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    Render::Draw->SetDrawList(drawList);
    Render::Draw->Reset();

    const int screenW = static_cast<int>(io.DisplaySize.x);
    const int screenH = static_cast<int>(io.DisplaySize.y);
    const uintptr_t client = memory::GetModuleBase("client.dll");
    const bool inMatch = game_state::IsInMatch();
    const bool streamerMode = cfg->b["misc_hide_from_obs"];

    streamer_mode::Update(streamerMode);

    if (!CMenu::get()->m_bIsBanned) {
        lua_engine::TickFrame();
        lua_engine::RunEvent("run");

        if (!streamer_mode::ShouldHideOverlays()) {
            esp::Run(drawList, screenW, screenH);
            esp::DrawKeybindList(screenW, screenH);

            chams::Update();

            if (inMatch) {
                tracers::Update(client);
                tracers::Draw(drawList, screenW, screenH);

                if (!menuOpenNow) {
                    legitbot::Draw(drawList, screenW, screenH);
                    legitbot::Run();
                }

                thirdperson::Run();
                tp_glitch::ApplyView(client);
            }

            night_mode::Run();
        } else if (inMatch && !menuOpenNow) {
            legitbot::Run();
            thirdperson::Run();
            tp_glitch::ApplyView(client);
        }
    }

    if (!Settings->UnloadCheat)
    {
        CMenu::get()->Draw();
        if (!CMenu::get()->m_bIsBanned && !streamer_mode::ShouldHideOverlays()) {
            lua_engine::RunEvent("paint");
            Features::EventLogger->Draw();
        }
    }

    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}

static DWORD WINAPI MainThread(LPVOID)
{
    bool initHook = false;
    do
    {
        if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
        {
            kiero::bind(8, reinterpret_cast<void**>(&oPresent), hkPresent);
            initHook = true;
        }
    } while (!initHook);

    while (!nerv::initialize())
        Sleep(200);

    return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        chams::Shutdown();
        SteamAvatars::Shutdown();
        lua_engine::Shutdown();
        kiero::shutdown();
        break;
    }
    return TRUE;
}
