#pragma once
#include <string>

namespace Spotify {
    bool IsRunning();
    bool IsPlaying();
    std::string GetCurrentTrack();
    void PlayPause();
    void NextTrack();
    void PrevTrack();
    void Update();
}
