// Spatial Audio - shared maths
// Coordinates are in metres:
//   x = left (-) / right (+) from the room centre
//   y = back (-) / front (+) from the room centre
//   z = height above the floor
#pragma once

#include <cmath>

namespace spatial
{

struct Vec3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3 (float xIn, float yIn, float zIn) : x (xIn), y (yIn), z (zIn) {}

    Vec3 operator+ (const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const       { return { x * s, y * s, z * s }; }

    float dot (const Vec3& o) const      { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross (const Vec3& o) const     { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
    float lengthSquared() const          { return dot (*this); }
    float length() const                 { return std::sqrt (lengthSquared()); }

    Vec3 normalised() const
    {
        const float len = length();
        return len > 1.0e-9f ? *this * (1.0f / len) : Vec3 {};
    }
};

constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

inline float degreesToRadians (float d) { return d * (pi / 180.0f); }

} // namespace spatial
