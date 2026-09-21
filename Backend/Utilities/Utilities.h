#pragma once
#include "../Globalincludes.h"
#include "../Misc/lazy_ptr.hpp"
#include "../ValveSDK/Vector.h"

namespace Misc {
    class CUtilities
    {
    public:
        void Game_Msg(const char* msg, ...);
        void AdvanceFrame();
        float GetDeltaTime();
        bool WorldToScreen(const Vector& origin, Vector& screen);
    };

    inline lazy_ptr<CUtilities> Utilities;
}
