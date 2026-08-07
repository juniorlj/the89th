#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>
#include <the89th/Engine.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

TEST_CASE ("the converter clock divides by bandwidth", "[bandwidth]")
{
    Spec mono;
    mono.channels = 1;

    REQUIRE (internalRate (mono, Bandwidth::k20kHz) == Approx (52910.0));
    REQUIRE (internalRate (mono, Bandwidth::k10kHz) == Approx (26455.0));
    REQUIRE (internalRate (mono, Bandwidth::k5kHz)  == Approx (13227.5));
}

TEST_CASE ("true stereo splits memory and puts 20 kHz out of reach", "[bandwidth]")
{
    Spec stereo;  // channels == 2

    REQUIRE (stereo.memoryWordsPerChannel() == 8192);
    REQUIRE (stereo.effectiveBandwidth (Bandwidth::k20kHz) == Bandwidth::k10kHz);
    REQUIRE (stereo.effectiveBandwidth (Bandwidth::k10kHz) == Bandwidth::k10kHz);
    REQUIRE (stereo.effectiveBandwidth (Bandwidth::k5kHz)  == Bandwidth::k5kHz);
    REQUIRE (internalRate (stereo, Bandwidth::k20kHz) == Approx (26455.0));

    Spec quasi;
    quasi.channels = 1;
    REQUIRE (quasi.memoryWordsPerChannel() == 16384);
    REQUIRE (quasi.effectiveBandwidth (Bandwidth::k20kHz) == Bandwidth::k20kHz);
}

TEST_CASE ("delay maxima match the published figures", "[bandwidth]")
{
    Spec stereo;
    // Manual gives ~300 ms at 10 kHz and ~600 ms at 5 kHz for true stereo.
    REQUIRE (1000.0 * maxDelaySeconds (stereo, Bandwidth::k10kHz) == Approx (309.66).margin (0.5));
    REQUIRE (1000.0 * maxDelaySeconds (stereo, Bandwidth::k5kHz)  == Approx (619.32).margin (0.5));

    Spec quasi;
    quasi.channels = 1;
    // And 300 / 600 / 1200 ms for quasi-stereo.
    REQUIRE (1000.0 * maxDelaySeconds (quasi, Bandwidth::k20kHz) == Approx (309.66).margin (0.5));
    REQUIRE (1000.0 * maxDelaySeconds (quasi, Bandwidth::k10kHz) == Approx (619.32).margin (0.5));
    REQUIRE (1000.0 * maxDelaySeconds (quasi, Bandwidth::k5kHz)  == Approx (1238.63).margin (0.5));
}

TEST_CASE ("switching bandwidth leaves memory and pointers untouched", "[bandwidth]")
{
    // This is the whole mechanism behind the octave jump: the addresses stay
    // put and only the clock moves, so stored audio replays at a new rate.
    DefaultChannelEngine eng;
    Spec spec;
    spec.channels = 1;          // give both bandwidths room to differ
    eng.prepare (spec);
    eng.setBandwidth (Bandwidth::k20kHz);

    ChannelParams p;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int n = 30000;
    const auto in = test_support::sine (n, 440.0, 52910.0);
    std::vector<float> out (static_cast<std::size_t> (n));
    eng.process (in.data(), out.data(), n);

    const int words = eng.memory().words();
    std::vector<float> before (static_cast<std::size_t> (words));
    for (int i = 0; i < words; ++i)
        before[static_cast<std::size_t> (i)] = eng.memory().at (i);

    const int    writeIndexBefore = eng.memory().writeIndex();
    const double delayBefore      = eng.traversal().primary().delaySamples;
    const double rateBefore       = eng.internalSampleRate();

    eng.setBandwidth (Bandwidth::k10kHz);

    REQUIRE (eng.memory().writeIndex() == writeIndexBefore);
    REQUIRE (eng.traversal().primary().delaySamples == delayBefore);

    for (int i = 0; i < words; ++i)
        REQUIRE (eng.memory().at (i) == before[static_cast<std::size_t> (i)]);

    REQUIRE (eng.internalSampleRate() == Approx (rateBefore / 2.0));
}

TEST_CASE ("stored audio replays an octave down at half the clock", "[bandwidth]")
{
    // Fill at 52910, latch, then reinterpret the same words at 26455.
    Spec spec;
    spec.channels = 1;

    DefaultChannelEngine eng;
    eng.prepare (spec);
    eng.setBandwidth (Bandwidth::k20kHz);

    ChannelParams p;
    p.pitchRatio  = 1.0;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int n = 60000;
    const auto in = test_support::sine (n, 880.0, 52910.0);
    std::vector<float> fill (static_cast<std::size_t> (n));
    eng.process (in.data(), fill.data(), n);

    p.freeze = true;
    eng.setParams (p);
    eng.setBandwidth (Bandwidth::k10kHz);

    std::vector<float> silence (static_cast<std::size_t> (n), 0.0f);
    std::vector<float> held (static_cast<std::size_t> (n));
    eng.process (silence.data(), held.data(), n);

    // Same stored words, half the clock: the tone that went in at 880 Hz now
    // reads out at 440 Hz against the new rate.
    const double f = test_support::estimateFreq (held.data() + n / 2, 8192,
                                                 eng.internalSampleRate(), 100.0, 4000.0);
    REQUIRE (f == Approx (440.0).epsilon (0.02));
}

TEST_CASE ("the engine keeps running across a live bandwidth switch", "[bandwidth]")
{
    Engine engine;
    engine.prepare (48000.0, 512);

    EngineParams p;
    p.bandwidth   = Bandwidth::k10kHz;
    p.mix         = 1.0;
    p.left.pitchRatio = p.right.pitchRatio = 1.0;
    p.left.crosspoint1 = p.right.crosspoint1 = 0.0;
    p.left.crosspoint2 = p.right.crosspoint2 = 0.5;
    engine.setParams (p);

    const int block = 512, blocks = 200;
    auto tone = test_support::sine (block, 440.0, 48000.0);

    std::vector<float> l (static_cast<std::size_t> (block));
    std::vector<float> r (static_cast<std::size_t> (block));
    float* io[2] = { l.data(), r.data() };

    float peakAfterSwitch = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        std::copy (tone.begin(), tone.end(), l.begin());
        std::copy (tone.begin(), tone.end(), r.begin());

        if (b == blocks / 2)
        {
            p.bandwidth = Bandwidth::k5kHz;
            engine.setParams (p);
        }

        engine.process (io, 2, block);

        if (b > blocks / 2 + 20)
            for (int i = 0; i < block; ++i)
                peakAfterSwitch = std::max (peakAfterSwitch, std::abs (l[static_cast<std::size_t> (i)]));
    }

    // Memory survives the switch, so the output carries on rather than dropping out.
    REQUIRE (peakAfterSwitch > 0.1f);
    REQUIRE (std::isfinite (peakAfterSwitch));
}
