#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/Engine.hpp>
#include <the89th/Keyboard.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

// ─── Held keys and ratios ───────────────────────────────────────────────────

TEST_CASE ("held keys: the newest sounds, lifting it falls back", "[keys]")
{
    keys::NoteStack s;
    REQUIRE_FALSE (s.active());
    REQUIRE (s.current() == -1);

    s.press (60);
    s.press (64);
    s.press (67);
    REQUIRE (s.current() == 67);
    REQUIRE (s.recent (1) == 64);
    REQUIRE (s.recent (2) == 60);
    REQUIRE (s.recent (3) == -1);

    s.release (67);
    REQUIRE (s.current() == 64);

    s.release (60);              // lifting a key underneath changes nothing heard
    REQUIRE (s.current() == 64);

    s.press (64);                // pressing a held key again doesn't double it
    s.release (64);
    REQUIRE_FALSE (s.active());
}

TEST_CASE ("held keys: overflow drops the oldest", "[keys]")
{
    keys::NoteStack s;
    for (int n = 0; n < keys::NoteStack::kCapacity + 4; ++n)
        s.press (40 + n);

    REQUIRE (s.held() == keys::NoteStack::kCapacity);
    REQUIRE (s.current() == 40 + keys::NoteStack::kCapacity + 3);
    s.clear();
    REQUIRE_FALSE (s.active());
}

TEST_CASE ("key to ratio: root is unity, a semitone per key, inside 0.25 to 2", "[keys]")
{
    REQUIRE (keys::ratio (60, 60) == 1.0);
    REQUIRE (keys::ratio (72, 60) == Approx (2.0));
    REQUIRE (keys::ratio (67, 60) == Approx (1.4983).margin (1e-4));
    REQUIRE (keys::ratio (36, 60) == Approx (0.25));

    // Beyond the hardware's span, keys clamp rather than wrap.
    REQUIRE (keys::ratio (90, 60) == Approx (2.0));
    REQUIRE (keys::ratio (20, 60) == Approx (0.25));

    // Bend adds on top.
    REQUIRE (keys::ratio (60, 60, 2.0) == Approx (std::exp2 (2.0 / 12.0)));
    REQUIRE (keys::bendSemitones (8192) == 0.0);
    REQUIRE (keys::bendSemitones (16383) == Approx (2.0).margin (1e-3));
    REQUIRE (keys::bendSemitones (0) == Approx (-2.0));
}

// ─── Envelope ───────────────────────────────────────────────────────────────

TEST_CASE ("envelope: Push/Play follows the key, Sustain runs its own course", "[keys]")
{
    constexpr double fs = 1000.0;   // one sample a millisecond
    auto run = [] (Envelope& e, int ms) { for (int i = 0; i < ms; ++i) e.next(); return e.level(); };

    Envelope push;
    push.configure (0.1, 0.2, 0.1, fs, false);
    push.trigger();
    REQUIRE (run (push, 50) == Approx (0.5).margin (0.02));
    REQUIRE (run (push, 400) == 1.0);            // held: stays up
    push.release();
    REQUIRE (run (push, 50) == Approx (0.5).margin (0.02));
    run (push, 60);
    REQUIRE (push.idle());

    Envelope sustain;
    sustain.configure (0.1, 0.2, 0.1, fs, true);
    sustain.trigger();
    run (sustain, 20);
    sustain.release();                           // ignored: the note runs on
    REQUIRE (run (sustain, 200) == 1.0);         // attack done, holding
    REQUIRE (run (sustain, 130) == Approx (0.5).margin (0.03));
    run (sustain, 60);
    REQUIRE (sustain.idle());
}

// ─── Through the engine ─────────────────────────────────────────────────────

namespace
{
constexpr double kHost = 48000.0;

int ms (double m) { return static_cast<int> (m * kHost / 1000.0); }

struct Rig
{
    Engine e;
    EngineParams p;
    std::vector<float> l, r;
    int pos = 0;

    explicit Rig (std::vector<float> input, int block = 32)
        : l (input), r (std::move (input)), block_ (block)
    {
        e.prepare (kHost, 256);
    }

    void set() { e.setParams (p); }

    /** Runs to sample `to` in small blocks, as a host split at events does. */
    void runTo (int to)
    {
        to = std::min (to, static_cast<int> (l.size()));
        while (pos < to)
        {
            const int n = std::min (block_, to - pos);
            float* io[2] = { l.data() + pos, r.data() + pos };
            e.process (io, 2, n);
            pos += n;
        }
    }

    double freqL (int from, int len) const
    {
        return test_support::estimateFreq (l.data() + from, len, kHost, 100.0, 3000.0);
    }

    float peak (const std::vector<float>& x, int from, int to) const
    {
        float m = 0.0f;
        for (int i = from; i < to; ++i)
            m = std::max (m, std::fabs (x[static_cast<std::size_t> (i)]));
        return m;
    }

private:
    int block_;
};
} // namespace

TEST_CASE ("with the keyboard off, notes change nothing", "[keys][engine]")
{
    auto render = [] (bool notes)
    {
        Rig rig (test_support::noise (24000, 3u, 0.5f));
        rig.p.left.pitchRatio = 1.5;
        rig.p.left.crosspoint2 = rig.p.right.crosspoint2 = 0.3;
        rig.set();
        rig.runTo (8000);
        if (notes)
        {
            rig.e.noteOn (67);
            rig.e.pitchWheel (12000);
        }
        rig.runTo (24000);
        return rig.l;
    };
    REQUIRE (render (false) == render (true));
}

TEST_CASE ("a key plays its pitch; silence latches and mutes", "[keys][engine]")
{
    Rig rig (test_support::sine (ms (900), 440.0, kHost, 0.5f));
    rig.p.keys.channels = KeyChannels::Left;
    rig.p.left.crosspoint2 = rig.p.right.crosspoint2 = 0.3;
    rig.set();

    // No key yet: the left is latched and silent, the right plays on.
    rig.runTo (ms (200));
    REQUIRE (rig.peak (rig.l, ms (100), ms (200)) < 1e-4f);
    REQUIRE (rig.peak (rig.r, ms (100), ms (200)) > 0.3f);
    REQUIRE (rig.e.machine().memory (0).writeHeld());

    rig.e.noteOn (67);   // a fifth above the root
    rig.runTo (ms (600));
    REQUIRE (rig.freqL (ms (450), ms (150)) == Approx (440.0 * 1.4983).epsilon (0.02));

    rig.e.noteOff (67);
    rig.runTo (ms (900));
    REQUIRE (rig.peak (rig.l, ms (620), ms (900)) < 1e-4f);
    REQUIRE (rig.e.machine().memory (0).writeHeld());
    REQUIRE (rig.e.keyboard().sounding (0) == -1);
    REQUIRE (rig.e.keyboard().sounding (1) == -2);
}

TEST_CASE ("a side the keyboard starts playing picks up the key already down", "[keys][engine]")
{
    Rig rig (std::vector<float> (static_cast<std::size_t> (ms (100)), 0.0f));
    rig.set();
    rig.e.noteOn (67);                  // held while the keyboard is off
    rig.runTo (ms (10));
    REQUIRE (rig.e.keyboard().sounding (0) == -2);

    rig.p.keys.channels = KeyChannels::Left;
    rig.set();
    rig.runTo (ms (30));
    REQUIRE (rig.e.keyboard().sounding (0) == 67);
    REQUIRE (rig.e.effective().left.pitchRatio == Approx (1.4983).margin (1e-4));
}

TEST_CASE ("biphonic: two keys split across the channels, one plays both", "[keys][engine]")
{
    Rig rig (std::vector<float> (static_cast<std::size_t> (ms (400)), 0.0f));
    rig.p.keys.channels = KeyChannels::Biphonic;
    rig.set();

    auto ratios = [&rig]
    {
        rig.runTo (rig.pos + ms (10));
        return std::pair { rig.e.effective().left.pitchRatio, rig.e.effective().right.pitchRatio };
    };

    rig.e.noteOn (60);
    auto [l1, r1] = ratios();
    REQUIRE (l1 == Approx (1.0));
    REQUIRE (r1 == Approx (1.0));

    rig.e.noteOn (67);
    auto [l2, r2] = ratios();
    REQUIRE (l2 == Approx (1.0));
    REQUIRE (r2 == Approx (1.4983).margin (1e-4));

    rig.e.noteOff (67);
    auto [l3, r3] = ratios();
    REQUIRE (r3 == Approx (1.0));
    (void) l3;

    rig.e.noteOn (55);   // below the held key: takes the left
    auto [l4, r4] = ratios();
    REQUIRE (l4 == Approx (std::exp2 (-5.0 / 12.0)));
    REQUIRE (r4 == Approx (1.0));
}

TEST_CASE ("Slope glides from one note to the next; zero lands at once", "[keys][engine]")
{
    auto ratioAfter = [] (double slope, double afterMs)
    {
        Rig rig (std::vector<float> (static_cast<std::size_t> (ms (600)), 0.0f));
        rig.p.keys.channels = KeyChannels::Left;
        rig.p.keys.glideSeconds = slope;
        rig.set();
        rig.e.noteOn (60);
        rig.runTo (ms (50));
        rig.e.noteOn (72);
        rig.runTo (ms (50 + afterMs));
        return rig.e.machine().params().left.pitchRatio;
    };

    REQUIRE (ratioAfter (0.0, 1.0) == Approx (2.0));
    const double mid = ratioAfter (0.2, 100.0);
    REQUIRE (mid > 1.3);
    REQUIRE (mid < 1.6);   // halfway in octaves is sqrt 2
    REQUIRE (ratioAfter (0.2, 250.0) == Approx (2.0));
}

TEST_CASE ("Trimmer tunes the keyboard; Added Delay pushes the region deeper", "[keys][engine]")
{
    Rig rig (std::vector<float> (static_cast<std::size_t> (ms (100)), 0.0f));
    rig.p.keys.channels   = KeyChannels::Left;
    rig.p.keys.trimCents  = 50.0;
    rig.p.keys.addedDelay = 0.5;
    rig.p.left.crosspoint1 = 0.0;
    rig.p.left.crosspoint2 = 0.2;
    rig.set();
    rig.e.noteOn (60);
    rig.runTo (ms (20));

    const auto& left = rig.e.effective().left;
    REQUIRE (left.pitchRatio == Approx (std::exp2 (0.5 / 12.0)));
    REQUIRE (left.crosspoint1 == Approx (0.4));
    REQUIRE (left.crosspoint2 == Approx (0.6));
}

TEST_CASE ("envelope through the engine: Sustain keeps a tapped key sounding", "[keys][engine]")
{
    Rig rig (test_support::sine (ms (1000), 440.0, kHost, 0.5f));
    rig.p.keys.channels = KeyChannels::Left;
    rig.p.keys.play     = KeyPlay::Sustain;
    rig.p.keys.envelope = true;
    rig.p.keys.attackSeconds  = 0.05;
    rig.p.keys.holdSeconds    = 0.3;
    rig.p.keys.releaseSeconds = 0.1;
    rig.p.left.crosspoint2 = 0.3;
    rig.set();

    rig.runTo (ms (200));
    rig.e.noteOn (60);
    rig.runTo (ms (220));
    rig.e.noteOff (60);                 // a tap
    rig.runTo (ms (1000));

    // Attack, then the hold carries it well past the key.
    REQUIRE (rig.peak (rig.l, ms (400), ms (500)) > 0.3f);
    // 50 + 300 + 100 ms after the key it has gone, and the side is latched.
    REQUIRE (rig.peak (rig.l, ms (700), ms (1000)) < 1e-4f);
    REQUIRE (rig.e.effective().left.freeze);
}

TEST_CASE ("Memory Synchro starts each note at the attack point and loops", "[keys][engine]")
{
    // 400 ms of 300 Hz, then 155 ms of 900 Hz, then latch. The latched 310 ms
    // holds 300 Hz in its older half and 900 Hz in its newer half.
    const int fill = ms (555);
    std::vector<float> in (static_cast<std::size_t> (ms (1400)), 0.0f);
    const auto low  = test_support::sine (ms (400), 300.0, kHost, 0.5f);
    const auto high = test_support::sine (ms (155), 900.0, kHost, 0.5f);
    std::copy (low.begin(),  low.end(),  in.begin());
    std::copy (high.begin(), high.end(), in.begin() + ms (400));

    auto playFrom = [&] (double attackPoint, double speed)
    {
        Rig rig (in);
        rig.p.keys.channels      = KeyChannels::Left;
        rig.p.keys.memorySynchro = Sides::Left;
        rig.p.keys.attackPoint   = attackPoint;
        rig.p.keys.returnPoint   = attackPoint;
        rig.p.keys.endPoint      = attackPoint + 0.2;
        rig.p.keys.speed         = speed;
        rig.p.left.crosspoint1 = 0.0;
        rig.p.left.crosspoint2 = 0.06;   // a ~19 ms grain
        rig.set();

        // Fill, holding a key so the memory records, then latch by hand.
        rig.e.noteOn (60);
        rig.runTo (fill);
        rig.p.left.freeze = true;
        rig.set();
        rig.e.noteOff (60);
        rig.runTo (fill + ms (100));

        rig.e.noteOn (60);
        rig.runTo (fill + ms (160));
        return std::pair { rig.freqL (fill + ms (110), ms (40)), rig.e.keyboard().synchroPosition (0) };
    };

    const auto early = playFrom (0.1, 1.0);
    const auto late  = playFrom (0.6, 1.0);
    REQUIRE (early.first == Approx (300.0).epsilon (0.03));
    REQUIRE (late.first  == Approx (900.0).epsilon (0.03));

    // 60 ms at speed 1 is about 0.19 of a 310 ms memory: nearly at the end.
    REQUIRE (early.second == Approx (0.1 + 0.06 / 0.31).margin (0.02));

    // Faster reading reaches the end point and loops within return..end.
    const auto fast = playFrom (0.1, 2.0);
    REQUIRE (fast.second >= 0.1);
    REQUIRE (fast.second <= 0.3);
}

TEST_CASE ("Reverse Synchro restarts the traversal on each attack in the input", "[keys][engine]")
{
    // Short bursts at uneven times, silence between.
    const int n = ms (1200);
    std::vector<float> in (static_cast<std::size_t> (n), 0.0f);
    const std::array<int, 3> attacks { ms (200), ms (523), ms (826) };
    for (int a : attacks)
    {
        const auto burst = test_support::sine (ms (30), 440.0, kHost, 0.5f);
        std::copy (burst.begin(), burst.end(), in.begin() + a);
    }

    auto spliceStartsNear = [&] (double delaySeconds, Sides sides = Sides::Left)
    {
        Rig rig (in);
        rig.p.keys.reverseSynchro      = sides;
        rig.p.keys.thresholdDb         = -20.0;
        rig.p.keys.reverseDelaySeconds = delaySeconds;
        rig.p.left.crosspoint1 = 0.3;   // deeper than crosspoint 2: reverse
        rig.p.left.crosspoint2 = 0.0;
        rig.set();

        std::vector<int> starts;
        bool was = false;
        while (rig.pos < n)
        {
            rig.runTo (rig.pos + 32);
            const bool now = rig.e.machine().voice (0).traversal().splicing();
            if (now && ! was)
                starts.push_back (rig.pos);
            was = now;
        }

        int hits = 0;
        const int lag = static_cast<int> (delaySeconds * kHost);
        for (int a : attacks)
            for (int s : starts)
                if (s >= a + lag && s <= a + lag + ms (4))
                {
                    ++hits;
                    break;
                }
        return hits;
    };

    REQUIRE (spliceStartsNear (0.0) == 3);
    REQUIRE (spliceStartsNear (0.1) == 3);

    // Left to itself the traversal splices on its own schedule, not the input's.
    REQUIRE (spliceStartsNear (0.0, Sides::Off) < 2);
}

TEST_CASE ("the noise gate mutes a side while its input is below threshold", "[keys][engine]")
{
    std::vector<float> in = test_support::sine (ms (800), 440.0, kHost, 0.5f);
    for (int i = ms (400); i < ms (800); ++i)
        in[static_cast<std::size_t> (i)] *= 0.01f;   // -46 dB below the loud part

    Rig rig (in);
    rig.p.keys.reverseSynchro = Sides::Left;
    rig.p.keys.noiseGate      = true;
    rig.p.keys.thresholdDb    = -20.0;
    rig.p.left.crosspoint2 = rig.p.right.crosspoint2 = 0.1;
    rig.set();
    rig.runTo (ms (800));

    REQUIRE (rig.peak (rig.l, ms (200), ms (400)) > 0.3f);
    REQUIRE (rig.peak (rig.l, ms (600), ms (800)) < 1e-4f);
    REQUIRE (rig.peak (rig.r, ms (600), ms (800)) > 1e-3f);   // the right has no gate
}

TEST_CASE ("keyboard vibrato swings the pitch; its modulator grows with the note", "[keys][engine]")
{
    auto swing = [] (double baseDepth, double modDepth, double fromMs, double toMs)
    {
        Rig rig (std::vector<float> (static_cast<std::size_t> (ms (toMs)), 0.0f));
        rig.p.keys.channels  = KeyChannels::Left;
        rig.p.keys.vibrato   = true;
        rig.p.keys.vibRateHz = 6.0;
        rig.p.keys.vibDepth  = baseDepth;
        rig.p.keys.vibModDepth = modDepth;
        rig.p.keys.vibAttackSeconds = 0.5;
        rig.set();
        rig.e.noteOn (60);

        double lo = 10.0, hi = 0.0;
        while (rig.pos < ms (toMs))
        {
            rig.runTo (rig.pos + 32);
            if (rig.pos >= ms (fromMs))
            {
                const double st = 12.0 * std::log2 (rig.e.machine().params().left.pitchRatio);
                lo = std::min (lo, st);
                hi = std::max (hi, st);
            }
        }
        return std::pair { lo, hi };
    };

    const auto plain = swing (1.0, 0.0, 100.0, 600.0);
    REQUIRE (plain.first  == Approx (-1.0).margin (0.05));
    REQUIRE (plain.second == Approx (1.0).margin (0.05));

    // No base depth, the modulator pulling it to +1 st over the 0.5 s attack.
    const auto early = swing (0.0, 0.5, 0.0, 120.0);
    const auto later = swing (0.0, 0.5, 600.0, 1000.0);
    REQUIRE (early.second < 0.35);
    REQUIRE (later.second == Approx (1.0).margin (0.05));
}
