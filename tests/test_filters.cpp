#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/BandLimit.hpp>
#include <the89th/Engine.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;
using test_support::toDb;
using test_support::toneAmplitude;

namespace
{
constexpr double kHost = 48000.0;

/** Tone through the whole host-facing engine: filters, resampler, converter,
    machine at unity pitch. Returns output amplitude relative to input. */
double throughEngine (double freqHz, Bandwidth bw, Mode mode = Mode::Pitch,
                      float amp = 0.25f, double probeHz = -1.0)
{
    Engine engine;
    engine.prepare (kHost, 512);
    engine.setXing (false);

    EngineParams p;
    p.bandwidth = bw;
    p.mode      = mode;
    p.left.crosspoint1 = p.right.crosspoint1 = 0.0;
    p.left.crosspoint2 = p.right.crosspoint2 = 0.2;
    p.left.delay       = p.right.delay       = 0.2;
    engine.setParams (p);

    const int n = static_cast<int> (kHost * 1.5);
    auto l = test_support::sine (n, freqHz, kHost, amp);
    auto r = l;
    for (int pos = 0; pos < n; pos += 512)
    {
        float* io[2] = { l.data() + pos, r.data() + pos };
        engine.process (io, 2, std::min (512, n - pos));
    }

    const int from = static_cast<int> (kHost * 0.5);
    const double probe = probeHz > 0.0 ? probeHz : freqHz;
    return toneAmplitude (l.data() + from, n - from, probe, kHost) / amp;
}
} // namespace

TEST_CASE ("each filter sits 1.5 dB down at the band edge, the pair 3 dB", "[filters]")
{
    for (double edge : { 5000.0, 10000.0, 20000.0 })
    {
        BandLimitFilter f;
        f.design (kHost, edge);
        INFO ("edge " << edge);
        REQUIRE (f.responseDb (std::min (edge, 0.45 * kHost), kHost)
                 == Approx (-BandLimitFilter::kEdgeDb).margin (0.05));
    }
}

TEST_CASE ("passband ripple stays within the Chebyshev ripple", "[filters]")
{
    BandLimitFilter f;
    f.design (kHost, 10000.0);

    double lo = 1e9, hi = -1e9;
    for (double hz = 20.0; hz < 9000.0; hz += 25.0)
    {
        const double db = f.responseDb (hz, kHost);
        lo = std::min (lo, db);
        hi = std::max (hi, db);
    }
    REQUIRE (hi <= 0.01);
    REQUIRE (hi - lo <= BandLimitFilter::kRippleDb + 0.05);
}

TEST_CASE ("the stopband falls steeply past the edge", "[filters]")
{
    BandLimitFilter f;
    f.design (kHost, 5000.0);
    REQUIRE (f.responseDb (10000.0, kHost) < -60.0);
    REQUIRE (f.responseDb (12787.0, kHost) < -70.0);
}

TEST_CASE ("the 12.8 kHz image at 5 kHz bandwidth is gone", "[filters][engine]")
{
    // Phase 0 measured this image at -2.7 dB relative to a 440 Hz tone: the
    // replica of the tone around the 13227.5 Hz clock, with nothing to stop it.
    const double tone  = throughEngine (440.0, Bandwidth::k5kHz);
    const double image = throughEngine (440.0, Bandwidth::k5kHz, Mode::Pitch, 0.25f, 13227.5 - 440.0);

    INFO ("tone " << toDb (tone) << " dB, image " << toDb (image / tone) << " dB relative");
    REQUIRE (toDb (tone) == Approx (0.0).margin (1.0));
    REQUIRE (toDb (image / tone) < -60.0);
}

TEST_CASE ("overall response is +0/-3 dB to the band edge", "[filters][engine]")
{
    for (auto [bw, edge] : { std::pair { Bandwidth::k5kHz, 5000.0 },
                             std::pair { Bandwidth::k10kHz, 10000.0 } })
    {
        const double ref = throughEngine (1000.0, bw);
        const double atEdge = throughEngine (edge, bw);
        const double past   = throughEngine (1.4 * edge, bw);

        INFO ("edge " << edge << ": 1 kHz " << toDb (ref) << " dB, edge " << toDb (atEdge / ref)
              << " dB, 1.4x edge " << toDb (past / ref) << " dB");
        REQUIRE (toDb (ref) == Approx (0.0).margin (1.0));
        REQUIRE (toDb (atEdge / ref) == Approx (-3.0).margin (1.0));
        REQUIRE (toDb (past / ref) < -40.0);
    }
}

TEST_CASE ("de-emphasis exactly undoes pre-emphasis", "[filters][emphasis]")
{
    Emphasis e;
    e.design (kHost);

    const auto x = test_support::noise (20000, 5u, 0.5f);
    double maxErr = 0.0;
    for (float v : x)
        maxErr = std::max (maxErr, static_cast<double> (std::fabs (e.de (e.pre (v)) - v)));
    REQUIRE (maxErr < 1e-5);
}

TEST_CASE ("pre-emphasis lifts the treble by the 50/15 us curve", "[filters][emphasis]")
{
    auto gainAt = [] (double hz)
    {
        Emphasis e;
        e.design (kHost);
        const auto x = test_support::sine (48000, hz, kHost, 0.1f);
        std::vector<float> y (x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
            y[i] = e.pre (x[i]);
        return toneAmplitude (y.data() + 4800, 43200, hz, kHost) / 0.1;
    };

    // Analog |1 + jwT1| / |1 + jwT2|: 0 dB low down, about +9.5 dB at 10 kHz.
    auto analog = [] (double hz)
    {
        const double w = 2.0 * M_PI * hz;
        return 10.0 * std::log10 ((1.0 + w * w * Emphasis::kT1 * Emphasis::kT1)
                                / (1.0 + w * w * Emphasis::kT2 * Emphasis::kT2));
    };

    REQUIRE (toDb (gainAt (100.0))   == Approx (analog (100.0)).margin (0.1));
    REQUIRE (toDb (gainAt (3000.0))  == Approx (analog (3000.0)).margin (0.2));
    REQUIRE (toDb (gainAt (10000.0)) == Approx (analog (10000.0)).margin (0.5));
}

TEST_CASE ("in delay mode, emphasis leaves the response flat", "[filters][emphasis][engine]")
{
    // Pre before the converter and de after cancel for the signal; what they
    // change is where the converter's noise lands.
    const double low  = throughEngine (500.0,  Bandwidth::k10kHz, Mode::Delay, 0.1f);
    const double high = throughEngine (7000.0, Bandwidth::k10kHz, Mode::Delay, 0.1f);
    INFO ("500 Hz " << toDb (low) << " dB, 7 kHz " << toDb (high) << " dB");
    REQUIRE (toDb (low)  == Approx (0.0).margin (1.0));
    REQUIRE (toDb (high) == Approx (0.0).margin (1.5));
}

TEST_CASE ("in delay mode, emphasis pushes converter noise down in the treble", "[filters][emphasis][engine]")
{
    // A low tone drives the converter; everything above it in the output is the
    // converter's error. De-emphasis should cut that error in the treble.
    auto trebleNoise = [] (Mode mode)
    {
        Engine engine;
        engine.prepare (kHost, 512);
        EngineParams p;
        p.mode = mode;
        p.bandwidth = Bandwidth::k10kHz;
        p.left.crosspoint2 = p.right.crosspoint2 = 0.2;
        p.left.delay       = p.right.delay       = 0.2;
        engine.setParams (p);

        const int n = static_cast<int> (kHost * 1.5);
        auto l = test_support::sine (n, 220.0, kHost, 0.5f);
        auto r = l;
        for (int pos = 0; pos < n; pos += 512)
        {
            float* io[2] = { l.data() + pos, r.data() + pos };
            engine.process (io, 2, std::min (512, n - pos));
        }

        const int from = static_cast<int> (kHost * 0.5);
        double sum = 0.0;
        for (double hz = 6000.0; hz <= 9000.0; hz += 250.0)
            sum += std::pow (toneAmplitude (l.data() + from, n - from, hz, kHost), 2.0);
        return std::sqrt (sum);
    };

    const double withEmphasis    = trebleNoise (Mode::Delay);
    const double withoutEmphasis = trebleNoise (Mode::Pitch);
    INFO ("treble noise: delay mode " << toDb (withEmphasis) << " dB, pitch mode " << toDb (withoutEmphasis) << " dB");
    REQUIRE (toDb (withoutEmphasis / withEmphasis) > 4.0);
}

TEST_CASE ("an impulse arrives when the delay says, from the first sample", "[engine][timing]")
{
    // Phase 0's resampler read its output ring ahead of what the machine had
    // written, so for the first lap it played output from one ring lap
    // earlier. A steady tone hides that completely; an impulse does not.
    for (Bandwidth bw : { Bandwidth::k5kHz, Bandwidth::k10kHz })
    {
        Engine engine;
        engine.prepare (kHost, 512);

        EngineParams p;
        p.bandwidth = bw;
        p.left.crosspoint2 = p.right.crosspoint2 = 0.2;
        engine.setParams (p);

        const int n = 24000;
        std::vector<float> l (static_cast<std::size_t> (n), 0.0f), r = l;
        l[100] = r[100] = 0.8f;
        for (int pos = 0; pos < n; pos += 512)
        {
            float* io[2] = { l.data() + pos, r.data() + pos };
            engine.process (io, 2, std::min (512, n - pos));
        }

        int peakAt = 0;
        for (int i = 0; i < n; ++i)
            if (std::fabs (l[static_cast<std::size_t> (i)]) > std::fabs (l[static_cast<std::size_t> (peakAt)]))
                peakAt = i;

        // The two band-limit filters delay the peak of an impulse too; measure
        // theirs on its own rather than guess it.
        BandLimitFilter a, b;
        a.design (kHost, engine.bandEdgeHz());
        b.design (kHost, engine.bandEdgeHz());
        int filterPeak = 0;
        float best = 0.0f;
        for (int i = 0; i < 2000; ++i)
        {
            const float y = std::fabs (b.process (a.process (i == 0 ? 1.0f : 0.0f)));
            if (y > best) { best = y; filterPeak = i; }
        }

        const double fs       = engine.machine().internalSampleRate();
        const double delay    = std::floor (2.0 + 0.2 * (8189.0 - 2.0));   // stepped read: whole words
        const double expected = 100.0 + delay * kHost / fs + engine.latencySamples() + filterPeak;

        INFO ("bandwidth clock " << fs << ": peak at " << peakAt << ", expected " << expected);
        REQUIRE (static_cast<double> (peakAt) == Approx (expected).margin (6.0));
    }
}
