// Spatial Audio - pathways
//
// A pathway is a shape a source travels along: line, circle, triangle,
// square, figure eight or spiral. The shape is drawn flat, then scaled,
// tilted, rotated and moved to its centre point in the room.
//
// Movement is measured in "passes". One pass = one trip along the whole
// shape. Speed is set as seconds per pass, or locked to tempo as bars per
// pass.
#pragma once

#include "Vec3.h"
#include <algorithm>
#include <cmath>

namespace spatial
{

enum class PathShape
{
    none = 0,   // source stays still at the centre point
    line,
    circle,
    triangle,
    square,
    figureEight,
    spiral
};

enum class PathDirection
{
    forward = 0,
    reverse,
    backAndForth
};

struct PathParams
{
    PathShape shape = PathShape::circle;
    PathDirection direction = PathDirection::forward;

    Vec3 centre { 0.0f, 0.0f, 1.2f }; // metres; z = height above floor
    float size = 1.5f;                // metres (radius, or half the length of a line)
    float stretch = 1.0f;             // squashes or stretches the shape front-to-back (ellipses etc.)
    float tiltDegrees = 0.0f;         // 0 = flat like a tabletop, 90 = standing up like a wheel
    float rotationDegrees = 0.0f;     // turns the whole shape around the vertical axis
    float startOffset = 0.0f;         // 0..1, where along the path the source starts

    // Spiral only
    float spiralTurns = 3.0f;         // how many loops
    float spiralInner = 0.1f;         // inner radius as a fraction of size (0..1)
    float spiralRise = 0.0f;          // metres of height change from outside to inside (+ up, - down)
};

struct SpeedParams
{
    bool tempoSync = false;
    float secondsPerPass = 8.0f;  // used when tempoSync is off
    float bpm = 120.0f;           // used when tempoSync is on
    float barsPerPass = 4.0f;     // used when tempoSync is on
    float beatsPerBar = 4.0f;

    float effectiveSecondsPerPass() const
    {
        const float s = tempoSync ? barsPerPass * beatsPerBar * 60.0f / std::max (bpm, 1.0f)
                                  : secondsPerPass;
        return std::max (s, 0.05f);
    }
};

namespace detail
{
    inline Vec3 lerp (const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

    // Walks around a closed polygon of equal-length sides.
    inline Vec3 polygonPoint (const Vec3* corners, int numCorners, float t)
    {
        const float scaled = t * (float) numCorners;
        int edge = (int) std::floor (scaled);
        float within = scaled - (float) edge;
        edge = ((edge % numCorners) + numCorners) % numCorners;
        return lerp (corners[edge], corners[(edge + 1) % numCorners], within);
    }
}

// Point on the flat, unit-sized shape. x/y are multiplied by size later;
// z is already in metres (used by the spiral's rise).
inline Vec3 localShapePoint (const PathParams& p, float t)
{
    using namespace detail;

    switch (p.shape)
    {
        case PathShape::none:
            return {};

        case PathShape::line:
            return lerp ({ -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, t);

        case PathShape::circle:
        {
            // Starts at the front and travels clockwise seen from above.
            const float angle = pi * 0.5f - twoPi * t;
            return { std::cos (angle), std::sin (angle), 0.0f };
        }

        case PathShape::triangle:
        {
            const Vec3 corners[3] = {
                { 0.0f, 1.0f, 0.0f },
                { std::cos (-pi / 6.0f), std::sin (-pi / 6.0f), 0.0f },
                { std::cos (pi + pi / 6.0f), std::sin (pi + pi / 6.0f), 0.0f }
            };
            return polygonPoint (corners, 3, t);
        }

        case PathShape::square:
        {
            const Vec3 corners[4] = {
                { -1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f },
                { 1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f }
            };
            return polygonPoint (corners, 4, t);
        }

        case PathShape::figureEight:
        {
            const float angle = twoPi * t;
            return { std::sin (angle), std::sin (2.0f * angle) * 0.5f, 0.0f };
        }

        case PathShape::spiral:
        {
            // Starts on the outside and winds inward (reverse = outward).
            const float inner = std::clamp (p.spiralInner, 0.0f, 1.0f);
            const float radius = 1.0f + (inner - 1.0f) * t;
            const float angle = pi * 0.5f - twoPi * std::max (p.spiralTurns, 0.1f) * t;
            return { radius * std::cos (angle), radius * std::sin (angle), p.spiralRise * t };
        }
    }

    return {};
}

// Point in the room, for a position t (0..1) along the path.
inline Vec3 pathPoint (const PathParams& p, float t)
{
    Vec3 local = localShapePoint (p, t);

    // Scale (x/y only; spiral height is already in metres)
    local.x *= p.size;
    local.y *= p.size * p.stretch;

    // Tilt around the left-right axis
    const float tilt = degreesToRadians (p.tiltDegrees);
    const float ct = std::cos (tilt), st = std::sin (tilt);
    Vec3 tilted { local.x, local.y * ct - local.z * st, local.y * st + local.z * ct };

    // Rotate around the vertical axis
    const float rot = degreesToRadians (-p.rotationDegrees); // positive = clockwise from above
    const float cr = std::cos (rot), sr = std::sin (rot);
    Vec3 rotated { tilted.x * cr - tilted.y * sr, tilted.x * sr + tilted.y * cr, tilted.z };

    return rotated + p.centre;
}

// Turns a running count of passes (0, 0.5, 1.7, ...) into a position 0..1
// along the path, applying the direction and start offset.
inline float passesToPathPosition (double passes, PathDirection direction, float startOffset)
{
    if (direction == PathDirection::backAndForth)
    {
        // One pass there, one pass back.
        double f = std::fmod (passes + (double) startOffset, 2.0);
        if (f < 0.0) f += 2.0;
        return (float) (f < 1.0 ? f : 2.0 - f);
    }

    double f = std::fmod (passes + (double) startOffset, 1.0);
    if (f < 0.0) f += 1.0;
    return direction == PathDirection::reverse ? (float) (1.0 - f) : (float) f;
}

} // namespace spatial
