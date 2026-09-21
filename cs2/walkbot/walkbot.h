#pragma once
#include <cstdint>

class c_user_cmd;
class i_csgo_input;

namespace walkbot {
    bool IsActive();
    void Process(c_user_cmd* cmd, i_csgo_input* input, uintptr_t localPawn);
}
