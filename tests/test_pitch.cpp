#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

namespace
{
constexpr double kFs = 26455.0;  // 10 kHz bandwidth, the true-stereo ceiling
constexpr double kF0 = 440.0;

/** Runs a sine through the engine and measures the output's fundamental.
    Measurement starts after the memory has filled and the head has settled. */
double shiftedFreq (double ratio, bool reverse, int n = 120000)
{
    DefaultChannelEngine eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.pitchRatio  = ratio;
    p.crosspoint1 = reverse ? 1.0 : 0.0;
    p.crosspoint2 = reverse ? 0.0 : 1.0;
    eng.setParams (p);
    eng.reset();

    const auto in = test_support::sine (n, kF0, kFs);
    std::vector<float> out (static_cast<std::size_t> (n));
    eng.process (in.data(), out.data(), n);

    const int from = n / 2;
    return test_support::estimateFreq (out.data() + from, 8192, kFs, 100.0, 4000.0);
}
} // namespace

TEST_CASE ("ratio 2.0 doubles the frequency", "[pitch]")
{
    REQUIRE (shiftedFreq (2.0, false) == Approx (2.0 * kF0).epsilon (0.01));
}

TEST_CASE ("ratio 0.5 halves the frequency", "[pitch]")
{
    REQUIRE (shiftedFreq (0.5, false) == Approx (0.5 * kF0).epsilon (0.01));
}

TEST_CASE ("intermediate ratios scale the frequency", "[pitch]")
{
    REQUIRE (shiftedFreq (1.5, false)  == Approx (1.5 * kF0).epsilon (0.01));
    REQUIRE (shiftedFreq (0.75, false) == Approx (0.75 * kF0).epsilon (0.01));
}

TEST_CASE ("ratio 1.0 forward leaves the frequency alone", "[pitch]")
{
    REQUIRE (shiftedFreq (1.0, false) == Approx (kF0).epsilon (0.01));
}

TEST_CASE ("reverse holds pitch at ratio 1.0", "[pitch]")
{
    // Walking backwards at playback speed inverts time without transposing:
    // a sine reversed is still a sine at the same frequency.
    REQUIRE (shiftedFreq (1.0, true) == Approx (kF0).epsilon (0.01));
}

TEST_CASE ("reverse and transpose compose", "[pitch]")
{
    REQUIRE (shiftedFreq (2.0, true) == Approx (2.0 * kF0).epsilon (0.01));
}

TEST_CASE ("freeze keeps looping the region after the input stops", "[pitch]")
{
    DefaultChannelEngine eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.pitchRatio  = 1.0;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int n = 40000;
    const auto in = test_support::sine (n, kF0, kFs);
    std::vector<float> fill (static_cast<std::size_t> (n));
    eng.process (in.data(), fill.data(), n);

    // Latch, then feed silence. Unity forward frozen has step -1, so the head
    // keeps moving through the region rather than parking on one sample.
    p.freeze = true;
    eng.setParams (p);
    REQUIRE (eng.traversal().step() == Approx (-1.0));

    std::vector<float> silence (static_cast<std::size_t> (n), 0.0f);
    std::vector<float> held (static_cast<std::size_t> (n));
    eng.process (silence.data(), held.data(), n);

    float peak = 0.0f;
    for (int i = n / 2; i < n; ++i)
        peak = std::max (peak, std::abs (held[static_cast<std::size_t> (i)]));

    REQUIRE (peak > 0.3f);

    const double f = test_support::estimateFreq (held.data() + n / 2, 8192, kFs, 100.0, 4000.0);
    REQUIRE (f == Approx (kF0).epsilon (0.02));
}
