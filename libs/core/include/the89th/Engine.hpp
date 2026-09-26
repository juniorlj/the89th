#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

#include "BandLimit.hpp"
#include "Glide.hpp"
#include "Keyboard.hpp"
#include "Machine.hpp"
#include "Params.hpp"
#include "Resampler.hpp"
#include "Scrub.hpp"
#include "Spec.hpp"

namespace the89th
{

/** The machine as a host sees it. Per channel, in signal order:

        in -> [pre-emphasis, in delay mode] -> anti-alias filter
           -> down to the internal clock -> Machine -> up to host rate
           -> reconstruction filter -> [de-emphasis, in delay mode] -> mix -> out

    The filters and emphasis stand in for the analog stages either side of the
    converter, so they run at host rate. The machine runs at its own clock.

    Tests of the delay path bypass all of this and drive ChannelEngine directly,
    because a host round trip goes through filters and a resampler and can never
    be bit-exact.

    Continuous controls glide here, at the host boundary, so the Machine below
    stays a 1:1 model that takes whatever it is given. Pitch, feedback, vibrato
    depth, mix and the feedback tone move to a new setting over kGlideSeconds
    instead of jumping once per host block. The scrub lives here too: it moves
    the crosspoint region, every kGlideChunk samples while it runs. Delay needs no glide: the voice already crossfades to a
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

        // One latency for every clock: the slowest one's, rounded up to whole
        // samples. A host has to restart a plugin to take a new latency, and a
        // restart empties the memory, which would turn a bandwidth switch's
        // octave jump into a gap.
        const double slowest = spec.converterClockHz / rateDivisor (Bandwidth::k5kHz);
        latency_ = static_cast<int> (std::ceil (RateAdapter::naturalLatency (hostRate_, slowest)));
        adapter_.setFixedLatency (latency_);
        for (auto& d : dryDelay_)
            d.assign (static_cast<std::size_t> (latency_), 0.0f);

        const auto block = static_cast<std::size_t> (maxBlock_);
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            dry_[ch].assign (block, 0.0f);
            pre_[ch].assign (block, 0.0f);
            wet_[ch].assign (block, 0.0f);
            emphasis_[ch].design (hostRate_);
        }
        mixGain_.assign (block, 0.0f);
        for (auto& g : keyGain_)
            g.assign (block, 1.0f);

        const int glideLen = static_cast<int> (std::lround (kGlideSeconds * hostRate_));
        forEachGlide ([glideLen] (Glide& g) { g.setLength (glideLen); });
        keyboard_.prepare (hostRate_);
        keyboard_.setParams (params_.keys);
        pitchGlideSet_ = { -1.0, -1.0 };
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
        for (auto& d : dryDelay_)
            std::fill (d.begin(), d.end(), 0.0f);
        dryPos_ = 0;
        forEachGlide ([] (Glide& g) { g.snap (g.target()); });
        keyboard_.reset();
        scrub_.reset();
        scrubPos_ = 0.0;
        resolve();
        machine_.setParams (glided());
    }

    void setParams (const EngineParams& p)
    {
        const std::array<bool, kNumChannels> emphasisWas { emphasisOn (0), emphasisOn (1) };
        params_ = p;
        keyboard_.setParams (p.keys);
        resolve();
        retarget();

        machine_.setParams (glided());
        adapter_.setInternalRate (machine_.internalSampleRate());
        updateFilters();

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
            if (emphasisOn (ch) != emphasisWas[ch])
                emphasis_[ch].reset();
    }

    void setXing (bool on) noexcept { machine_.setXing (on); }

    // The keyboard, as MIDI arrives. The host splits its block at each event,
    // so these land between process calls, on the sample they were played.
    void noteOn  (int note)  noexcept { keyboard_.noteOn (note); }
    void noteOff (int note)  noexcept { keyboard_.noteOff (note); }
    void pitchWheel (int v)  noexcept { keyboard_.pitchWheel (v); }
    void allNotesOff()       noexcept { keyboard_.allNotesOff(); }

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

    /** Whole host samples, the same at every clock, and shared by the dry path. */
    double latencySamples() const noexcept { return static_cast<double> (latency_); }

    const DefaultMachine& machine() const noexcept { return machine_; }
    const EngineParams&   params()  const noexcept { return params_; }

    /** What the machine is being asked for once the keyboard has had its say. */
    const EngineParams&   effective() const noexcept { return effective_; }
    const Keyboard&       keyboard()  const noexcept { return keyboard_; }
    double                bandEdgeHz() const noexcept { return edgeHz_; }

private:
    bool emphasisOn (std::size_t ch) const noexcept
    {
        return (ch == 0 ? effective_.left : effective_.right).mode == Mode::Delay;
    }

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
        fn (lowCut_);
        fn (highCut_);
        fn (drive_);
        fn (mix_);
    }

    /** Anything that has to reach the machine more often than once a block. */
    bool machineMoving() const noexcept
    {
        for (const auto& g : glides_)
            if (g.pitch.moving() || g.feedback.moving() || g.vibratoDepth.moving())
                return true;
        return lowCut_.moving() || highCut_.moving() || drive_.moving() || scrubbing()
            || keyboard_.active();
    }

    bool scrubbing() const noexcept { return params_.scrubDepth > 0.0; }

    bool gliding() const noexcept { return machineMoving() || mix_.moving(); }

    /** The host's settings with the keyboard's on top. */
    void resolve() noexcept
    {
        effective_ = params_;
        if (keyboard_.active())
            keyboard_.apply (effective_);
    }

    /** Points the glides at effective_. The first settings after prepare are
        where the host starts, not a move, so they land at once. */
    void retarget() noexcept
    {
        const ChannelParams* cs[kNumChannels] = { &effective_.left, &effective_.right };
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            // A keyboard channel glides between notes for the Slope time,
            // which can be none. Only reset the length when it changes, so a
            // glide in progress isn't disturbed by the host resending it.
            const double pg = keyboard_.drives (static_cast<int> (ch)) ? params_.keys.glideSeconds : kGlideSeconds;
            if (! std::equal_to<double> {} (pg, pitchGlideSet_[ch]))
            {
                glides_[ch].pitch.setLength (static_cast<int> (std::lround (std::max (pg, 0.0) * hostRate_)));
                pitchGlideSet_[ch] = pg;
            }

            glides_[ch].pitch.setTarget (std::log2 (std::max (cs[ch]->pitchRatio, 1e-6)));
            glides_[ch].feedback.setTarget (cs[ch]->feedback);
            glides_[ch].vibratoDepth.setTarget (cs[ch]->vibratoDepth);
        }
        mix_.setTarget (effective_.mix);
        lowCut_.setTarget (std::log2 (std::max (effective_.lowCutHz, 1.0)));
        highCut_.setTarget (std::log2 (std::max (effective_.highCutHz, 1.0)));
        drive_.setTarget (effective_.drive);

        if (! primed_)
        {
            forEachGlide ([] (Glide& g) { g.snap (g.target()); });
            primed_ = true;
        }
    }

    /** effective_ with each gliding control at its current point. A settled control
        passes the host's value straight through, untouched by the octave round
        trip, so settled output is bit-identical to having no glide at all. */
    EngineParams glided() const noexcept
    {
        EngineParams p = effective_;
        ChannelParams* cs[kNumChannels] = { &p.left, &p.right };
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            const auto& g = glides_[ch];
            if (g.pitch.moving())        cs[ch]->pitchRatio   = std::exp2 (g.pitch.current());

            if (params_.keys.vibrato && keyboard_.drives (static_cast<int> (ch)))
            {
                const double vib = keyboard_.vibratoSemitones (static_cast<int> (ch));
                cs[ch]->pitchRatio = std::clamp (cs[ch]->pitchRatio * std::exp2 (vib / 12.0),
                                                 musical::kMinRatio, musical::kMaxRatio);
            }
            if (g.feedback.moving())     cs[ch]->feedback     = g.feedback.current();
            if (g.vibratoDepth.moving()) cs[ch]->vibratoDepth = g.vibratoDepth.current();

            if (p.scrubDepth > 0.0)
            {
                // Slide the region as a whole, by up to its own length each
                // way, stopping at the ends of memory rather than squashing it.
                auto& c = *cs[ch];
                const double lo    = std::min (c.crosspoint1, c.crosspoint2);
                const double hi    = std::max (c.crosspoint1, c.crosspoint2);
                const double shift = std::clamp (p.scrubDepth * (hi - lo) * scrubPos_, -lo, 1.0 - hi);
                c.crosspoint1 += shift;
                c.crosspoint2 += shift;
            }
        }
        if (lowCut_.moving())  p.lowCutHz  = std::exp2 (lowCut_.current());
        if (highCut_.moving()) p.highCutHz = std::exp2 (highCut_.current());
        if (drive_.moving())   p.drive     = drive_.current();
        return p;
    }

    void processChunk (float* const* io, int numChannels, int numSamples)
    {
        const auto len = static_cast<std::size_t> (numSamples);

        // A mono host feeds the same signal to both sides.
        const float* src[2] = { io[0], numChannels > 1 ? io[1] : io[0] };

        // The keyboard runs first: its envelopes, synchros and detector decide
        // this chunk's pitch, latch and region.
        const bool keys = keyboard_.active();
        if (keys)
        {
            const auto& m = machine_;
            float* gains[kNumChannels] = { keyGain_[0].data(), keyGain_[1].data() };
            const double span = static_cast<double> (m.wordsPerVoice() - DelayMemory::kEndGuard - DelayMemory::kMinDelay);
            keyboard_.process (src, gains, numSamples,
                               { m.memory (0).writeHeld(), m.memory (m.quasi() ? 0 : 1).writeHeld() },
                               { m.params().left.pitchRatio, m.params().right.pitchRatio },
                               m.internalSampleRate(), span);
            resolve();
            retarget();
        }

        const bool machineGliding = machineMoving();
        for (auto& g : glides_)
            for (Glide* gl : { &g.pitch, &g.feedback, &g.vibratoDepth })
                gl->advance (numSamples);
        for (Glide* gl : { &lowCut_, &highCut_, &drive_ })
            gl->advance (numSamples);
        if (scrubbing())
            scrubPos_ = scrub_.advance (numSamples, params_.scrubRate, hostRate_, params_.scrubMode);
        if (machineGliding)
            machine_.setParams (glided());

        if (keys)
            for (int ch = 0; ch < static_cast<int> (kNumChannels); ++ch)
                if (keyboard_.takeRestart (ch))
                    machine_.restart (ch);

        // Mix is applied here at host rate, so it can glide per sample.
        const bool mixGliding = mix_.moving();
        if (mixGliding)
            for (std::size_t i = 0; i < len; ++i)
                mixGain_[i] = static_cast<float> (mix_.advance (1));

        const std::array<bool, kNumChannels> emph { emphasisOn (0), emphasisOn (1) };
        const float wetG = static_cast<float> (mix_.current());
        const float dryG = 1.0f - wetG;

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            std::copy (src[ch], src[ch] + len, dry_[ch].begin());
            for (std::size_t i = 0; i < len; ++i)
            {
                float x = dry_[ch][i];
                if (emph[ch])
                    x = emphasis_[ch].pre (x);
                pre_[ch][i] = antiAlias_[ch].process (x);
            }
        }

        adapter_.process (pre_[0].data(), pre_[1].data(), wet_[0].data(), wet_[1].data(), numSamples,
                          [this] (float l, float r, float& ol, float& orr) noexcept
                          { machine_.step (l, r, ol, orr); });

        // The host shifts this plugin's output earlier by the reported latency,
        // so the dry signal is held back by the same amount to stay in time.
        const auto delayLen = static_cast<std::size_t> (latency_);
        for (std::size_t ch = 0; ch < kNumChannels && delayLen > 0; ++ch)
        {
            auto& line = dryDelay_[ch];
            std::size_t pos = dryPos_;
            for (std::size_t i = 0; i < len; ++i)
            {
                const float held = line[pos];
                line[pos] = dry_[ch][i];
                dry_[ch][i] = held;
                if (++pos == delayLen)
                    pos = 0;
            }
        }
        if (delayLen > 0)
            dryPos_ = (dryPos_ + len) % delayLen;

        const auto outs = std::min (static_cast<std::size_t> (numChannels), kNumChannels);
        for (std::size_t ch = 0; ch < outs; ++ch)
        {
            for (std::size_t i = 0; i < len; ++i)
            {
                float y = reconstruct_[ch].process (wet_[ch][i]);
                if (emph[ch])
                    y = emphasis_[ch].de (y);
                if (keys)
                    y *= keyGain_[ch][i];  // the keyboard's VCA, and the gate
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
        for (auto& g : keyGain_)
            g.assign (need, 1.0f);
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

    int latency_ = 0;
    std::array<std::vector<float>, kNumChannels> dryDelay_ {};
    std::size_t dryPos_ = 0;

    std::array<ChannelGlides, kNumChannels> glides_ {};
    Glide lowCut_, highCut_;  // in octaves
    Glide drive_;
    Glide mix_;
    std::array<std::vector<float>, kNumChannels> keyGain_ {};
    std::array<double, kNumChannels> pitchGlideSet_ { -1.0, -1.0 };

    Keyboard     keyboard_;
    EngineParams effective_ {};

    ScrubLfo scrub_;
    double   scrubPos_ = 0.0;  // -1..1
    bool  primed_ = false;
};

} // namespace the89th
