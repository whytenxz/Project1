#include "thirdperson.h"
#include "tp_glitch.h"
#include "memory.h"
#include "offsets.h"
#include "../Backend/Config/Config.h"
#include "../nerv/valve/interfaces/interfaces.hpp"

namespace thirdperson {
    namespace {
        uintptr_t g_resetAddr = 0;
        bool g_patchApplied = false;

        uintptr_t GetResetAddr() {
            if (!g_resetAddr) {
                g_resetAddr = memory::FindPattern("client.dll", offsets::signatures::third_person_reset);
            }
            return g_resetAddr;
        }

        void SetPatch(bool enabled) {
            const uintptr_t addr = GetResetAddr();
            if (!addr)
                return;

            DWORD oldProtect = 0;
            if (!VirtualProtect(reinterpret_cast<void*>(addr), 16, PAGE_EXECUTE_READWRITE, &oldProtect))
                return;

            *reinterpret_cast<uint8_t*>(addr + 7) = enabled ? 0xEB : 0x75;

            DWORD ignored = 0;
            VirtualProtect(reinterpret_cast<void*>(addr), 16, oldProtect, &ignored);
        }

        void UpdatePatchState(bool enabled) {
            if (enabled != g_patchApplied) {
                SetPatch(enabled);
                g_patchApplied = enabled;
            }
        }

        uintptr_t GetInputAddr() {
            if (g_interfaces && g_interfaces->m_csgo_input)
                return reinterpret_cast<uintptr_t>(g_interfaces->m_csgo_input);

            const uintptr_t client = memory::GetModuleBase("client.dll");
            if (!client)
                return 0;

            return memory::Read<uintptr_t>(client + offsets::client_dll::dwCSGOInput);
        }

        bool IsThirdPersonActive(CConfig* cfg) {
            if (!cfg->b["misc_third_person"])
                return false;

            return cfg->IsBindActive("misc_third_person_key");
        }
    }

    bool IsActive() {
        return IsThirdPersonActive(CConfig::get());
    }

    void EnsurePatch(bool enabled) {
        UpdatePatchState(enabled);
    }

    void SetInputThirdPerson(bool active) {
        const uintptr_t input = GetInputAddr();
        if (input)
            memory::Write<bool>(input + offsets::csgo_input::third_person, active);
    }

    void SetObserverChaseDistance(uintptr_t pawn, float distance) {
        if (!pawn)
            return;

        const uintptr_t observerServices = memory::Read<uintptr_t>(pawn + schema::C_BasePlayerPawn::m_pObserverServices);
        if (observerServices)
            memory::Write<float>(observerServices + schema::CPlayer_ObserverServices::m_flObserverChaseDistance, distance);
    }

    void Run() {
        CConfig* cfg = CConfig::get();
        if (tp_glitch::IsViewGlitchActive())
            return;

        const bool featureEnabled = cfg->b["misc_third_person"];
        const bool active = IsThirdPersonActive(cfg);

        UpdatePatchState(featureEnabled);

        if (!featureEnabled)
            return;

        SetInputThirdPerson(active);
    }
}
