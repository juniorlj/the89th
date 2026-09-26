#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <the89th/ChannelEngine.hpp>
#include <the89th/Engine.hpp>
#include <the89th/Glide.hpp>

#include "TestSupport.hpp"

using namespace the89th;
using Catch::Approx;

namespace
{
constexpr double kHost = 48000.0;
constexpr int    kBlock = 512;
const int        kGlideLen = static_cast<int> (Engine::kGlideSeconds * kHost);

/** Runs the engine for n samples of a steady tone, in host-sized blocks. */
void run (Engine& e, int n, std::vector<float>* capture = nullptr)
{
    std::vector<float> l (static_cast<std::size_t> (kBlock)), r (l.size());
    for (int done = 0; done < n; done += kBlock)
    {
        const int len = std::min (kBlock, n - done);
        for (int i = 0; i < len; ++i)
            l[static_cast<std::size_t> (i)] = r[static_cast<std::size_t> (i)]
                = 0.5f * static_cast<float> (std::sin (2.0 * M_PI * 440.0 * (done + i) / kHost));

        float* io[2] = { l.data(), r.data() };
        e.process (io, 2, len);

        if (capture != nullptr)
            capture->insert (capture->end(), l.begin(), l.begin() + len);
    }
}
} // namespace

TEST_CASE ("a glide moves in a straight line and lands exactly", "[glide]")
{
    Glide g;
    g.setLength (100);
    g.snap (0.3);
    REQUIRE_FALSE (g.moving());

    g.setTarget (1.7);
    REQUIRE (g.moving());
    REQUIRE (g.advance (25) == Approx (0.3 + 0.25 * 1.4));
    REQUIRE (g.advance (25) == Approx (0.3 + 0.50 * 1.4));

    // Overshooting the length still lands on the target, bit for bit.
    REQUIRE (g.advance (1000) == 1.7);
    REQUIRE_FALSE (g.moving());
}

TEST_CASE ("the first settings land at once rather than gliding from defaults", "[glide][engine]")
{
    Engine e;
    e.prepare (kHost, kBlock);

    EngineParams p;
    p.left.pitchRatio = 1.7;
    p.mix = 0.3;
    e.setParams (p);

    REQUIRE (e.machine().params().left.pitchRatio == 1.7);
    run (e, kBlock);
    REQUIRE (e.machine().params().left.pitchRatio == 1.7);
}

TEST_CASE ("pitch glides to a new setting instead of stepping per block", "[glide][engine]")
{
    Engine e;
    e.prepare (kHost, kBlock);

    EngineParams p;
    e.setParams (p);
    run (e, 20 * kBlock);

    p.left.pitchRatio = 2.0;
    e.setParams (p);

    // One glide chunk in, the heads run between the two settings.
    std::vector<float> l (static_cast<std::size_t> (Engine::kGlideChunk)), r (l.size());
    float* io[2] = { l.data(), r.data() };
    e.process (io, 2, Engine::kGlideChunk);

    const double rate = e.machine().voice (0).traversal().signedRate();
    REQUIRE (rate > 1.0);
    REQUIRE (rate < 1.1);

    // Once the glide is over the machine gets the host's number exactly, not a
    // value that went through the octave conversion and back.
    run (e, kGlideLen + kBlock);
    REQUIRE (e.machine().params().left.pitchRatio == 2.0);
    REQUIRE (e.machine().voice (0).traversal().signedRate() == 2.0);
}

TEST_CASE ("mix fades rather than switching", "[glide][engine]")
{
    // Latched on empty memory, so the wet path is exact silence and the output
    // is the dry signal scaled by whatever the mix currently is.
    Engine e;
    e.prepare (kHost, kBlock);

    EngineParams p;
    p.left.freeze = p.right.freeze = true;
    p.mix = 0.0;
    e.setParams (p);
    run (e, 4 * kBlock);

    p.mix = 1.0;
    e.setParams (p);

    std::vector<float> out;
    run (e, kGlideLen + 4 * kBlock, &out);

    // The dry gain at sample i is the tone's envelope: 1 - (i + 1) / length.
    // The dry path runs the reported latency behind the input, and run()
    // starts its tone from phase zero on each call.
    const int lat = static_cast<int> (e.latencySamples());
    for (int i : { lat + 1, kGlideLen / 4, kGlideLen / 2, (3 * kGlideLen) / 4 })
    {
        const double in  = 0.5 * std::sin (2.0 * M_PI * 440.0 * (i - lat) / kHost);
        if (std::abs (in) < 0.1)
            continue;

        const double gain = out[static_cast<std::size_t> (i)] / in;
        INFO ("sample " << i);
        REQUIRE (gain == Approx (1.0 - (i + 1.0) / kGlideLen).margin (1e-4));
    }

    for (std::size_t i = static_cast<std::size_t> (kGlideLen); i < out.size(); ++i)
        REQUIRE (out[i] == 0.0f);
}

TEST_CASE ("moving a crosspoint past the head splices instead of clicking", "[glide][splice]")
{
    constexpr double kFs = 26455.0;

    // Unity forward: the head never moves on its own, so it stays parked on
    // crosspoint 2 until the region is pulled out from under it.
    ChannelEngine<CatmullRom, NoQuantiser> eng;
    eng.prepare (Spec {});

    ChannelParams p;
    p.crosspoint1 = 0.0;
    p.crosspoint2 = 1.0;
    eng.setParams (p);
    eng.reset();

    const int n = 30000, moveAt = 20000;
    const auto in = test_support::sine (n, 440.0, kFs, 0.9f);
    std::vector<float> out (static_cast<std::size_t> (n));

    for (int i = 0; i < n; ++i)
    {
        if (i == moveAt)
        {
            const double before = eng.traversal().primaryDelay();
            p.crosspoint2 = 0.3;
            eng.setParams (p);

            // Out of the region now, but still reading where it was: the
            // crossfade moves it, not a jump.
            REQUIRE (eng.traversal().splicing());
            REQUIRE (eng.traversal().primaryDelay() == before);
        }
        out[static_cast<std::size_t> (i)] = eng.processSample (in[static_cast<std::size_t> (i)]);
    }

    REQUIRE_FALSE (eng.traversal().splicing());
    REQUIRE (eng.traversal().primaryDelay() <= eng.traversal().regionHi());

    const double natural = 0.9 * 2.0 * std::sin (M_PI * 440.0 / kFs);
    const float observed = test_support::maxAbsDelta (out.data(), moveAt - 200, moveAt + 400);
    INFO ("observed " << observed << " natural " << natural);
    REQUIRE (static_cast<double> (observed) < 2.0 * natural);
}

TEST_CASE ("the dry signal arrives exactly on the reported latency", "[glide][engine][timing]")
{
    // The host moves this plugin's output earlier by the reported latency, so
    // dry has to be late by exactly that much to land back in time.
    for (double host : { 44100.0, 48000.0, 96000.0 })
    {
        Engine e;
        e.prepare (host, kBlock);

        EngineParams p;
        p.mix = 0.0;
        e.setParams (p);

        std::vector<float> l (static_cast<std::size_t> (4 * kBlock), 0.0f), r = l;
        l[10] = r[10] = 1.0f;
        for (int pos = 0; pos < 4 * kBlock; pos += kBlock)
        {
            float* io[2] = { l.data() + pos, r.data() + pos };
            e.process (io, 2, kBlock);
        }

        const auto at = static_cast<std::size_t> (10 + static_cast<int> (e.latencySamples()));
        INFO ("host " << host << ", latency " << e.latencySamples());
        REQUIRE (l[at] == 1.0f);
        REQUIRE (r[at] == 1.0f);
    }
}
