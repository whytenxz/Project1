#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace players {
    struct PlayerInfo {
        int index = 0;
        std::string name;
        int team = 0;
        int health = 0;
        int armor = 0;
        bool alive = false;
        bool isLocal = false;
        bool hasHelmet = false;
        bool hasDefuser = false;
        std::string weapon;
        int distance = 0;
        uint64_t steamId64 = 0;
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
        bool hasPosition = false;
        float velX = 0.f;
        float velY = 0.f;
        float velZ = 0.f;
        bool hasVelocity = false;
    };

    void Gather(std::vector<PlayerInfo>& out);
    void GatherSpectators(std::vector<std::string>& out);
}
