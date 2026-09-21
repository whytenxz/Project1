#pragma once
#include "../../Misc/lazy_ptr.hpp"
#include <string>
#include <vector>

namespace Features
{
    struct EventLog_t
    {
        float lifetime = 4.f;
        int alpha = 255;
        std::string text;
    };

    class CEventLogger
    {
    public:
        void AddLog(const char* str, ...);
        void Draw();
    private:
        std::vector<EventLog_t> logs;
    };

    inline lazy_ptr<CEventLogger> EventLogger;
}
