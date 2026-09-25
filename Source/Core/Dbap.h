// Spatial Audio - DBAP (distance-based amplitude panning)
//
// Every speaker gets a gain based on its distance to the source. Closer
// speakers are louder. The gains are normalised so the total power stays
// constant wherever the source is. This works with any speaker layout,
// regular or irregular.
//
// Reference: Lossius, Baltazar & de la Hogue, "DBAP - Distance-Based
// Amplitude Panning", ICMC 2009.
#pragma once

#include "Vec3.h"
#include <cmath>
#include <cstddef>

namespace spatial
{

struct DbapSettings
{
    // How quickly level falls off with distance, in dB per doubling of
    // distance. 6 dB is the physical inverse-distance law. Higher values
    // make the sound more focused on the nearest speakers.
    float rolloffDb = 6.0f;

    // Spatial blur in metres. Stops a single speaker taking everything when
    // the source sits right on top of it, and softens the image overall.
    float blur = 0.2f;
};

// Calculates one gain per speaker. Only speakers whose `include` flag is true
// take part; the others get a gain of 0. `include` may be nullptr (all in).
// Returns the number of speakers that took part.
inline std::size_t computeDbapGains (const Vec3& source,
                                     const Vec3* speakerPositions,
                                     const bool* include,
                                     std::size_t numSpeakers,
                                     const DbapSettings& settings,
                                     float* gainsOut)
{
    const float a = settings.rolloffDb / (20.0f * std::log10 (2.0f));
    const float blurSq = settings.blur * settings.blur;

    float sumSquares = 0.0f;
    std::size_t used = 0;

    for (std::size_t i = 0; i < numSpeakers; ++i)
    {
        if (include != nullptr && ! include[i])
        {
            gainsOut[i] = 0.0f;
            continue;
        }

        const float distSq = (speakerPositions[i] - source).lengthSquared() + blurSq;
        const float dist = std::sqrt (distSq > 1.0e-12f ? distSq : 1.0e-12f);
        const float v = 1.0f / std::pow (dist, a);
        gainsOut[i] = v;
        sumSquares += v * v;
        ++used;
    }

    if (used == 0 || sumSquares <= 0.0f)
        return 0;

    const float k = 1.0f / std::sqrt (sumSquares);

    for (std::size_t i = 0; i < numSpeakers; ++i)
        gainsOut[i] *= k;

    return used;
}

} // namespace spatial
