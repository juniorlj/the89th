#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

namespace
{
constexpr double kFs = 26455.0;

struct Run
{
    std::vector<float> out;
    int    splices = 0;
    int    searched = 0;
    double meanCorrelation = 0.0;
    double headLow = 1e18, headHigh = -1e18;
};

Run render (bool xing, double ratio, double xp1, double xp2, int fade,
            const std::vector<float>& in)
{
    Spec spec;
    spec.crossfadeSamples = fade;

    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (spec);
    eng.setXing (xing);

    ChannelParams p;
    p.pitchRatio  = ratio;
    p.crosspoint1 = xp1;
    p.crosspoint2 = xp2;
    eng.setParams (p);
    eng.reset();

    Run r;
    r.out.resize (in.size());
    bool was = false;

    for (std::size_t i = 0; i < in.size(); ++i)
    {
        r.out[i] = eng.processSample (in[i]);
        const auto& t = eng.traversal();
        const bool now = t.splicing();

        if (now && ! was && i > 20000)
        {
            ++r.splices;
            const auto m = eng.voice().xing().last();
            if (m.candidates > 0)
            {
                ++r.searched;
                r.meanCorrelation += m.correlation;
            }
        }
        if (now)
        {
            r.headLow  = std::min ({ r.headLow,  t.primary().delaySamples, t.secondary().delaySamples });
            r.headHigh = std::max ({ r.headHigh, t.primary().delaySamples, t.secondary().delaySamples });
        }
        was = now;
    }

    if (r.searched > 0)
        r.meanCorrelation /= r.searched;
    return r;
}

float envelopePeak (const std::vector<float>& y, int from, int win, bool wantMax)
{
    float result = wantMax ? 0.0f : 1e9f;
    for (int i = from; i + win < static_cast<int> (y.size()); i += win / 2)
    {
        float m = 0.0f;
        for (int k = 0; k < win; ++k)
            m = std::max (m, std::fabs (y[static_cast<std::size_t> (i + k)]));
        result = wantMax ? std::max (result, m) : std::min (result, m);
    }
    return result;
}
} // namespace

TEST_CASE ("Xing finds a matching join at every splice", "[xing]")
{
    const auto in = test_support::sine (150000, 440.0, kFs, 0.9f);

    struct Case { double ratio, xp1, xp2; const char* label; };
    for (const Case c : { Case { 1.5, 0.1, 0.4, "fifth up, mid region" },
                          Case { 2.0, 0.0, 1.0, "octave up, full memory (the default region)" },
                          Case { 0.5, 0.0, 1.0, "octave down, full memory" },
                          Case { 1.0, 0.6, 0.2, "reverse at pitch" } })
    {
        INFO (c.label);
        const auto r = render (true, c.ratio, c.xp1, c.xp2, 96, in);
        REQUIRE (r.splices > 2);
        REQUIRE (r.searched == r.splices);
        REQUIRE (r.meanCorrelation > 0.99);
    }
}

TEST_CASE ("Xing keeps the level flat through splices", "[xing]")
{
    // Without matching, the two heads meet at arbitrary phase and an
    // equal-power fade bumps or dips the level. Matched, the fade is seamless.
    const auto in = test_support::sine (150000, 440.0, kFs, 0.9f);

    const auto fixed   = render (false, 1.5, 0.1, 0.4, 96, in);
    const auto matched = render (true,  1.5, 0.1, 0.4, 96, in);

    const float fixedSwing   = envelopePeak (fixed.out,   20000, 128, true) / envelopePeak (fixed.out,   20000, 128, false);
    const float matchedSwing = envelopePeak (matched.out, 20000, 128, true) / envelopePeak (matched.out, 20000, 128, false);

    INFO ("envelope max/min: fixed " << fixedSwing << ", matched " << matchedSwing);
    REQUIRE (20.0 * std::log10 (fixedSwing)   > 1.5);
    REQUIRE (20.0 * std::log10 (matchedSwing) < 0.2);
}

TEST_CASE ("with Xing even a one-sample splice is continuous", "[xing]")
{
    // The strongest form of the claim: no fade to hide behind. The largest
    // sample step must be the one the output tone makes by itself.
    const auto in = test_support::sine (150000, 440.0, kFs, 0.9f);

    for (double ratio : { 1.5, 2.0 })
    {
        const auto r = render (true, ratio, 0.0, 1.0, 1, in);
        const double natural = 0.9 * 2.0 * std::sin (M_PI * 440.0 * ratio / kFs);
        const double observed = test_support::maxAbsDelta (r.out.data(), 20000, static_cast<int> (r.out.size()));

        INFO ("ratio " << ratio << ": observed " << observed << ", natural " << natural);
        REQUIRE (observed < natural * 1.02);
    }
}

TEST_CASE ("Xing keeps both heads inside the crosspoint region", "[xing]")
{
    const auto in = test_support::noise (150000, 3u, 0.8f);
    const auto r = render (true, 1.5, 0.1, 0.4, 96, in);

    const double base = DelayMemory::kMinDelay;
    const double span = (8192 - DelayMemory::kEndGuard) - base;
    REQUIRE (r.headLow  >= base + 0.1 * span - 1.0);
    REQUIRE (r.headHigh <= base + 0.4 * span + 1.0);
}

TEST_CASE ("Xing on noise or silence stays finite and falls back cleanly", "[xing]")
{
    const auto noise = test_support::noise (150000, 11u, 0.8f);
    const auto a = render (true, 1.5, 0.1, 0.4, 96, noise);
    REQUIRE (a.searched == a.splices);
    REQUIRE (a.meanCorrelation < 0.9);   // uncorrelated material cannot fake a match
    for (float y : a.out)
        REQUIRE (std::isfinite (y));

    const std::vector<float> silence (150000, 0.0f);
    const auto b = render (true, 1.5, 0.1, 0.4, 96, silence);
    REQUIRE (b.searched == 0);            // nothing to match against
    for (float y : b.out)
        REQUIRE (y == 0.0f);
}
