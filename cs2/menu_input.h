#pragma once

class i_csgo_input;

namespace menu_input {
    void PrepareBeforeCreateMove(i_csgo_input* input);
    void CleanupAfterCreateMove(i_csgo_input* input);
    void RestoreWhenMenuClosed(i_csgo_input* input);
}
