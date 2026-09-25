#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "BandLimit.hpp"
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
    be bit-exact. */
class Engine
{
public:
    static constexpr std::size_t kNumChannels = 2;

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
    }

    void setParams (const EngineParams& p)
    {
        const bool emphasisWas = emphasisOn();
        params_ = p;

        machine_.setParams (p);
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

        const auto  len  = static_cast<std::size_t> (numSamples);
        const bool  emph = emphasisOn();
        const float wetG = static_cast<float> (params_.mix);
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
                io[ch][i] = dryG * dry_[ch][i] + wetG * y;
            }
        }
    }

    double latencySamples() const noexcept { return adapter_.latencySamples(); }

    const DefaultMachine& machine() const noexcept { return machine_; }
    const EngineParams&   params()  const noexcept { return params_; }
    double                bandEdgeHz() const noexcept { return edgeHz_; }
    const BandLimitFilter& antiAlias (int ch) const noexcept { return antiAlias_[static_cast<std::size_t> (ch)]; }

private:
    bool emphasisOn() const noexcept { return params_.mode == Mode::Delay; }

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
};

} // namespace the89th
