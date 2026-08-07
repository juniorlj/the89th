#pragma once

#include "DelayMemory.hpp"
#include "Interpolation.hpp"
#include "Params.hpp"
#include "Quantiser.hpp"
#include "Spec.hpp"
#include "SpliceTraversal.hpp"

namespace the89th
{

/** One channel: memory, a fixed-rate write head, two variable-rate read heads.

    Runs at the internal converter rate. Rate conversion to and from the host
    lives in RateAdapter, deliberately outside, so this class can be driven
    sample-accurately in tests without a resampler in the path. */
template <class Interp = CatmullRom, class Quant = NoQuantiser>
class ChannelEngine
{
public:
    void prepare (const Spec& spec)
    {
        spec_ = spec;
        memory_.setSize (spec.memoryWordsPerChannel());
        traversal_.setCrossfadeLength (spec.crossfadeSamples);
        setBandwidth (requested_);
        reset();
    }

    void reset()
    {
        memory_.clear();
        traversal_.reset();
        feedbackState_ = 0.0f;
        applyRegion();
    }

    /** Changes the clock and nothing else. Memory contents, the write pointer
        and both head delays are left exactly where they are, which is what
        turns a bandwidth switch into an octave jump. */
    void setBandwidth (Bandwidth requested) noexcept
    {
        requested_ = requested;
        effective_ = spec_.effectiveBandwidth (requested);
    }

    Bandwidth requestedBandwidth() const noexcept { return requested_; }
    Bandwidth effectiveBandwidth() const noexcept { return effective_; }
    double    internalSampleRate() const noexcept { return internalRate (spec_, requested_); }

    void setParams (const ChannelParams& p) noexcept
    {
        params_ = p;
        traversal_.setRatio (p.pitchRatio);
        traversal_.setWriteRate (p.freeze ? 0.0 : 1.0);
        memory_.setWriteHeld (p.freeze);
        applyRegion();
    }

    float processSample (float in) noexcept
    {
        const float toStore = in + static_cast<float> (params_.feedback) * feedbackState_;
        memory_.write (Quant::store (toStore));

        const Tap a = traversal_.primary();
        const Tap b = traversal_.secondary();

        float wet = a.gain * Quant::load (memory_.template read<Interp> (a.delaySamples));

        if (b.gain != 0.0f)
            wet += b.gain * Quant::load (memory_.template read<Interp> (b.delaySamples));

        traversal_.advance();
        feedbackState_ = wet;
        return wet;
    }

    void process (const float* in, float* out, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            out[i] = processSample (in[i]);
    }

    const SpliceTraversal& traversal() const noexcept { return traversal_; }
    const DelayMemory&     memory()    const noexcept { return memory_; }
    const Spec&            spec()      const noexcept { return spec_; }

private:
    using Tap = SpliceTraversal::Tap;

    /** Normalised crosspoints map onto the usable delay span. Both endpoints
        land on exact integers, so crosspoint 2 at 1.0 gives an integer delay and
        the transparency test can assert bit equality rather than a tolerance. */
    void applyRegion() noexcept
    {
        const double base = static_cast<double> (DelayMemory::kMinDelay);
        const double span = static_cast<double> (memory_.words() - DelayMemory::kEndGuard)
                          - base;

        traversal_.setRegion (base + params_.crosspoint1 * span,
                              base + params_.crosspoint2 * span);
    }

    Spec            spec_ {};
    DelayMemory     memory_;
    SpliceTraversal traversal_;
    ChannelParams   params_ {};
    Bandwidth       requested_ = Bandwidth::k20kHz;
    Bandwidth       effective_ = Bandwidth::k10kHz;
    float           feedbackState_ = 0.0f;
};

using DefaultChannelEngine = ChannelEngine<CatmullRom, NoQuantiser>;

} // namespace the89th
