#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/SpliceTraversal.hpp>

using namespace the89th;
using Catch::Approx;

namespace
{
SpliceTraversal make (double xp1, double xp2, double ratio, double writeRate = 1.0)
{
    SpliceTraversal t;
    t.setCrossfadeLength (64);
    t.setRatio (ratio);
    t.setWriteRate (writeRate);
    t.setRegion (xp1, xp2);
    t.reset();
    t.setRegion (xp1, xp2);
    return t;
}
} // namespace

TEST_CASE ("step follows writeRate minus signed rate", "[traversal]")
{
    SECTION ("forward at unity does not move")
    {
        auto t = make (100.0, 2000.0, 1.0);
        REQUIRE_FALSE (t.reversed());
        REQUIRE (t.signedRate() == Approx (1.0));
        REQUIRE (t.step() == Approx (0.0));
    }

    SECTION ("forward an octave up walks toward the shallow bound")
    {
        auto t = make (100.0, 2000.0, 2.0);
        REQUIRE (t.step() == Approx (-1.0));
    }

    SECTION ("forward an octave down walks toward the deep bound")
    {
        auto t = make (100.0, 2000.0, 0.5);
        REQUIRE (t.step() == Approx (0.5));
    }

    SECTION ("crosspoint order alone flips the sign")
    {
        auto t = make (2000.0, 100.0, 1.0);
        REQUIRE (t.reversed());
        REQUIRE (t.signedRate() == Approx (-1.0));
        REQUIRE (t.step() == Approx (2.0));  // retreats at exactly playback speed
    }

    SECTION ("reverse and transpose compose")
    {
        auto t = make (2000.0, 100.0, 2.0);
        REQUIRE (t.step() == Approx (3.0));
    }

    SECTION ("freeze drops writeRate, so unity forward loops instead of parking")
    {
        auto t = make (100.0, 2000.0, 1.0, 0.0);
        REQUIRE (t.step() == Approx (-1.0));
    }
}

TEST_CASE ("unity forward never splices", "[traversal]")
{
    auto t = make (100.0, 2000.0, 1.0);
    const double start = t.primary().delaySamples;

    for (int i = 0; i < 100000; ++i)
    {
        t.advance();
        REQUIRE_FALSE (t.splicing());
    }

    REQUIRE (t.primary().delaySamples == start);
    REQUIRE (t.primary().gain == 1.0f);
    REQUIRE (t.secondary().gain == 0.0f);
}

TEST_CASE ("the head stays inside the region", "[traversal]")
{
    const double lo = 100.0, hi = 2000.0;

    for (double ratio : { 0.25, 0.5, 0.75, 1.5, 2.0 })
    {
        for (bool rev : { false, true })
        {
            auto t = rev ? make (hi, lo, ratio) : make (lo, hi, ratio);

            // Between splices the carrying head is strictly inside the region,
            // give or take the sample of overshoot discrete stepping allows.
            const double inner = std::abs (t.step()) + 1e-9;

            // During a splice the incoming head starts beyond the entry bound
            // and ramps in from silence, the way an analog crossfade does. The
            // fade cap keeps that excursion inside half a region.
            const double outer = 0.5 * t.regionLength() + 1.0;

            double idleLow = 1e18, idleHigh = -1e18;
            double anyLow  = 1e18, anyHigh  = -1e18;

            for (int i = 0; i < 200000; ++i)
            {
                t.advance();

                const double p = t.primary().delaySamples;
                const double s = t.secondary().delaySamples;

                anyLow  = std::min ({ anyLow,  p, s });
                anyHigh = std::max ({ anyHigh, p, s });

                if (! t.splicing())
                {
                    idleLow  = std::min (idleLow,  p);
                    idleHigh = std::max (idleHigh, p);
                }
            }

            INFO ("ratio " << ratio << (rev ? " reverse" : " forward"));
            REQUIRE (idleLow  >= lo - inner);
            REQUIRE (idleHigh <= hi + inner);
            REQUIRE (anyLow   >= lo - outer);
            REQUIRE (anyHigh  <= hi + outer);
        }
    }
}

TEST_CASE ("the head starts on the bound it travels away from", "[traversal]")
{
    const double lo = 100.0, hi = 2000.0;

    // Which bound is the entry follows the sign of the step, not crosspoint
    // order. A ratio below 1.0 drifts deeper, so it has to start shallow.
    auto up = make (lo, hi, 2.0);
    REQUIRE (up.step() < 0.0);
    REQUIRE (up.primary().delaySamples == Approx (hi));

    auto down = make (lo, hi, 0.5);
    REQUIRE (down.step() > 0.0);
    REQUIRE (down.primary().delaySamples == Approx (lo));

    auto rev = make (hi, lo, 1.0);
    REQUIRE (rev.step() > 0.0);
    REQUIRE (rev.primary().delaySamples == Approx (lo));

    // Neither placement should splice on the first sample.
    for (auto* t : { &up, &down, &rev })
    {
        t->advance();
        REQUIRE_FALSE (t->splicing());
    }
}

TEST_CASE ("both heads stay in memory across a splice", "[traversal]")
{
    // The incoming head must land on the entry bound rather than a region length
    // behind the outgoing one. Offsetting by a full region pushes it outside
    // memory whenever the region spans most of it, and a clamped head reads at
    // 1x instead of the pitch ratio, leaking untransposed signal into the fade.
    for (double ratio : { 0.25, 0.5, 1.5, 2.0 })
    {
        for (bool rev : { false, true })
        {
            const double lo = 2.0, hi = 8189.0;
            auto t = rev ? make (hi, lo, ratio) : make (lo, hi, ratio);

            int splices = 0;
            bool wasSplicing = false;
            double worstLow = 1e18, worstHigh = -1e18;

            for (int i = 0; i < 200000; ++i)
            {
                t.advance();
                if (! t.splicing())
                {
                    wasSplicing = false;
                    continue;
                }

                if (! wasSplicing)
                    ++splices;
                wasSplicing = true;

                const double p = t.primary().delaySamples;
                const double s = t.secondary().delaySamples;
                worstLow  = std::min ({ worstLow,  p, s });
                worstHigh = std::max ({ worstHigh, p, s });
            }

            INFO ("ratio " << ratio << (rev ? " reverse" : " forward"));
            REQUIRE (splices > 0);
            REQUIRE (worstLow  >= lo - 1.0);
            REQUIRE (worstHigh <= hi + 1.0);
        }
    }
}

TEST_CASE ("crossfade gains are equal-power and hand over cleanly", "[traversal]")
{
    auto t = make (100.0, 2000.0, 2.0);

    bool checked = false;
    for (int i = 0; i < 50000; ++i)
    {
        t.advance();
        if (! t.splicing())
            continue;

        const float a = t.primary().gain;
        const float b = t.secondary().gain;
        REQUIRE (a * a + b * b == Approx (1.0f).margin (1e-5));
        checked = true;
    }

    REQUIRE (checked);
}

TEST_CASE ("splices repeat at the traversal period", "[traversal]")
{
    auto t = make (100.0, 2000.0, 2.0);   // step -1: one sample of travel per sample

    // Handover leaves the head one fade's travel inside the entry bound, so a
    // traversal is the region shortened by exactly that much.
    const double length = t.regionLength() - 64.0;

    int  firstStart = -1, secondStart = -1, n = 0;
    bool wasSplicing = false;

    for (int i = 0; i < 20000; ++i, ++n)
    {
        t.advance();
        const bool now = t.splicing();
        if (now && ! wasSplicing)
        {
            if (firstStart < 0)       firstStart = n;
            else if (secondStart < 0) secondStart = n;
        }
        wasSplicing = now;
    }

    REQUIRE (firstStart >= 0);
    REQUIRE (secondStart > firstStart);
    REQUIRE (static_cast<double> (secondStart - firstStart) == Approx (length).margin (2.0));
}

TEST_CASE ("a degenerate region is widened rather than stalling", "[traversal]")
{
    auto t = make (500.0, 500.0, 2.0);
    REQUIRE (t.regionLength() == Approx (SpliceTraversal::kMinRegion));

    const double outer = 0.5 * t.regionLength() + 1.0;
    double low = 1e18, high = -1e18;

    for (int i = 0; i < 10000; ++i)
    {
        t.advance();
        const double d = t.primary().delaySamples;
        REQUIRE (std::isfinite (d));
        low  = std::min (low, d);
        high = std::max (high, d);
    }

    REQUIRE (low  >= t.regionLo() - outer);
    REQUIRE (high <= t.regionHi() + outer);
}
