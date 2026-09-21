#pragma once
#include "../../../sdk/vfunc/vfunc.hpp"

class i_engine_client {
public:
    bool is_in_game() {
        return vmt::call_virtual<bool>(this, 38);
    }

    bool is_connected() {
        return vmt::call_virtual<bool>(this, 39);
    }
};
