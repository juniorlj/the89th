#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "ChannelEngine.hpp"
#include "Params.hpp"
#include "Resampler.hpp"
#include "Spec.hpp"

namespace the89th
{

/** Two channels, rate conversion, dry/wet. What the plugin and the CLI drive.

    Tests bypass this and talk to ChannelEngine directly, because a host round
    trip goes through a resampler and can never be bit-exact. Transparency is a
    property of the delay path, so that is where it gets asserted. */
class Engine
{
public:
    static constexpr std::size_t kNumChannels = 2;

    void prepare (double hostSampleRate, int maxBlockSize, const Spec& spec = {})
    {
        spec_     = spec;
        hostRate_ = hostSampleRate;
        maxBlock_ = std::max (maxBlockSize, 1);

        const auto block = static_cast<std::size_t> (maxBlock_);

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            channels_[ch].prepare (spec_);
            channels_[ch].setBandwidth (params_.bandwidth);
            adapters_[ch].prepare (hostRate_, channels_[ch].internalSampleRate());
            dry_[ch].assign (block, 0.0f);
            wet_[ch].assign (block, 0.0f);
        }

        setParams (params_);
    }

    void reset()
    {
        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            channels_[ch].reset();
            adapters_[ch].reset();
        }
    }

    void setParams (const EngineParams& p)
    {
        params_ = p;

        channels_[0].setParams (p.left);
        channels_[1].setParams (p.right);

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            // Clock only. The channel deliberately keeps its memory and pointer
            // positions across this, which is what produces the octave jump.
            channels_[ch].setBandwidth (p.bandwidth);
            adapters_[ch].setInternalRate (channels_[ch].internalSampleRate());
        }
    }

    void process (float* const* io, int numChannels, int numSamples)
    {
        if (numSamples <= 0 || numChannels <= 0)
            return;

        ensureCapacity (numSamples);

        const auto n    = std::min (static_cast<std::size_t> (numChannels), kNumChannels);
        const auto len  = static_cast<std::size_t> (numSamples);
        const float wetG = static_cast<float> (params_.mix);
        const float dryG = 1.0f - wetG;

        for (std::size_t ch = 0; ch < n; ++ch)
        {
            float* data = io[ch];
            std::copy (data, data + len, dry_[ch].begin());

            auto& eng = channels_[ch];
            adapters_[ch].process (data, wet_[ch].data(), numSamples,
                                   [&eng] (float x) noexcept { return eng.processSample (x); });

            for (std::size_t i = 0; i < len; ++i)
                data[i] = dryG * dry_[ch][i] + wetG * wet_[ch][i];
        }
    }

    double latencySamples() const noexcept { return adapters_.front().latencySamples(); }

    DefaultChannelEngine& channel (int i) noexcept
    {
        return channels_[static_cast<std::size_t> (i)];
    }

    const DefaultChannelEngine& channel (int i) const noexcept
    {
        return channels_[static_cast<std::size_t> (i)];
    }

    const EngineParams& params() const noexcept { return params_; }

private:
    /** Only fires if a host breaks its own maxBlockSize contract. */
    void ensureCapacity (int numSamples)
    {
        const auto need = static_cast<std::size_t> (numSamples);
        if (need <= dry_.front().size())
            return;

        for (std::size_t ch = 0; ch < kNumChannels; ++ch)
        {
            dry_[ch].assign (need, 0.0f);
            wet_[ch].assign (need, 0.0f);
        }

        maxBlock_ = numSamples;
    }

    Spec         spec_ {};
    EngineParams params_ {};
    double       hostRate_ = 48000.0;
    int          maxBlock_ = 512;

    std::array<DefaultChannelEngine, kNumChannels> channels_ {};
    std::array<RateAdapter, kNumChannels>          adapters_ {};
    std::array<std::vector<float>, kNumChannels>   dry_ {};
    std::array<std::vector<float>, kNumChannels>   wet_ {};
};

} // namespace the89th
