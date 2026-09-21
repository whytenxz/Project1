#pragma once
#include <string>

namespace lua_engine {
    void Initialize();
    void Shutdown();
    bool RunString(const char* source, const char* chunkName, std::string& errorOut);
    bool RunFile(const char* filePath, std::string& errorOut);
    void TickFrame();
    void RunEvent(const char* eventName);
    bool HasCallback(const char* eventName);
    const char* GetLastError();
}
