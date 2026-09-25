#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>
#include <the89th/Quantiser.hpp>

#include "TestSupport.hpp"

#include <cmath>

using namespace the89th;
using Catch::Approx;

namespace
{
/** Signal to quantisation-noise ratio of a sine at the given level. */
double snrDb (double levelDb)
{
    const double amp = std::pow (10.0, levelDb / 20.0);
    double sig = 0.0, err = 0.0;
    for (int i = 0; i < 48000; ++i)
    {
        const float x = static_cast<float> (amp * std::sin (2.0 * M_PI * 997.0 * i / 48000.0));
        const float q = FlyingComma::store (x);
        sig += static_cast<double> (x) * x;
        err += static_cast<double> (q - x) * (q - x);
    }
    return 10.0 * std::log10 (sig / err);
}
} // namespace

TEST_CASE ("the converter clips at full scale", "[converter]")
{
    REQUIRE (FlyingComma::store (1.0f)  == FlyingComma::kMaxCode);
    REQUIRE (FlyingComma::store (4.0f)  == FlyingComma::kMaxCode);
    REQUIRE (FlyingComma::store (-9.0f) == -FlyingComma::kMaxCode);
    REQUIRE (FlyingComma::store (NAN)   == FlyingComma::kMaxCode);
}

TEST_CASE ("dynamic range is the published ~95 dB", "[converter]")
{
    const float finest = std::ldexp (1.0f, -16);
    REQUIRE (FlyingComma::store (finest)        == finest);
    REQUIRE (FlyingComma::store (0.4f * finest) == 0.0f);

    const double range = 20.0 * std::log10 (FlyingComma::kMaxCode / finest);
    REQUIRE (range == Approx (96.3).margin (0.2));
}

TEST_CASE ("eight ranges, 6 dB apart", "[converter]")
{
    REQUIRE (FlyingComma::rangeOf (0.9f)   == 7);
    REQUIRE (FlyingComma::rangeOf (0.3f)   == 6);
    REQUIRE (FlyingComma::rangeOf (0.2f)   == 5);
    REQUIRE (FlyingComma::rangeOf (1e-6f)  == 0);

    // Within a range, codes sit on a grid 2^(r-16) wide.
    const float x = 0.3f;
    const float q = FlyingComma::store (x);
    const float step = std::ldexp (1.0f, FlyingComma::rangeOf (x) - 16);
    REQUIRE (std::fmod (q, step) == Approx (0.0f).margin (1e-12));
    REQUIRE (std::fabs (q - x) <= 0.5f * step);
}

TEST_CASE ("error stays constant relative to level across the ranging span", "[converter]")
{
    // The defining property. Three exponent bits give eight ranges 6 dB apart,
    // so across the top 42 dB the error scales with the signal and SNR holds
    // steady. A linear converter would lose 6 dB of SNR per 6 dB of level.
    const double a = snrDb (-6.0);
    const double b = snrDb (-20.0);
    const double c = snrDb (-36.0);

    INFO ("SNR at -6 / -20 / -36 dBFS: " << a << " / " << b << " / " << c);
    REQUIRE (a > 55.0);
    REQUIRE (a < 66.0);
    REQUIRE (std::fabs (a - b) < 3.0);
    REQUIRE (std::fabs (a - c) < 3.0);
}

TEST_CASE ("below the ranging span it behaves like 16-bit linear", "[converter]")
{
    // Past -42 dBFS every sample lands in the finest range, step 2^-16, so SNR
    // now falls with level. Still ahead of plain 16-bit, which has twice the step.
    const double quiet   = snrDb (-60.0);
    const double quieter = snrDb (-72.0);
    const double linear16At60 = 6.02 * 16 + 1.76 - 60.0;

    INFO ("SNR at -60 / -72 dBFS: " << quiet << " / " << quieter);
    REQUIRE (quiet > linear16At60);
    REQUIRE (quiet - quieter == Approx (12.0).margin (2.0));
}

TEST_CASE ("converting twice changes nothing", "[converter]")
{
    const auto v = test_support::noise (20000, 77u, 1.2f);
    for (float x : v)
    {
        const float q = FlyingComma::store (x);
        REQUIRE (FlyingComma::store (q) == q);
    }
}

TEST_CASE ("the delay path stays bit-exact behind the converter", "[converter][transparency]")
{
    // The converter changes the signal once, on the way into memory. After
    // that the delay path must add nothing: output equals the converted input,
    // delayed by exactly the crosspoint delay.
    ChannelEngine<CatmullRom, FlyingComma> eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int D = 8192 - DelayMemory::kEndGuard;
    const int N = 20000;
    const auto in = test_support::noise (N, 4242u);
    std::vector<float> out (static_cast<std::size_t> (N));
    eng.process (in.data(), out.data(), N);

    int mismatches = 0;
    for (int n = D; n < N; ++n)
        if (out[static_cast<std::size_t> (n)]
            != FlyingComma::store (in[static_cast<std::size_t> (n - D)]))
            ++mismatches;

    REQUIRE (mismatches == 0);
}

TEST_CASE ("feedback cannot run past full scale", "[converter]")
{
    ChannelEngine<CatmullRom, FlyingComma> eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.pitchRatio  = 1.5;
    p.crosspoint1 = 0.1;
    p.crosspoint2 = 0.4;
    p.feedback    = 0.99;
    eng.setParams (p);
    eng.reset();

    const int N = 200000;
    const auto in = test_support::sine (N, 330.0, 26455.0, 0.95f);
    float peak = 0.0f;
    for (int i = 0; i < N; ++i)
        peak = std::max (peak, std::fabs (eng.processSample (in[static_cast<std::size_t> (i)])));

    // The converter bounds what is stored: no word ever exceeds full scale.
    for (int i = 0; i < eng.memory().words(); ++i)
        REQUIRE (std::fabs (eng.memory().at (i)) <= FlyingComma::kMaxCode);

    // What comes out can exceed one code: an equal-power splice sums two heads
    // (up to sqrt 2), and a cubic interpolator overshoots a clipped waveform
    // (its weights sum to at most 1.25 in magnitude). Bounded, not runaway.
    REQUIRE (peak <= 1.4143f * 1.25f * FlyingComma::kMaxCode);
}
