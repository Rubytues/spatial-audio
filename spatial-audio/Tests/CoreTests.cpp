// Spatial Audio - checks for the panning and pathway maths.
// Runs automatically on every build. No JUCE needed.
#include "../Source/Core/Dbap.h"
#include "../Source/Core/Filters.h"
#include "../Source/Core/Pathway.h"

#include <cstdio>
#include <vector>

using namespace spatial;

static int failures = 0;

#define CHECK(condition, message) \
    do { if (! (condition)) { std::printf ("FAIL: %s (line %d)\n", message, __LINE__); ++failures; } } while (0)

static bool near (float a, float b, float tolerance = 1.0e-3f) { return std::abs (a - b) <= tolerance; }

static void testDbap()
{
    const std::vector<Vec3> quad { { -2, 2, 1.2f }, { 2, 2, 1.2f }, { 2, -2, 1.2f }, { -2, -2, 1.2f } };
    std::vector<float> gains (4);
    DbapSettings s;

    // Constant power anywhere in the room
    for (float x = -3; x <= 3; x += 0.5f)
        for (float y = -3; y <= 3; y += 0.5f)
        {
            computeDbapGains ({ x, y, 1.2f }, quad.data(), nullptr, 4, s, gains.data());
            float power = 0;
            for (float g : gains) power += g * g;
            CHECK (near (power, 1.0f), "total power should stay at 1");
        }

    // Source in the middle: all equal
    computeDbapGains ({ 0, 0, 1.2f }, quad.data(), nullptr, 4, s, gains.data());
    CHECK (near (gains[0], 0.5f) && near (gains[1], 0.5f) && near (gains[2], 0.5f) && near (gains[3], 0.5f),
           "centre source should be equal in all four speakers");

    // Source on the front-left speaker: that one is loudest by far
    computeDbapGains ({ -2, 2, 1.2f }, quad.data(), nullptr, 4, s, gains.data());
    CHECK (gains[0] > 0.9f, "source on a speaker should mostly come from that speaker");
    CHECK (gains[2] < gains[1] && gains[2] < gains[3], "opposite speaker should be quietest");

    // Excluded speakers get nothing, the rest still sum to full power
    const bool include[4] = { true, true, false, false };
    const auto used = computeDbapGains ({ 0, -2, 1.2f }, quad.data(), include, 4, s, gains.data());
    CHECK (used == 2, "two speakers should take part");
    CHECK (gains[2] == 0.0f && gains[3] == 0.0f, "excluded speakers should be silent");
    CHECK (near (gains[0] * gains[0] + gains[1] * gains[1], 1.0f), "included speakers keep full power");

    // No speakers at all: no crash, returns 0
    CHECK (computeDbapGains ({ 0, 0, 0 }, quad.data(), nullptr, 0, s, gains.data()) == 0, "empty layout should return 0");
}

static void testPathways()
{
    PathParams p;
    p.centre = { 1, 2, 1.5f };
    p.size = 2;

    // Circle: every point is 2 m from the centre, starting at the front
    p.shape = PathShape::circle;
    for (int i = 0; i <= 100; ++i)
        CHECK (near ((pathPoint (p, i / 100.0f) - p.centre).length(), 2.0f), "circle radius should equal size");
    const auto start = pathPoint (p, 0.0f);
    CHECK (near (start.x, 1.0f) && near (start.y, 4.0f), "circle should start at the front");
    const auto quarter = pathPoint (p, 0.25f);
    CHECK (near (quarter.x, 3.0f) && near (quarter.y, 2.0f), "circle should move clockwise (to the right) first");

    // Tilt 90: circle stands up like a wheel facing the audience
    p.tiltDegrees = 90;
    const auto top = pathPoint (p, 0.0f);
    CHECK (near (top.z, 3.5f) && near (top.y, 2.0f), "tilted circle should start at the top");
    p.tiltDegrees = 0;

    // Square corners
    p.shape = PathShape::square;
    const auto corner = pathPoint (p, 0.25f);
    CHECK (near (corner.x, 3.0f) && near (corner.y, 4.0f), "square second corner should be front right");

    // Closed shapes end where they start
    for (auto shape : { PathShape::circle, PathShape::triangle, PathShape::square, PathShape::figureEight })
    {
        p.shape = shape;
        CHECK ((pathPoint (p, 0.0f) - pathPoint (p, 1.0f)).length() < 1.0e-3f, "closed shapes should loop seamlessly");
    }

    // Spiral: outside to inside, rising
    p.shape = PathShape::spiral;
    p.spiralInner = 0.25f;
    p.spiralRise = 1.0f;
    const auto outer = pathPoint (p, 0.0f) - p.centre;
    const auto inner = pathPoint (p, 1.0f) - p.centre;
    CHECK (near (std::sqrt (outer.x * outer.x + outer.y * outer.y), 2.0f), "spiral should start on the outside");
    CHECK (near (std::sqrt (inner.x * inner.x + inner.y * inner.y), 0.5f), "spiral should end at inner radius");
    CHECK (near (inner.z, 1.0f), "spiral should rise by the set amount");

    // Directions
    CHECK (near (passesToPathPosition (0.25, PathDirection::forward, 0.0f), 0.25f), "forward");
    CHECK (near (passesToPathPosition (0.25, PathDirection::reverse, 0.0f), 0.75f), "reverse");
    CHECK (near (passesToPathPosition (1.25, PathDirection::backAndForth, 0.0f), 0.75f), "back and forth returns");
    CHECK (near (passesToPathPosition (2.25, PathDirection::backAndForth, 0.0f), 0.25f), "back and forth repeats");
    CHECK (near (passesToPathPosition (0.0, PathDirection::forward, 0.5f), 0.5f), "start offset");

    // Tempo: 4 bars of 4/4 at 120 BPM = 8 seconds
    SpeedParams speed;
    speed.tempoSync = true;
    speed.bpm = 120;
    speed.barsPerPass = 4;
    CHECK (near (speed.effectiveSecondsPerPass(), 8.0f), "4 bars at 120 BPM should be 8 seconds");
}

static void testFilters()
{
    // Low-pass passes a low tone and cuts a high one
    auto rms = [] (double hz)
    {
        LowPassLR4 f;
        f.setCutoff (48000.0, 100.0);
        double sum = 0;
        const int n = 48000;
        for (int i = 0; i < n; ++i)
        {
            const float y = f.process ((float) std::sin (2.0 * 3.14159265358979 * hz * i / 48000.0));
            if (i > n / 2) sum += y * y;
        }
        return std::sqrt (sum / (n / 2));
    };

    CHECK (rms (40.0) > 0.6, "40 Hz should pass the 100 Hz crossover");
    CHECK (rms (1000.0) < 0.01, "1 kHz should be removed by the 100 Hz crossover");
}

int main()
{
    testDbap();
    testPathways();
    testFilters();

    if (failures == 0)
        std::printf ("All checks passed.\n");

    return failures == 0 ? 0 : 1;
}
