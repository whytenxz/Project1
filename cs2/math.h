#pragma once
#include <cmath>

struct Vector3 {
    float x, y, z;

    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
};

struct Vector2 {
    float x, y;
};

inline bool WorldToScreen(const Vector3& world, Vector2& screen, const float* matrix, int width, int height) {
    const float w = matrix[12] * world.x + matrix[13] * world.y + matrix[14] * world.z + matrix[15];
    if (w < 0.001f)
        return false;

    float x = matrix[0] * world.x + matrix[1] * world.y + matrix[2] * world.z + matrix[3];
    float y = matrix[4] * world.x + matrix[5] * world.y + matrix[6] * world.z + matrix[7];

    const float invW = 1.0f / w;
    x *= invW;
    y *= invW;

    screen.x = (width * 0.5f) + (x * width * 0.5f);
    screen.y = (height * 0.5f) - (y * height * 0.5f);
    return true;
}
