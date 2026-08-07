// Headless renderer: WAV in, WAV out, no DAW and no JUCE.

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <the89th/Engine.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

struct Options
{
    std::string inPath, outPath;
    the89th::EngineParams params {};
    int  blockSize = 512;
    bool freezeAfterFill = false;
};

void usage()
{
    std::fprintf (stderr,
        "the89th-render - headless renderer\n\n"
        "  the89th-render <in.wav> <out.wav> [options]\n\n"
        "  --pitch <r>       pitch ratio magnitude, 0.25 to 2.0   (default 1.0)\n"
        "  --xp1 <v>         crosspoint 1, 0 to 1                 (default 0.0)\n"
        "  --xp2 <v>         crosspoint 2, 0 to 1                 (default 1.0)\n"
        "  --feedback <v>    0 to 0.99                            (default 0.0)\n"
        "  --mix <v>         0 dry to 1 wet                       (default 1.0)\n"
        "  --bandwidth <n>   5, 10 or 20 kHz                      (default 10)\n"
        "  --freeze          latch memory from the start\n"
        "  --freeze-after    latch once the input has played through\n"
        "  --block <n>       host block size                      (default 512)\n\n"
        "Crosspoint 1 deeper than crosspoint 2 plays the region in reverse.\n"
        "20 kHz is unreachable in true stereo and falls back to 10 kHz.\n");
}

bool parseDouble (const char* s, double& out)
{
    char* end = nullptr;
    const double v = std::strtod (s, &end);
    if (end == s || *end != '\0')
        return false;
    out = v;
    return true;
}

bool parse (int argc, char** argv, Options& o)
{
    if (argc < 3)
        return false;

    o.inPath  = argv[1];
    o.outPath = argv[2];

    auto& L = o.params.left;
    auto& R = o.params.right;

    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        const bool hasValue = (i + 1 < argc);
        double v = 0.0;

        auto takeValue = [&] () -> bool
        {
            if (! hasValue || ! parseDouble (argv[i + 1], v))
            {
                std::fprintf (stderr, "the89th-render: %s needs a number\n", a.c_str());
                return false;
            }
            ++i;
            return true;
        };

        if (a == "--pitch")
        {
            if (! takeValue()) return false;
            L.pitchRatio = R.pitchRatio = v;
        }
        else if (a == "--xp1")
        {
            if (! takeValue()) return false;
            L.crosspoint1 = R.crosspoint1 = v;
        }
        else if (a == "--xp2")
        {
            if (! takeValue()) return false;
            L.crosspoint2 = R.crosspoint2 = v;
        }
        else if (a == "--feedback")
        {
            if (! takeValue()) return false;
            L.feedback = R.feedback = v;
        }
        else if (a == "--mix")
        {
            if (! takeValue()) return false;
            o.params.mix = v;
        }
        else if (a == "--block")
        {
            if (! takeValue()) return false;
            o.blockSize = static_cast<int> (v);
        }
        else if (a == "--bandwidth")
        {
            if (! takeValue()) return false;
            if      (v == 5.0)  o.params.bandwidth = the89th::Bandwidth::k5kHz;
            else if (v == 10.0) o.params.bandwidth = the89th::Bandwidth::k10kHz;
            else if (v == 20.0) o.params.bandwidth = the89th::Bandwidth::k20kHz;
            else { std::fprintf (stderr, "the89th-render: --bandwidth must be 5, 10 or 20\n"); return false; }
        }
        else if (a == "--freeze")
        {
            L.freeze = R.freeze = true;
        }
        else if (a == "--freeze-after")
        {
            o.freezeAfterFill = true;
        }
        else
        {
            std::fprintf (stderr, "the89th-render: unknown option %s\n", a.c_str());
            return false;
        }
    }

    return true;
}

const char* bandwidthName (the89th::Bandwidth b)
{
    switch (b)
    {
        case the89th::Bandwidth::k5kHz:  return "5 kHz";
        case the89th::Bandwidth::k10kHz: return "10 kHz";
        case the89th::Bandwidth::k20kHz: return "20 kHz";
    }
    return "?";
}

} // namespace

int main (int argc, char** argv)
{
    Options o;
    if (! parse (argc, argv, o))
    {
        usage();
        return 1;
    }

    unsigned int channels = 0, sampleRate = 0;
    drwav_uint64 frameCount = 0;

    float* raw = drwav_open_file_and_read_pcm_frames_f32 (
        o.inPath.c_str(), &channels, &sampleRate, &frameCount, nullptr);

    if (raw == nullptr)
    {
        std::fprintf (stderr, "the89th-render: cannot read %s\n", o.inPath.c_str());
        return 1;
    }

    const int frames = static_cast<int> (frameCount);
    const int inCh   = static_cast<int> (channels);
    const int outCh  = 2;

    // Deinterleave, promoting mono to both channels.
    std::vector<std::vector<float>> planar (static_cast<std::size_t> (outCh),
                                            std::vector<float> (static_cast<std::size_t> (frames), 0.0f));
    for (int i = 0; i < frames; ++i)
        for (int ch = 0; ch < outCh; ++ch)
            planar[static_cast<std::size_t> (ch)][static_cast<std::size_t> (i)]
                = raw[static_cast<std::size_t> (i) * channels
                      + static_cast<std::size_t> (inCh > ch ? ch : 0)];

    drwav_free (raw, nullptr);

    the89th::Engine engine;
    engine.prepare (static_cast<double> (sampleRate), o.blockSize);
    engine.setParams (o.params);

    // A tail so freeze and feedback have somewhere to sound.
    const int tail  = static_cast<int> (sampleRate) * 4;
    const int total = frames + tail;

    std::vector<float> l (static_cast<std::size_t> (o.blockSize), 0.0f);
    std::vector<float> r (static_cast<std::size_t> (o.blockSize), 0.0f);
    float* io[2] = { l.data(), r.data() };

    std::vector<float> interleaved (static_cast<std::size_t> (total) * 2, 0.0f);

    bool latched = false;
    for (int pos = 0; pos < total; pos += o.blockSize)
    {
        const int n = std::min (o.blockSize, total - pos);

        for (int i = 0; i < n; ++i)
        {
            const int src = pos + i;
            const bool inRange = src < frames;
            l[static_cast<std::size_t> (i)] = inRange ? planar[0][static_cast<std::size_t> (src)] : 0.0f;
            r[static_cast<std::size_t> (i)] = inRange ? planar[1][static_cast<std::size_t> (src)] : 0.0f;
        }

        if (o.freezeAfterFill && ! latched && pos >= frames)
        {
            o.params.left.freeze = o.params.right.freeze = true;
            latched = true;
        }

        engine.setParams (o.params);
        engine.process (io, 2, n);

        for (int i = 0; i < n; ++i)
        {
            const auto k = static_cast<std::size_t> (pos + i) * 2;
            interleaved[k]     = l[static_cast<std::size_t> (i)];
            interleaved[k + 1] = r[static_cast<std::size_t> (i)];
        }
    }

    drwav_data_format fmt {};
    fmt.container     = drwav_container_riff;
    fmt.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    fmt.channels      = 2;
    fmt.sampleRate    = sampleRate;
    fmt.bitsPerSample = 32;

    drwav out;
    if (! drwav_init_file_write (&out, o.outPath.c_str(), &fmt, nullptr))
    {
        std::fprintf (stderr, "the89th-render: cannot write %s\n", o.outPath.c_str());
        return 1;
    }

    drwav_write_pcm_frames (&out, static_cast<drwav_uint64> (total), interleaved.data());
    drwav_uninit (&out);

    const the89th::Spec spec {};
    std::fprintf (stderr,
        "the89th-render: %d frames in, %d out at %u Hz\n"
        "  pitch %.4f  xp1 %.3f  xp2 %.3f  %s\n"
        "  bandwidth %s requested, %s effective, internal %.1f Hz\n"
        "  feedback %.2f  mix %.2f  memory %d words/channel\n",
        frames, total, sampleRate,
        o.params.left.pitchRatio, o.params.left.crosspoint1, o.params.left.crosspoint2,
        o.params.left.crosspoint1 > o.params.left.crosspoint2 ? "reverse" : "forward",
        bandwidthName (o.params.bandwidth),
        bandwidthName (spec.effectiveBandwidth (o.params.bandwidth)),
        engine.channel (0).internalSampleRate(),
        o.params.left.feedback, o.params.mix, spec.memoryWordsPerChannel());

    return 0;
}
