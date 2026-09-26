#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>
#include <the89th/Engine.hpp>
#include <the89th/FeedbackTone.hpp>
#include <the89th/Machine.hpp>
#include <the89th/Musical.hpp>
#include <the89th/Scrub.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

namespace
{
using TestMachine = Machine<CatmullRom, NoQuantiser>;

/** Delay mode, a short fixed delay, heavy feedback on both sides. */
EngineParams repeats (FeedbackRoute route)
{
    EngineParams p;
    p.route = route;
    for (auto* c : { &p.left, &p.right })
    {
        c->mode     = Mode::Delay;
        c->delay    = 0.1;
        c->feedback = 0.8;
    }
    return p;
}

struct Out
{
    std::vector<float> l, r;
};

/** A burst of noise into the given sides, then silence. */
Out render (const EngineParams& p, bool intoLeft, bool intoRight, int n = 40000, int burst = 400)
{
    TestMachine m;
    m.prepare (Spec {});
    m.setParams (p);
    m.reset();

    const auto noise = test_support::noise (burst, 7u, 0.5f);
    Out o;
    o.l.resize (static_cast<std::size_t> (n));
    o.r.resize (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        const float x = i < burst ? noise[static_cast<std::size_t> (i)] : 0.0f;
        m.step (intoLeft ? x : 0.0f, intoRight ? x : 0.0f,
                o.l[static_cast<std::size_t> (i)], o.r[static_cast<std::size_t> (i)]);
    }
    return o;
}

double energy (const std::vector<float>& x, int from, int to)
{
    double e = 0.0;
    for (int i = from; i < to; ++i)
        e += static_cast<double> (x[static_cast<std::size_t> (i)]) * x[static_cast<std::size_t> (i)];
    return e;
}

/** Share of energy in the sample-to-sample change: rises with brightness. */
double brightness (const std::vector<float>& x, int from, int to)
{
    double d = 0.0, e = 0.0;
    for (int i = from + 1; i < to; ++i)
    {
        const double a = x[static_cast<std::size_t> (i)];
        const double b = x[static_cast<std::size_t> (i - 1)];
        d += (a - b) * (a - b);
        e += a * a;
    }
    return e > 0.0 ? d / e : 0.0;
}
} // namespace

// ─── Routing ────────────────────────────────────────────────────────────────

TEST_CASE ("routing: normal keeps the sides apart, cross ping-pongs, sum shares", "[modern][routing]")
{
    const int tail = 3000;   // after the burst has gone in and started repeating

    SECTION ("normal: left input never reaches the right")
    {
        const auto o = render (repeats (FeedbackRoute::Normal), true, false);
        REQUIRE (energy (o.r, 0, static_cast<int> (o.r.size())) == 0.0);
        REQUIRE (energy (o.l, tail, static_cast<int> (o.l.size())) > 0.0);
    }

    SECTION ("cross: the left's repeats come out of the right")
    {
        const auto o = render (repeats (FeedbackRoute::Cross), true, false);
        REQUIRE (energy (o.r, tail, static_cast<int> (o.r.size())) > 1e-3);
    }

    SECTION ("sum: both sides carry the repeats")
    {
        const auto o = render (repeats (FeedbackRoute::Sum), true, false);
        REQUIRE (energy (o.l, tail, static_cast<int> (o.l.size())) > 1e-3);
        REQUIRE (energy (o.r, tail, static_cast<int> (o.r.size())) > 1e-3);
    }

    SECTION ("quasi-stereo merges whatever the routing says")
    {
        auto a = repeats (FeedbackRoute::Normal);
        auto b = repeats (FeedbackRoute::Cross);
        a.stereo = b.stereo = StereoMode::Quasi;
        REQUIRE (render (a, true, false).l == render (b, true, false).l);
    }
}

// ─── Feedback tone ──────────────────────────────────────────────────────────

TEST_CASE ("the feedback tone is bypassed at its neutral settings", "[modern][tone]")
{
    FeedbackTone t;
    t.setSampleRate (26455.0);
    t.setParams (EngineParams::kLowCutOffHz, EngineParams::kHighCutOffHz, 0.0);
    REQUIRE_FALSE (t.active());
    for (float x : { 0.0f, 0.3f, -0.9f, 1.7f })
        REQUIRE (t.process (x) == x);
}

TEST_CASE ("a high cut in the loop darkens every repeat a little more", "[modern][tone]")
{
    auto p = repeats (FeedbackRoute::Normal);
    p.highCutHz = 2500.0;
    const auto o = render (p, true, false);

    // Delay 0.1 of the memory is about 820 words: repeat k sits around k * 820.
    const int d = static_cast<int> (2 + 0.1 * (8189 - 2));
    const double first  = brightness (o.l, d,     d + 400);
    const double third  = brightness (o.l, 3 * d, 3 * d + 400);
    const double sixth  = brightness (o.l, 6 * d, 6 * d + 400);
    INFO ("brightness " << first << " -> " << third << " -> " << sixth);
    REQUIRE (third < first);
    REQUIRE (sixth < third);

    const auto plain = render (repeats (FeedbackRoute::Normal), true, false);
    REQUIRE (brightness (o.l, 6 * d, 6 * d + 400) < brightness (plain.l, 6 * d, 6 * d + 400));
}

TEST_CASE ("a low cut in the loop thins the repeats", "[modern][tone]")
{
    auto p = repeats (FeedbackRoute::Normal);
    p.lowCutHz = 800.0;
    const auto cut   = render (p, true, false);
    const auto plain = render (repeats (FeedbackRoute::Normal), true, false);

    const int d = static_cast<int> (2 + 0.1 * (8189 - 2));
    REQUIRE (brightness (cut.l, 5 * d, 5 * d + 400) > brightness (plain.l, 5 * d, 5 * d + 400));
}

TEST_CASE ("drive rounds off loud repeats and never runs away", "[modern][tone]")
{
    // A steady tone into 0.99 feedback: the repeats pile up on the input.
    auto peakWith = [] (double drive)
    {
        auto p = repeats (FeedbackRoute::Normal);
        for (auto* c : { &p.left, &p.right })
            c->feedback = 0.99;
        p.drive = drive;

        TestMachine m;
        m.prepare (Spec {});
        m.setParams (p);
        m.reset();

        float peak = 0.0f;
        for (int i = 0; i < 100000; ++i)
        {
            const float x = 0.9f * static_cast<float> (std::sin (0.07 * i));
            float l, r;
            m.step (x, x, l, r);
            REQUIRE (std::isfinite (l));
            if (i > 50000)
                peak = std::max (peak, std::abs (l));
        }
        return peak;
    };

    const float plain  = peakWith (0.0);
    const float driven = peakWith (1.0);
    INFO ("peak without drive " << plain << ", with " << driven);
    REQUIRE (plain > 1.3f);     // measured 1.46
    REQUIRE (driven < 1.05f);   // measured 1.00
}

// ─── Musical pitch ──────────────────────────────────────────────────────────

TEST_CASE ("snap lands on the nearest step of the scale", "[modern][snap]")
{
    using musical::Scale;
    using musical::snapSemitones;

    REQUIRE (snapSemitones (5.3, Scale::Chromatic) == 5.0);
    REQUIRE (snapSemitones (-7.6, Scale::Chromatic) == -8.0);
    REQUIRE (snapSemitones (5.3, Scale::Off) == 5.3);

    // Major: 0 2 4 5 7 9 11, across two octaves either way.
    REQUIRE (snapSemitones (6.0, Scale::Major) == 5.0);    // tie goes down
    REQUIRE (snapSemitones (8.2, Scale::Major) == 9.0);
    REQUIRE (snapSemitones (-1.2, Scale::Major) == -1.0);  // the 7th below
    REQUIRE (snapSemitones (-15.0, Scale::Major) == -15.0); // the 6th, an octave and a half down
    REQUIRE (snapSemitones (-22.6, Scale::Major) == -22.0);

    // Minor: 0 2 3 5 7 8 10.
    REQUIRE (snapSemitones (4.0, Scale::Minor) == 3.0);
    REQUIRE (snapSemitones (11.4, Scale::Minor) == 12.0);

    // Pentatonic: 0 2 4 7 9.
    REQUIRE (snapSemitones (5.0, Scale::Pentatonic) == 4.0);
    REQUIRE (snapSemitones (6.0, Scale::Pentatonic) == 7.0);
}

TEST_CASE ("fine adds cents on top and stays inside the hardware's span", "[modern][snap]")
{
    using musical::Scale;
    REQUIRE (musical::pitchRatio (1.5, Scale::Off, 0.0) == 1.5);   // untouched at neutral
    REQUIRE (musical::pitchRatio (1.0, Scale::Off, 100.0) == Approx (std::exp2 (1.0 / 12.0)));
    REQUIRE (musical::pitchRatio (1.41, Scale::Chromatic, 0.0) == Approx (std::exp2 (6.0 / 12.0)));
    REQUIRE (musical::pitchRatio (2.0, Scale::Off, 100.0) == 2.0);
    REQUIRE (musical::pitchRatio (0.25, Scale::Off, -100.0) == 0.25);
}

// ─── Sync ───────────────────────────────────────────────────────────────────

TEST_CASE ("sync steps through note values at the tempo", "[modern][sync]")
{
    using namespace musical;
    REQUIRE (syncStep (0.0) == 0);
    REQUIRE (syncSeconds (0, 144.0) == 0.0);
    REQUIRE (std::string (syncName (0)) == "MIN");
    REQUIRE (syncStep (1.0) == kSyncSteps - 1);
    REQUIRE (std::string (syncName (kSyncSteps - 1)) == "1/1");

    // 1/8 at 144 BPM is 208.33 ms.
    int eighth = 0;
    for (int s = 1; s < kSyncSteps; ++s)
        if (std::string (syncName (s)) == "1/8")
            eighth = s;
    REQUIRE (syncSeconds (eighth, 144.0) == Approx (0.208333).margin (1e-6));

    // Steps get longer all the way up.
    for (int s = 2; s < kSyncSteps; ++s)
        REQUIRE (syncSeconds (s, 120.0) > syncSeconds (s - 1, 120.0));
}

// ─── Motion ─────────────────────────────────────────────────────────────────

TEST_CASE ("scrub slides the region and keeps it inside memory", "[modern][scrub]")
{
    for (ScrubMode mode : { ScrubMode::Lfo, ScrubMode::Random })
    {
        Engine e;
        e.prepare (48000.0, 512);

        EngineParams p;
        p.left.crosspoint1 = p.right.crosspoint1 = 0.2;
        p.left.crosspoint2 = p.right.crosspoint2 = 0.4;
        p.scrubDepth = 1.0;
        p.scrubRate  = 4.0;
        p.scrubMode  = mode;
        e.setParams (p);

        std::vector<float> l (512, 0.1f), r = l;
        double lo = 1e9, hi = -1e9;
        const double length = e.machine().voice (0).traversal().regionLength();
        for (int b = 0; b < 200; ++b)
        {
            float* io[2] = { l.data(), r.data() };
            e.process (io, 2, 512);

            const auto& t = e.machine().voice (0).traversal();
            lo = std::min (lo, t.regionLo());
            hi = std::max (hi, t.regionHi());
            REQUIRE (t.regionLength() == Approx (length).margin (1.0));
            REQUIRE (t.regionLo() >= DelayMemory::kMinDelay - 1e-9);
            REQUIRE (t.regionHi() <= 8192 - DelayMemory::kEndGuard + 1e-9);
        }

        // It actually wanders: well over half a region each way at full depth.
        INFO ((mode == ScrubMode::Lfo ? "LFO" : "random") << " travel " << (hi - lo) / length << " regions");
        REQUIRE ((hi - lo) > 1.5 * length);
    }
}

TEST_CASE ("scrub at zero depth leaves the region alone", "[modern][scrub]")
{
    Engine e;
    e.prepare (48000.0, 512);
    EngineParams p;
    p.left.crosspoint1 = 0.2;
    p.left.crosspoint2 = 0.4;
    p.scrubRate = 4.0;
    e.setParams (p);

    const double lo = e.machine().voice (0).traversal().regionLo();
    std::vector<float> l (512, 0.1f), r = l;
    for (int b = 0; b < 50; ++b)
    {
        float* io[2] = { l.data(), r.data() };
        e.process (io, 2, 512);
        REQUIRE (e.machine().voice (0).traversal().regionLo() == lo);
    }
}

TEST_CASE ("random scrub stays within bounds and repeats from a reset", "[modern][scrub]")
{
    ScrubLfo a, b;
    a.reset();
    b.reset();
    for (int i = 0; i < 5000; ++i)
    {
        const double x = a.advance (64, 3.0, 48000.0, ScrubMode::Random);
        REQUIRE (x >= -1.0);
        REQUIRE (x <= 1.0);
        REQUIRE (x == b.advance (64, 3.0, 48000.0, ScrubMode::Random));
    }
}

TEST_CASE ("square vibrato in pitch mode trills upward only", "[modern][vibrato]")
{
    constexpr double kFs = 26455.0;
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});
    eng.setXing (false);

    ChannelParams p;
    p.crosspoint1  = 0.0;
    p.crosspoint2  = 0.2;
    p.vibratoDepth = 2.0;
    p.vibratoRate  = 3.0;
    p.vibratoShape = VibratoShape::Square;
    eng.setParams (p);
    eng.reset();

    const int n = 120000;
    const auto in = test_support::sine (n, 1000.0, kFs, 0.8f);
    std::vector<float> out (static_cast<std::size_t> (n));
    std::vector<char> splicing (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        out[static_cast<std::size_t> (i)] = eng.processSample (in[static_cast<std::size_t> (i)]);
        splicing[static_cast<std::size_t> (i)] = eng.traversal().splicing();
    }

    double lo = 1e9, hi = 0.0;
    for (int i = 30000; i + 400 < n; i += 100)
    {
        bool clean = true;
        for (int k = 0; k < 400 && clean; ++k)
            clean = ! splicing[static_cast<std::size_t> (i + k)];
        if (! clean)
            continue;
        const double f = test_support::estimateFreq (out.data() + i, 400, kFs, 500.0, 3000.0);
        lo = std::min (lo, f);
        hi = std::max (hi, f);
    }

    // Two semitones up from 1 kHz is 1122.5 Hz; it never dips below the note.
    INFO ("range " << lo << " .. " << hi << " Hz");
    REQUIRE (lo == Approx (1000.0).margin (10.0));
    REQUIRE (hi == Approx (1122.5).margin (12.0));
}
