#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>
#include <the89th/Machine.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

namespace
{
constexpr double kFs = 26455.0;

/** Runs the machine at its own clock on separate left and right inputs. */
struct StereoRun
{
    std::vector<float> l, r;
};

StereoRun runMachine (DefaultMachine& m, const std::vector<float>& inL, const std::vector<float>& inR)
{
    StereoRun out;
    out.l.resize (inL.size());
    out.r.resize (inL.size());
    for (std::size_t i = 0; i < inL.size(); ++i)
        m.step (inL[i], inR[i], out.l[i], out.r[i]);
    return out;
}

float peakFrom (const std::vector<float>& v, std::size_t from)
{
    float p = 0.0f;
    for (std::size_t i = from; i < v.size(); ++i)
        p = std::max (p, std::fabs (v[i]));
    return p;
}
} // namespace

// ─── Delay mode ─────────────────────────────────────────────────────────────

TEST_CASE ("delay mode is a bit-exact fixed delay", "[machine][delay]")
{
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});
    eng.setMode (Mode::Delay);

    ChannelParams p;
    p.delay = 1.0;              // lands on words - kEndGuard, an integer
    p.pitchRatio = 1.7;         // ignored in delay mode
    eng.setParams (p);
    eng.reset();

    REQUIRE_FALSE (eng.voice().onTraversal());
    const int D = 8192 - DelayMemory::kEndGuard;
    REQUIRE (eng.voice().delayCurrent() == static_cast<double> (D));

    const int N = 20000;
    const auto in = test_support::noise (N, 91u);
    std::vector<float> out (static_cast<std::size_t> (N));
    eng.process (in.data(), out.data(), N);

    int mismatches = 0;
    for (int n = D; n < N; ++n)
        if (out[static_cast<std::size_t> (n)] != in[static_cast<std::size_t> (n - D)])
            ++mismatches;
    REQUIRE (mismatches == 0);
}

TEST_CASE ("moving Delay crossfades rather than clicking", "[machine][delay]")
{
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});
    eng.setMode (Mode::Delay);

    ChannelParams p;
    p.delay = 0.3;
    eng.setParams (p);
    eng.reset();

    const int N = 60000;
    const auto in = test_support::sine (N, 440.0, kFs, 0.9f);
    std::vector<float> out (static_cast<std::size_t> (N));

    for (int i = 0; i < N; ++i)
    {
        if (i == 30000) { p.delay = 0.71; eng.setParams (p); }
        out[static_cast<std::size_t> (i)] = eng.processSample (in[static_cast<std::size_t> (i)]);
    }

    const double natural = 0.9 * 2.0 * std::sin (M_PI * 440.0 / kFs);
    const double observed = test_support::maxAbsDelta (out.data(), 25000, N);
    INFO ("observed " << observed << " natural " << natural);
    REQUIRE (observed < 2.0 * natural);
    REQUIRE (eng.voice().delayCurrent() == Approx (2.0 + 0.71 * (8189.0 - 2.0)));
}

TEST_CASE ("the short range divides the delay scale by ten", "[machine][delay]")
{
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});
    eng.setMode (Mode::Delay);

    ChannelParams p;
    p.delay = 1.0;
    eng.setParams (p);

    eng.setRange (DelayRange::Long);
    const double longMax = eng.voice().maxDelaySamples();
    eng.setRange (DelayRange::Short);
    const double shortMax = eng.voice().maxDelaySamples();

    REQUIRE (longMax == Approx (8189.0));
    REQUIRE ((shortMax - 2.0) == Approx ((longMax - 2.0) / 10.0));

    // 819 samples at 26455 Hz: ~31 ms, doubling and flanging territory.
    REQUIRE (1000.0 * shortMax / kFs == Approx (31.0).margin (0.5));
}

TEST_CASE ("latching in delay mode carries the read position across", "[machine][delay][freeze]")
{
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});
    eng.setMode (Mode::Delay);

    ChannelParams p;
    p.delay = 0.5;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int N = 60000;
    const auto in = test_support::sine (N, 440.0, kFs, 0.9f);
    std::vector<float> out (static_cast<std::size_t> (N));

    for (int i = 0; i < N; ++i)
    {
        if (i == 20000) { p.freeze = true; eng.setParams (p); }
        if (i == 20000) REQUIRE (eng.voice().onTraversal());
        out[static_cast<std::size_t> (i)] = eng.processSample (i < 20000 ? in[static_cast<std::size_t> (i)] : 0.0f);
    }

    // Latching must not jump: the traversal picks up exactly where the delay
    // head was reading.
    const double natural = 0.9 * 2.0 * std::sin (M_PI * 440.0 / kFs);
    REQUIRE (test_support::maxAbsDelta (out.data(), 19000, 21000) < 1.5 * natural);

    // And the loop keeps sounding with the input gone.
    REQUIRE (peakFrom (out, 50000) > 0.5f);
}

// ─── Stereo layouts ─────────────────────────────────────────────────────────

TEST_CASE ("true stereo keeps the channels apart", "[machine][stereo]")
{
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;
    p.stereo = StereoMode::True;
    p.left.crosspoint2 = p.right.crosspoint2 = 0.3;
    m.setParams (p);
    m.reset();

    REQUIRE (m.wordsPerVoice() == 8192);
    REQUIRE (m.effectiveBandwidth() != Bandwidth::k20kHz);

    const auto tone    = test_support::sine (30000, 440.0, kFs, 0.8f);
    const std::vector<float> silent (30000, 0.0f);
    const auto out = runMachine (m, tone, silent);

    REQUIRE (peakFrom (out.l, 10000) > 0.5f);
    REQUIRE (peakFrom (out.r, 0) == 0.0f);
}

TEST_CASE ("quasi-stereo: one memory, full size, 20 kHz reachable", "[machine][stereo]")
{
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;
    p.stereo    = StereoMode::Quasi;
    p.bandwidth = Bandwidth::k20kHz;
    m.setParams (p);

    REQUIRE (m.quasi());
    REQUIRE (m.wordsPerVoice() == 16384);
    REQUIRE (m.effectiveBandwidth() == Bandwidth::k20kHz);
    REQUIRE (m.internalSampleRate() == Approx (52910.0));

    // Same RAM, full clock: 309.7 ms, the published 300.
    REQUIRE (1000.0 * 16384 / m.internalSampleRate() == Approx (309.66).margin (0.5));
}

TEST_CASE ("quasi-stereo feeds both sides from one input", "[machine][stereo]")
{
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;
    p.stereo = StereoMode::Quasi;
    p.left.crosspoint2  = 0.2;
    p.right.crosspoint2 = 0.35;     // each side keeps its own crosspoints
    p.right.pitchRatio  = 1.5;      // and its own pitch
    m.setParams (p);
    m.reset();

    const auto tone = test_support::sine (60000, 440.0, 52910.0, 0.8f);
    const std::vector<float> silent (60000, 0.0f);
    const auto out = runMachine (m, tone, silent);

    // Only the left input carried signal, yet both sides play it.
    REQUIRE (peakFrom (out.l, 20000) > 0.3f);
    REQUIRE (peakFrom (out.r, 20000) > 0.3f);

    const double fl = test_support::estimateFreq (out.l.data() + 30000, 8192, 52910.0, 100.0, 4000.0);
    const double fr = test_support::estimateFreq (out.r.data() + 30000, 8192, 52910.0, 100.0, 4000.0);
    REQUIRE (fl == Approx (440.0).epsilon (0.02));
    REQUIRE (fr == Approx (660.0).epsilon (0.02));
}

TEST_CASE ("quasi-stereo merges both outputs into one recirculation", "[machine][stereo]")
{
    // Feedback only on the right side. There is one write, so the right side's
    // repeats land in the memory the left side reads, and the left hears them.
    auto leftTail = [] (double rightFeedback)
    {
        DefaultMachine m;
        m.prepare (Spec {});
        EngineParams p;
        p.stereo = StereoMode::Quasi;
        p.left.crosspoint2 = p.right.crosspoint2 = 0.05;   // ~820 samples
        p.left.feedback  = 0.0;
        p.right.feedback = rightFeedback;
        m.setParams (p);
        m.reset();

        const int n = 12000;
        std::vector<float> burst (static_cast<std::size_t> (n), 0.0f);
        for (int i = 0; i < 400; ++i)
            burst[static_cast<std::size_t> (i)] = 0.8f * std::sin (0.05f * i);
        const std::vector<float> silent (static_cast<std::size_t> (n), 0.0f);

        const auto out = runMachine (m, burst, silent);

        // The first echo is over by ~1300 samples. After that, only
        // recirculation can put anything on the left.
        float peak = 0.0f;
        for (int i = 1700; i < 4000; ++i)
            peak = std::max (peak, std::fabs (out.l[static_cast<std::size_t> (i)]));
        return peak;
    };

    REQUIRE (leftTail (0.0) < 1e-3f);
    REQUIRE (leftTail (0.9) > 0.05f);
}

TEST_CASE ("in quasi-stereo the latch holds both sides", "[machine][stereo][freeze]")
{
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;
    p.stereo = StereoMode::Quasi;
    p.left.freeze = true;      // latch asked for on one side only
    m.setParams (p);

    REQUIRE (m.memory (0).writeHeld());
    REQUIRE (m.voice (0).traversal().step() == Approx (-1.0));  // both read as frozen
    REQUIRE (m.voice (1).traversal().step() == Approx (-1.0));
}

TEST_CASE ("each side has its own mode", "[machine][stereo]")
{
    // The panel has Delay and Pitch-Shifter buttons per side: one channel can
    // echo while the other transposes.
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;
    p.left.mode        = Mode::Delay;
    p.left.delay       = 0.1;
    p.left.pitchRatio  = 1.5;           // ignored: the left side is a delay
    p.right.mode       = Mode::Pitch;
    p.right.pitchRatio = 1.5;
    p.right.crosspoint2 = 0.35;
    m.setParams (p);
    m.reset();

    REQUIRE_FALSE (m.voice (0).onTraversal());
    REQUIRE (m.voice (1).onTraversal());

    const double fs = m.internalSampleRate();
    const auto tone = test_support::sine (40000, 440.0, fs, 0.8f);
    const auto out  = runMachine (m, tone, tone);

    const double fl = test_support::estimateFreq (out.l.data() + 20000, 8192, fs, 100.0, 4000.0);
    const double fr = test_support::estimateFreq (out.r.data() + 20000, 8192, fs, 100.0, 4000.0);
    REQUIRE (fl == Approx (440.0).epsilon (0.02));
    REQUIRE (fr == Approx (660.0).epsilon (0.02));
}

TEST_CASE ("in true stereo each side latches on its own", "[machine][stereo][freeze]")
{
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;
    p.right.freeze = true;
    m.setParams (p);

    REQUIRE_FALSE (m.memory (0).writeHeld());
    REQUIRE (m.memory (1).writeHeld());
    REQUIRE (m.voice (0).traversal().step() == Approx (0.0));
    REQUIRE (m.voice (1).traversal().step() == Approx (-1.0));
}

TEST_CASE ("switching layout repartitions without reallocating", "[machine][stereo]")
{
    DefaultMachine m;
    m.prepare (Spec {});
    EngineParams p;

    for (int k = 0; k < 6; ++k)
    {
        p.stereo = (k % 2 == 0) ? StereoMode::Quasi : StereoMode::True;
        m.setParams (p);
        REQUIRE (m.wordsPerVoice() == (p.stereo == StereoMode::Quasi ? 16384 : 8192));

        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < 5000; ++i)
            m.step (0.5f, -0.5f, l, r);
        REQUIRE (std::isfinite (l));
        REQUIRE (std::isfinite (r));
    }
}

// ─── Vibrato ────────────────────────────────────────────────────────────────

namespace
{
/** Instantaneous-frequency extremes of the output, measured in short windows. */
std::pair<double, double> vibratoExtent (Mode mode, double depth, double rate)
{
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});
    eng.setMode (mode);
    eng.setXing (false);

    ChannelParams p;
    p.delay        = 0.2;
    p.crosspoint1  = 0.0;
    p.crosspoint2  = 0.2;
    p.vibratoDepth = depth;
    p.vibratoRate  = rate;
    eng.setParams (p);
    eng.reset();

    const int N = 100000;
    const auto in = test_support::sine (N, 1000.0, kFs, 0.8f);
    std::vector<float> out (static_cast<std::size_t> (N));
    std::vector<char>  splicing (static_cast<std::size_t> (N));
    for (int i = 0; i < N; ++i)
    {
        out[static_cast<std::size_t> (i)] = eng.processSample (in[static_cast<std::size_t> (i)]);
        splicing[static_cast<std::size_t> (i)] = eng.voice().onTraversal() && eng.traversal().splicing();
    }

    // At unity the head sits on crosspoint 2, so vibrato carries it across and
    // splices; those windows measure the join, not the vibrato.
    double lo = 1e9, hi = 0.0;
    for (int i = 30000; i + 400 < N; i += 100)
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
    return { lo, hi };
}
} // namespace

TEST_CASE ("vibrato swings pitch by the set depth, in either mode", "[machine][vibrato]")
{
    for (Mode mode : { Mode::Pitch, Mode::Delay })
    {
        const auto [lo, hi] = vibratoExtent (mode, 1.0, 4.0);
        INFO ((mode == Mode::Pitch ? "pitch" : "delay") << " mode: " << lo << " .. " << hi << " Hz");

        // One semitone up from 1 kHz is 1059.5 Hz. Down is a little less than a
        // semitone in delay mode, the usual asymmetry of modulating a delay.
        REQUIRE (hi == Approx (1059.5).margin (12.0));
        REQUIRE (lo < 950.0);
        REQUIRE (lo > 930.0);
    }
}

TEST_CASE ("vibrato at zero depth changes nothing", "[machine][vibrato]")
{
    // With depth at zero the rate must be irrelevant. If the LFO leaked into
    // the signal anywhere, two different rates would render differently.
    auto render = [] (double rate)
    {
        ChannelEngine<CatmullRom, NoQuantiser> eng;
        eng.prepare (Spec {});
        ChannelParams p;
        p.pitchRatio   = 1.5;
        p.crosspoint1  = 0.1;
        p.crosspoint2  = 0.4;
        p.vibratoDepth = 0.0;
        p.vibratoRate  = rate;
        eng.setParams (p);
        eng.reset();
        const auto in = test_support::noise (60000, 8u);
        std::vector<float> out (in.size());
        eng.process (in.data(), out.data(), static_cast<int> (in.size()));
        return out;
    };

    REQUIRE (render (0.3) == render (7.0));
}
