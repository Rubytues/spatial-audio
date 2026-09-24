// Spatial Audio - simple filters for subs and transducers
#pragma once

#include <cmath>

namespace spatial
{

// Second-order Butterworth low-pass (RBJ cookbook biquad).
struct LowPass2
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void setCutoff (double sampleRate, double hz)
    {
        const double q = 0.70710678118654752;
        const double w0 = 2.0 * 3.14159265358979323846 * hz / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cw) * 0.5) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void reset() { z1 = z2 = 0.0f; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Fourth-order Linkwitz-Riley low-pass: two Butterworths in a row.
// The standard crossover filter for subwoofers.
struct LowPassLR4
{
    LowPass2 a, b;

    void setCutoff (double sampleRate, double hz) { a.setCutoff (sampleRate, hz); b.setCutoff (sampleRate, hz); }
    void reset() { a.reset(); b.reset(); }
    float process (float x) { return b.process (a.process (x)); }
};

} // namespace spatial
