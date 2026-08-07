#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

namespace
{
constexpr double kFs  = 26455.0;
constexpr double kF0  = 440.0;
constexpr float  kAmp = 0.9f;

/** Largest step a clean sine of this frequency can take between samples. An
    unmasked splice jumps between uncorrelated phases and clears this by roughly
    an order of magnitude, so the two are easy to tell apart. */
double naturalDelta (double freqHz, float amp)
{
    return amp * 2.0 * std::sin (M_PI * freqHz / kFs);
}

struct Run
{
    std::vector<float> out;
    int splices = 0;
};

Run render (double ratio, bool reverse, int n = 120000)
{
    DefaultChannelEngine eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.pitchRatio  = ratio;
    p.crosspoint1 = reverse ? 1.0 : 0.0;
    p.crosspoint2 = reverse ? 0.0 : 1.0;
    eng.setParams (p);
    eng.reset();

    const auto in = test_support::sine (n, kF0, kFs, kAmp);

    Run r;
    r.out.resize (static_cast<std::size_t> (n));

    bool wasSplicing = false;
    for (int i = 0; i < n; ++i)
    {
        r.out[static_cast<std::size_t> (i)] = eng.processSample (in[static_cast<std::size_t> (i)]);
        const bool now = eng.traversal().splicing();
        if (now && ! wasSplicing)
            ++r.splices;
        wasSplicing = now;
    }

    return r;
}
} // namespace

TEST_CASE ("the splice produces no sample discontinuity", "[splice]")
{
    struct Case { double ratio; bool reverse; const char* label; };

    const Case cases[] = {
        { 2.0,  false, "octave up" },
        { 0.5,  false, "octave down" },
        { 1.5,  false, "fifth up" },
        { 0.25, false, "two octaves down" },
        { 1.0,  true,  "reverse at pitch" },
        { 2.0,  true,  "reverse an octave up" },
    };

    for (const auto& c : cases)
    {
        INFO (c.label);

        const auto run = render (c.ratio, c.reverse);
        REQUIRE (run.splices > 2);   // the run has to actually cross splices

        // Skip the fill, where the head reads cleared memory.
        const int from = 8192 + 2000;
        const float observed = test_support::maxAbsDelta (run.out.data(), from,
                                                          static_cast<int> (run.out.size()));

        // Reverse inverts time without transposing, so its output sits at kF0.
        const double outHz = c.reverse ? kF0 * (c.ratio == 1.0 ? 1.0 : c.ratio)
                                       : kF0 * c.ratio;
        const double bound = 2.0 * naturalDelta (outHz, kAmp);

        INFO ("observed " << observed << " bound " << bound);
        REQUIRE (static_cast<double> (observed) < bound);
    }
}

TEST_CASE ("a splice without a crossfade is measurably worse", "[splice]")
{
    // Guards the test itself: if a zero-length fade also passed, the threshold
    // would be proving nothing.
    Spec spec;
    spec.crossfadeSamples = 1;

    DefaultChannelEngine eng;
    eng.prepare (spec);

    ChannelParams p;
    p.pitchRatio  = 2.0;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int n = 120000;
    const auto in = test_support::sine (n, kF0, kFs, kAmp);
    std::vector<float> out (static_cast<std::size_t> (n));
    eng.process (in.data(), out.data(), n);

    const float observed = test_support::maxAbsDelta (out.data(), 8192 + 2000, n);
    const double bound   = 2.0 * naturalDelta (2.0 * kF0, kAmp);

    REQUIRE (static_cast<double> (observed) > bound);
}

TEST_CASE ("the crossfade holds level across the splice", "[splice]")
{
    const auto run = render (2.0, false);

    float peak = 0.0f, trough = 1.0f;
    for (int i = 8192 + 2000; i + 256 < static_cast<int> (run.out.size()); i += 256)
    {
        float local = 0.0f;
        for (int k = 0; k < 256; ++k)
            local = std::max (local, std::abs (run.out[static_cast<std::size_t> (i + k)]));

        peak   = std::max (peak, local);
        trough = std::min (trough, local);
    }

    // Equal-power over a splice between uncorrelated phases keeps the envelope
    // inside a modest window rather than dropping out.
    REQUIRE (trough > 0.4f * peak);
}
