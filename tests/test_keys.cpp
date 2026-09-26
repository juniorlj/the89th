#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/Engine.hpp>
#include <the89th/Keys.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

TEST_CASE ("held keys: the newest sounds, lifting it falls back", "[keys]")
{
    keys::NoteStack s;
    REQUIRE_FALSE (s.active());
    REQUIRE (s.current() == -1);

    s.press (60);
    s.press (64);
    s.press (67);
    REQUIRE (s.current() == 67);

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

namespace
{
constexpr double kHost = 48000.0;

void run (Engine& e, std::vector<float>& l, std::vector<float>& r, int from, int n)
{
    for (int pos = from; pos < from + n; pos += 256)
    {
        float* io[2] = { l.data() + pos, r.data() + pos };
        e.process (io, 2, std::min (256, from + n - pos));
    }
}
} // namespace

TEST_CASE ("a closed gate mutes the wet output without a click", "[keys][engine]")
{
    Engine e;
    e.prepare (kHost, 256);
    EngineParams p;
    p.left.crosspoint2 = p.right.crosspoint2 = 0.1;   // ~31 ms
    e.setParams (p);

    const int n = static_cast<int> (kHost);
    auto l = test_support::sine (n, 330.0, kHost, 0.5f);
    auto r = l;

    run (e, l, r, 0, n / 2);
    float before = 0.0f;
    for (int i = n / 2 - 4800; i < n / 2; ++i)
        before = std::max (before, std::fabs (l[static_cast<std::size_t> (i)]));

    p.gate = { 0.0, 1.0 };
    e.setParams (p);
    run (e, l, r, n / 2, n / 2);

    // 5 ms later the left wet is gone; the right carries on.
    float left = 0.0f, right = 0.0f;
    for (int i = n / 2 + 480; i < n; ++i)
    {
        left  = std::max (left,  std::fabs (l[static_cast<std::size_t> (i)]));
        right = std::max (right, std::fabs (r[static_cast<std::size_t> (i)]));
    }
    REQUIRE (before > 0.3f);
    REQUIRE (left  < 1e-6f);
    REQUIRE (right > 0.3f);

    // The fade is smooth: no step larger than the tone's own.
    const double natural = 0.5 * 2.0 * std::sin (M_PI * 330.0 / kHost);
    REQUIRE (test_support::maxAbsDelta (l.data(), n / 2 - 100, n / 2 + 600) < 1.3 * natural);
}

TEST_CASE ("an open gate changes nothing", "[keys][engine]")
{
    auto render = [] (bool touchGate)
    {
        Engine e;
        e.prepare (kHost, 256);
        EngineParams p;
        p.left.pitchRatio = p.right.pitchRatio = 1.5;
        p.left.crosspoint2 = p.right.crosspoint2 = 0.3;
        if (touchGate)
            p.gate = { 1.0, 1.0 };
        e.setParams (p);
        const int n = 24000;
        auto l = test_support::noise (n, 3u, 0.5f);
        auto r = l;
        run (e, l, r, 0, n);
        return l;
    };
    REQUIRE (render (false) == render (true));
}

TEST_CASE ("with no pitch glide, a new pitch lands at once", "[keys][engine]")
{
    auto landedAfter = [] (double glideSeconds)
    {
        Engine e;
        e.prepare (kHost, 256);
        EngineParams p;
        e.setParams (p);
        std::vector<float> l (4096, 0.0f), r = l;
        run (e, l, r, 0, 1024);

        p.left.pitchRatio = 2.0;
        p.pitchGlideSeconds = { glideSeconds, glideSeconds };
        e.setParams (p);
        run (e, l, r, 1024, 64);
        return e.machine().params().left.pitchRatio;
    };

    REQUIRE (landedAfter (0.0) == 2.0);
    REQUIRE (landedAfter (EngineParams::kDefaultPitchGlide) < 1.9);   // still gliding
}
