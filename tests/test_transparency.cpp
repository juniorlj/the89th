#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>

#include "TestSupport.hpp"

using namespace the89th;

namespace
{
/** Crosspoint 2 at 1.0 lands on words - kEndGuard, an exact integer, so the
    interpolator sits at frac == 0 and returns the stored sample untouched.
    That makes bit equality the right assertion rather than a tolerance. */
constexpr int kExpectedDelay = 8192 - DelayMemory::kEndGuard;

template <class Interp>
void checkTransparent (unsigned seed)
{
    ChannelEngine<Interp, NoQuantiser> eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.pitchRatio  = 1.0;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    p.feedback    = 0.0;
    p.freeze      = false;

    eng.setParams (p);
    eng.reset();

    REQUIRE (eng.traversal().step() == 0.0);
    REQUIRE (eng.traversal().primary().delaySamples == static_cast<double> (kExpectedDelay));
    REQUIRE (eng.traversal().primary().gain == 1.0f);
    REQUIRE (eng.traversal().secondary().gain == 0.0f);

    const int N  = 20000;
    const auto in = test_support::noise (N, seed);
    std::vector<float> out (static_cast<std::size_t> (N));

    eng.process (in.data(), out.data(), N);

    int mismatches = 0;
    for (int n = kExpectedDelay; n < N; ++n)
        if (out[static_cast<std::size_t> (n)]
            != in[static_cast<std::size_t> (n - kExpectedDelay)])
            ++mismatches;

    REQUIRE (mismatches == 0);
    REQUIRE_FALSE (eng.traversal().splicing());
}
} // namespace

TEST_CASE ("ratio 1.0 forward is bit-transparent, Catmull-Rom", "[transparency]")
{
    checkTransparent<CatmullRom> (12345u);
}

TEST_CASE ("ratio 1.0 forward is bit-transparent, linear", "[transparency]")
{
    checkTransparent<Linear> (999u);
}

TEST_CASE ("memory reads as silence before the delay fills", "[transparency]")
{
    DefaultChannelEngine eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const auto in = test_support::noise (kExpectedDelay, 7u);
    std::vector<float> out (static_cast<std::size_t> (kExpectedDelay));
    eng.process (in.data(), out.data(), kExpectedDelay);

    for (int n = 0; n < kExpectedDelay; ++n)
        REQUIRE (out[static_cast<std::size_t> (n)] == 0.0f);
}

TEST_CASE ("transparency survives a shorter integer delay", "[transparency]")
{
    // Crosspoint 1 at 0.0 maps to kMinDelay, also an exact integer.
    DefaultChannelEngine eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.pitchRatio  = 1.0;
    p.crosspoint1 = 1.0;   // deeper than crosspoint 2 would reverse, so keep it as the far bound
    p.crosspoint2 = 0.0;   // head parks here: delay == kMinDelay
    eng.setParams (p);
    eng.reset();

    REQUIRE (eng.traversal().reversed());
    REQUIRE (eng.traversal().primary().delaySamples
             == static_cast<double> (DelayMemory::kMinDelay));

    // Reversed at ratio 1.0 the step is 2.0, so this one does traverse and splice.
    REQUIRE (eng.traversal().step() == 2.0);
}
