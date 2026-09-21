#pragma once

#include <cstdint>

namespace thirdperson {
    void Run();
    bool IsActive();
    void EnsurePatch(bool enabled);
    void SetInputThirdPerson(bool active);
    void SetObserverChaseDistance(uintptr_t pawn, float distance);
}
