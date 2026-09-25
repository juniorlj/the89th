#pragma once

#include "DelayMemory.hpp"
#include "Interpolation.hpp"
#include "Params.hpp"
#include "Quantiser.hpp"
#include "ReadVoice.hpp"
#include "Spec.hpp"

namespace the89th
{

/** One channel on its own: memory, write head, converter, read voice, feedback.

    Runs at the internal converter rate with no resampler in the path, so tests
    can drive the delay path sample-accurately. The real engine is Machine, which
    is built from the same memory and voice and adds the two stereo layouts. */
template <class Interp = Truncate, class Quant = FlyingComma>
class ChannelEngine
{
public:
    void prepare (const Spec& spec)
    {
        spec_ = spec;
        memory_.setSize (spec.memoryWordsPerChannel());
        voice_.prepare (memory_.words(), spec.crossfadeSamples);
        setBandwidth (requested_);
        reset();
    }

    void reset()
    {
        memory_.clear();
        feedbackState_ = 0.0f;
        voice_.setParams (params_);
        voice_.reset();
    }

    /** Clock only. Memory, write pointer and head positions stay where they
        are, which is what turns a bandwidth switch into an octave jump. */
    void setBandwidth (Bandwidth requested) noexcept
    {
        requested_ = requested;
        effective_ = spec_.effectiveBandwidth (requested);
        voice_.setSampleRate (internalSampleRate());
    }

    Bandwidth requestedBandwidth() const noexcept { return requested_; }
    Bandwidth effectiveBandwidth() const noexcept { return effective_; }
    double    internalSampleRate() const noexcept { return internalRate (spec_, requested_); }

    void setMode  (Mode m)       noexcept { voice_.setMode (m); }
    void setRange (DelayRange r) noexcept { voice_.setRange (r); }
    void setXing  (bool on)      noexcept { voice_.setXing (on); }

    void setParams (const ChannelParams& p) noexcept
    {
        params_ = p;
        memory_.setWriteHeld (p.freeze);
        voice_.setParams (p);
    }

    float processSample (float in) noexcept
    {
        memory_.write (Quant::store (in + static_cast<float> (params_.feedback) * feedbackState_));

        const float wet = Quant::load (voice_.read (memory_));
        voice_.advance (memory_);

        feedbackState_ = wet;
        return wet;
    }

    void process (const float* in, float* out, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            out[i] = processSample (in[i]);
    }

    const SpliceTraversal&   traversal() const noexcept { return voice_.traversal(); }
    const ReadVoice<Interp>& voice()     const noexcept { return voice_; }
    const DelayMemory&       memory()    const noexcept { return memory_; }
    const Spec&              spec()      const noexcept { return spec_; }

private:
    Spec              spec_ {};
    DelayMemory       memory_;
    ReadVoice<Interp> voice_;
    ChannelParams     params_ {};
    Bandwidth         requested_ = Bandwidth::k10kHz;
    Bandwidth         effective_ = Bandwidth::k10kHz;
    float             feedbackState_ = 0.0f;
};

using DefaultChannelEngine = ChannelEngine<Truncate, FlyingComma>;

} // namespace the89th
