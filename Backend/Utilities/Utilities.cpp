#include "Utilities.h"
#include "../Features/Visuals/EventLogger.h"
#include "../../cs2/memory.h"
#include "../../cs2/math.h"
#include "../../cs2/offsets.h"

#include <chrono>

using namespace Misc;

static std::chrono::steady_clock::time_point g_LastFrame = std::chrono::steady_clock::now();
static float g_FrameDelta = 1.f / 60.f;

void CUtilities::AdvanceFrame()
{
    const auto now = std::chrono::steady_clock::now();
    g_FrameDelta = std::chrono::duration<float>(now - g_LastFrame).count();
    g_LastFrame = now;

    if (g_FrameDelta <= 0.f)
        g_FrameDelta = 1.f / 60.f;
    else if (g_FrameDelta > 0.1f)
        g_FrameDelta = 0.1f;
}

float CUtilities::GetDeltaTime()
{
    return g_FrameDelta;
}

void CUtilities::Game_Msg(const char* msg, ...)
{
    if (!msg)
        return;

    char buffer[2048]{};
    va_list list;
    va_start(list, msg);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, msg, list);
    va_end(list);

    Features::EventLogger->AddLog(buffer);
}

bool CUtilities::WorldToScreen(const Vector& origin, Vector& screen)
{
    const uintptr_t client = memory::GetModuleBase("client.dll");
    if (!client)
        return false;

    float matrix[16]{};
    for (int i = 0; i < 16; ++i)
        matrix[i] = memory::Read<float>(client + offsets::client_dll::dwViewMatrix + i * sizeof(float));

    const uintptr_t engine = memory::GetModuleBase("engine2.dll");
    const int width = engine ? memory::Read<int>(engine + offsets::engine2_dll::dwWindowWidth) : 1920;
    const int height = engine ? memory::Read<int>(engine + offsets::engine2_dll::dwWindowHeight) : 1080;

    Vector2 out{};
    const Vector3 world{ origin.x, origin.y, origin.z };
    if (!::WorldToScreen(world, out, matrix, width, height))
        return false;

    screen.x = out.x;
    screen.y = out.y;
    screen.z = 0.f;
    return true;
}
