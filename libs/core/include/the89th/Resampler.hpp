#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace the89th
{

/** Host rate to internal rate and back.

    The engine has to run at the converter clock. Emulating a bandwidth switch
    at host rate by rescaling the delay time would reproduce the delay ranges and
    lose the octave jumps entirely, because the jump comes from replaying stored
    addresses at a different clock.

    Both directions are driven off one shared time base measured in host samples,
    so the two accumulators cannot drift apart: the engine is only ever run far
    enough ahead to satisfy the next output sample.

    SEAM: no band-limiting. The anti-alias filter that belongs in front of the
    downsampler and the reconstruction filter that belongs after the upsampler
    are Phase 2, and until then the low clock settings alias rather than ring. */
class RateAdapter
{
public:
    static constexpr int       kRingSize = 8192;
    static constexpr long long kMask     = kRingSize - 1;

    void prepare (double hostRate, double internalRateHz)
    {
        hostRate_ = hostRate > 0.0 ? hostRate : 48000.0;
        inRing_.assign (static_cast<std::size_t> (kRingSize), 0.0f);
        outRing_.assign (static_cast<std::size_t> (kRingSize), 0.0f);
        setInternalRate (internalRateHz);
        reset();
    }

    /** Safe to call mid-stream. A bandwidth switch only changes the clock, and
        the shared time base keeps both sides coherent across the change. */
    void setInternalRate (double r) noexcept
    {
        internalRate_ = r > 0.0 ? r : hostRate_;
        ratio_        = internalRate_ / hostRate_;   // internal samples per host sample
        invRatio_     = hostRate_ / internalRate_;   // host samples per internal sample
    }

    void reset() noexcept
    {
        std::fill (inRing_.begin(), inRing_.end(), 0.0f);
        std::fill (outRing_.begin(), outRing_.end(), 0.0f);
        inWritten_          = 0;
        engineIndex_        = 0;
        nextEngineTimeHost_ = 0.0;
        outPosInternal_     = 0.0;
    }

    /** engineStep runs once per internal sample, in order. */
    template <class Fn>
    void process (const float* in, float* out, int n, Fn&& engineStep)
    {
        for (int i = 0; i < n; ++i)
        {
            inRing_[static_cast<std::size_t> (inWritten_ & kMask)] = in[i];
            ++inWritten_;

            // Two samples back so the interpolator's upper tap is always written.
            const double horizon = static_cast<double> (inWritten_ - 2);
            const double needed  = outPosInternal_ + 2.0;

            while (static_cast<double> (engineIndex_) < needed
                   && nextEngineTimeHost_ <= horizon)
            {
                const float xi = lerp (inRing_, nextEngineTimeHost_);
                outRing_[static_cast<std::size_t> (engineIndex_ & kMask)] = engineStep (xi);
                ++engineIndex_;
                nextEngineTimeHost_ += invRatio_;
            }

            out[i] = lerp (outRing_, outPosInternal_);
            outPosInternal_ += ratio_;
        }
    }

    /** Conversion latency only. The delay the engine itself imposes is the
        effect, not latency, so it is deliberately not reported here. */
    double latencySamples() const noexcept { return 2.0 + 2.0 * invRatio_; }

    double internalSampleRate() const noexcept { return internalRate_; }

private:
    static float lerp (const std::vector<float>& r, double pos) noexcept
    {
        const double    fl   = std::floor (pos);
        const long long idx  = static_cast<long long> (fl);
        const float     frac = static_cast<float> (pos - fl);

        const float a = r[static_cast<std::size_t> (idx & kMask)];
        const float b = r[static_cast<std::size_t> ((idx + 1) & kMask)];
        return a + frac * (b - a);
    }

    std::vector<float> inRing_, outRing_;

    double hostRate_     = 48000.0;
    double internalRate_ = 26455.0;
    double ratio_        = 1.0;
    double invRatio_     = 1.0;

    long long inWritten_   = 0;
    long long engineIndex_ = 0;
    double nextEngineTimeHost_ = 0.0;
    double outPosInternal_     = 0.0;
};

} // namespace the89th
