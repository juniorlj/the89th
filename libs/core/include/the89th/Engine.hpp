#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "BandLimit.hpp"
#include "Glide.hpp"
#include "Machine.hpp"
#include "Params.hpp"
#include "Resampler.hpp"
#include "Spec.hpp"

namespace the89th
{

/** The machine as a host sees it. Per channel, in signal order:

        in -> [pre-emphasis, delay mode] -> anti-alias filter
           -> down to the internal clock -> Machine -> up to host rate
           -> reconstruction filter -> [de-emphasis, delay mode] -> mix -> out

    The filters and emphasis stand in for the analog stages either side of the
    converter, so they run at host rate. The machine runs at its own clock.

    Tests of the delay path bypass all of this and drive ChannelEngine directly,
    because a host round trip goes through filters and a resampler and can never
    be bit-exact.

    Continuous controls glide here, at the host boundary, so the Machine below
    stays a 1:1 model that takes whatever it is given. Pitch, feedback, vibrato
    depth and mix move to a new setting over kGlideSeconds instead of jumping
    once per host block. Delay needs no glide: the voice already crossfades to a
    new delay. Crosspoints need none either: they bound the region rather than
    being heard, and a bound that moves past the head splices it back in. */
class Engine
{
public:
    static constexpr std::size_t kNumChannels = 2;

    /** Long enough to hide a block-sized step, short enough to feel immediate. */
    static constexpr double kGlideSeconds = 0.03;

    /** While anything glides, the block runs in pieces this long, each with its
        own settings, so a glide climbs in small steps rather than one per block. */
    static constexpr int kGlideChunk = 32;

    void prepare (double hostSampleRate, int maxBlockSize, const Spec& spec = {})
    {
        hostRate_ = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
        maxBlock_ = std::max (maxBlockSize, 1);

        machine_.prepare (spec);
        machine_.setParams (params_);
        adapter_.prepare (hostRate_, machine_.internalSampleRate());

        const auto block = static_cast<std::size_t> (maxBlock_);
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            dry_[ch].assign (block, 0.0f);
            pre_[ch].assign (block, 0.0f);
            wet_[ch].assign (block, 0.0f);
            emphasis_[ch].design (hostRate_);
        }
        mixGain_.assign (block, 0.0f);

        const int glideLen = static_cast<int> (std::lround (kGlideSeconds * hostRate_));
        forEachGlide ([glideLen] (Glide& g) { g.setLength (glideLen); });
        primed_ = false;

        filtersDesigned_ = false;
        updateFilters();
        reset();
    }

    void reset()
    {
        machine_.reset();
        adapter_.reset();
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            antiAlias_[ch].reset();
            reconstruct_[ch].reset();
            emphasis_[ch].reset();
        }
        forEachGlide ([] (Glide& g) { g.snap (g.target()); });
        machine_.setParams (glided());
    }

    void setParams (const EngineParams& p)
    {
        const bool emphasisWas = emphasisOn();
        params_ = p;
        retarget();

        machine_.setParams (glided());
        adapter_.setInternalRate (machine_.internalSampleRate());
        updateFilters();

        if (emphasisOn() != emphasisWas)
            for (auto& e : emphasis_)
                e.reset();
    }

    void setXing (bool on) noexcept { machine_.setXing (on); }

    void process (float* const* io, int numChannels, int numSamples)
    {
        if (numSamples <= 0 || numChannels <= 0)
            return;

        ensureCapacity (numSamples);

        const int chans = std::min (numChannels, static_cast<int> (kNumChannels));
        for (int done = 0; done < numSamples;)
        {
            const int len = gliding() ? std::min (kGlideChunk, numSamples - done) : numSamples - done;

            float* chunk[kNumChannels] = { io[0] + done, chans > 1 ? io[1] + done : io[0] + done };
            processChunk (chunk, chans, len);
            done += len;
        }
    }

    double latencySamples() const noexcept { return adapter_.latencySamples(); }

    const DefaultMachine& machine() const noexcept { return machine_; }
    const EngineParams&   params()  const noexcept { return params_; }
    double                bandEdgeHz() const noexcept { return edgeHz_; }
    const BandLimitFilter& antiAlias (int ch) const noexcept { return antiAlias_[static_cast<std::size_t> (ch)]; }

private:
    bool emphasisOn() const noexcept { return params_.mode == Mode::Delay; }

    struct ChannelGlides
    {
        Glide pitch;  // in octaves, so a glide sounds even up and down
        Glide feedback;
        Glide vibratoDepth;
    };

    template <class Fn>
    void forEachGlide (Fn&& fn)
    {
        for (auto& g : glides_)
        {
            fn (g.pitch);
            fn (g.feedback);
            fn (g.vibratoDepth);
        }
        fn (mix_);
    }

    bool gliding() const noexcept
    {
        for (const auto& g : glides_)
            if (g.pitch.moving() || g.feedback.moving() || g.vibratoDepth.moving())
                return true;
        return mix_.moving();
    }

    /** Points the glides at params_. The first settings after prepare are where
        the host starts, not a move, so they land at once. */
    void retarget() noexcept
    {
        const ChannelParams* cs[kNumChannels] = { &params_.left, &params_.right };
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            glides_[ch].pitch.setTarget (std::log2 (std::max (cs[ch]->pitchRatio, 1e-6)));
            glides_[ch].feedback.setTarget (cs[ch]->feedback);
            glides_[ch].vibratoDepth.setTarget (cs[ch]->vibratoDepth);
        }
        mix_.setTarget (params_.mix);

        if (! primed_)
        {
            forEachGlide ([] (Glide& g) { g.snap (g.target()); });
            primed_ = true;
        }
    }

    /** params_ with each gliding control at its current point. A settled control
        passes the host's value straight through, untouched by the octave round
        trip, so settled output is bit-identical to having no glide at all. */
    EngineParams glided() const noexcept
    {
        EngineParams p = params_;
        ChannelParams* cs[kNumChannels] = { &p.left, &p.right };
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            const auto& g = glides_[ch];
            if (g.pitch.moving())        cs[ch]->pitchRatio   = std::exp2 (g.pitch.current());
            if (g.feedback.moving())     cs[ch]->feedback     = g.feedback.current();
            if (g.vibratoDepth.moving()) cs[ch]->vibratoDepth = g.vibratoDepth.current();
        }
        return p;
    }

    void processChunk (float* const* io, int numChannels, int numSamples)
    {
        const auto len = static_cast<std::size_t> (numSamples);

        bool machineGliding = false;
        for (auto& g : glides_)
        {
            for (Glide* gl : { &g.pitch, &g.feedback, &g.vibratoDepth })
            {
                machineGliding = machineGliding || gl->moving();
                gl->advance (numSamples);
            }
        }
        if (machineGliding)
            machine_.setParams (glided());

        // Mix is applied here at host rate, so it can glide per sample.
        const bool mixGliding = mix_.moving();
        if (mixGliding)
            for (std::size_t i = 0; i < len; ++i)
                mixGain_[i] = static_cast<float> (mix_.advance (1));

        const bool  emph = emphasisOn();
        const float wetG = static_cast<float> (mix_.current());
        const float dryG = 1.0f - wetG;

        // A mono host feeds the same signal to both sides.
        const float* src[2] = { io[0], numChannels > 1 ? io[1] : io[0] };

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            std::copy (src[ch], src[ch] + len, dry_[ch].begin());
            for (std::size_t i = 0; i < len; ++i)
            {
                float x = dry_[ch][i];
                if (emph)
                    x = emphasis_[ch].pre (x);
                pre_[ch][i] = antiAlias_[ch].process (x);
            }
        }

        adapter_.process (pre_[0].data(), pre_[1].data(), wet_[0].data(), wet_[1].data(), numSamples,
                          [this] (float l, float r, float& ol, float& orr) noexcept
                          { machine_.step (l, r, ol, orr); });

        const auto outs = std::min (static_cast<std::size_t> (numChannels), kNumChannels);
        for (std::size_t ch = 0; ch < outs; ++ch)
        {
            for (std::size_t i = 0; i < len; ++i)
            {
                float y = reconstruct_[ch].process (wet_[ch][i]);
                if (emph)
                    y = emphasis_[ch].de (y);
                const float w = mixGliding ? mixGain_[i] : wetG;
                const float d = mixGliding ? 1.0f - w    : dryG;
                io[ch][i] = d * dry_[ch][i] + w * y;
            }
        }
    }

    /** Redesigns only when the band edge moves, keeping filter state otherwise. */
    void updateFilters()
    {
        const Bandwidth bw = machine_.effectiveBandwidth();
        if (filtersDesigned_ && bw == filterBw_)
            return;

        const double edge = bw == Bandwidth::k5kHz  ? 5000.0
                          : bw == Bandwidth::k20kHz ? 20000.0
                                                    : 10000.0;
        filtersDesigned_ = true;
        filterBw_ = bw;
        edgeHz_   = edge;
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            antiAlias_[ch].design (hostRate_, edge);
            reconstruct_[ch].design (hostRate_, edge);
        }
    }

    /** Only fires if a host breaks its own maxBlockSize contract. */
    void ensureCapacity (int numSamples)
    {
        const auto need = static_cast<std::size_t> (numSamples);
        if (need <= dry_.front().size())
            return;

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            dry_[ch].assign (need, 0.0f);
            pre_[ch].assign (need, 0.0f);
            wet_[ch].assign (need, 0.0f);
        }
        mixGain_.assign (need, 0.0f);
        maxBlock_ = numSamples;
    }

    EngineParams   params_ {};
    DefaultMachine machine_;
    RateAdapter    adapter_;

    std::array<BandLimitFilter, kNumChannels> antiAlias_ {}, reconstruct_ {};
    std::array<Emphasis, kNumChannels>        emphasis_ {};

    double    hostRate_ = 48000.0;
    double    edgeHz_   = 10000.0;
    Bandwidth filterBw_ = Bandwidth::k10kHz;
    bool      filtersDesigned_ = false;
    int    maxBlock_ = 512;

    std::array<std::vector<float>, kNumChannels> dry_ {}, pre_ {}, wet_ {};
    std::vector<float> mixGain_;

    std::array<ChannelGlides, kNumChannels> glides_ {};
    Glide mix_;
    bool  primed_ = false;
};

} // namespace the89th
