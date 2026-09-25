#pragma once

#include <algorithm>
#include <cmath>

#include "DelayMemory.hpp"
#include "Interpolation.hpp"
#include "Params.hpp"
#include "SpliceTraversal.hpp"
#include "Xing.hpp"

namespace the89th
{

/** One channel's playback side: everything that reads memory, nothing that
    writes it.

    Split from the memory because quasi-stereo has two of these reading one
    memory, and because Xing needs a reader that can look at what it is about
    to splice.

    Two ways to read:
    - Delay mode, not latched: one head at a fixed delay. A change of Delay
      crossfades to the new position, the same two-circuit handover the pitch
      path uses, rather than gliding (which would bend pitch) or jumping (which
      would click).
    - Pitch mode, or latched in either mode: the crosspoint traversal.

    Handover between the two keeps the read position. Latching from delay mode
    places the traversal where the delay head already is; releasing crossfades
    back to the Delay setting. */
template <class Interp = Truncate>
class ReadVoice
{
public:
    void prepare (int memoryWords, int crossfadeSamples)
    {
        words_    = memoryWords;
        fadeLen_  = std::max (1, crossfadeSamples);
        traversal_.setCrossfadeLength (crossfadeSamples);
        reset();
    }

    void reset()
    {
        traversal_.reset();
        lfoPhase_    = 0.0;
        delayFade_   = -1;
        updateTargets();
        delayCur_    = delayTarget_;
        delayFrom_   = delayTarget_;
        onTraversal_ = wantsTraversal();
        applyRegion();
    }

    void setSampleRate (double fs) noexcept { fs_ = fs > 0.0 ? fs : 26455.0; }
    void setMode  (Mode m)       noexcept { mode_  = m; updatePath(); }
    void setRange (DelayRange r) noexcept { range_ = r; updateTargets(); }
    void setXing  (bool on)      noexcept { xingOn_ = on; }

    void setParams (const ChannelParams& p) noexcept
    {
        params_ = p;
        traversal_.setRatio (p.pitchRatio);
        traversal_.setWriteRate (p.freeze ? 0.0 : 1.0);
        updateTargets();
        applyRegion();
        updatePath();
    }

    /** Needs the memory, because Xing inspects it right after a launch. */
    float read (const DelayMemory& mem) noexcept
    {
        if (onTraversal_)
        {
            const auto a = traversal_.primary();
            const auto b = traversal_.secondary();
            float y = a.gain * mem.template read<Interp> (a.delaySamples);
            if (b.gain != 0.0f)
                y += b.gain * mem.template read<Interp> (b.delaySamples);
            return y;
        }

        const double vib = vibratoDelayOffset();
        if (delayFade_ < 0)
            return mem.template read<Interp> (delayCur_ + vib);

        constexpr float kHalfPi = 1.57079632679489662f;
        const float t = static_cast<float> (delayFade_) / static_cast<float> (fadeLen_);
        return std::cos (t * kHalfPi) * mem.template read<Interp> (delayFrom_ + vib)
             + std::sin (t * kHalfPi) * mem.template read<Interp> (delayCur_  + vib);
    }

    void advance (const DelayMemory& mem) noexcept
    {
        lfoPhase_ += 2.0 * M_PI * params_.vibratoRate / fs_;
        if (lfoPhase_ > 2.0 * M_PI)
            lfoPhase_ -= 2.0 * M_PI;

        if (onTraversal_)
        {
            if (params_.vibratoDepth > 0.0)
                traversal_.setRatio (params_.pitchRatio
                                     * std::exp2 (params_.vibratoDepth * std::sin (lfoPhase_) / 12.0));

            traversal_.advance();

            if (traversal_.justLaunched() && xingOn_)
            {
                const auto m = xing_.match (mem, traversal_);
                traversal_.shiftIncoming (m.shift);
                traversal_.setFadeShape (m.correlation);
            }
            else if (traversal_.justLaunched())
            {
                traversal_.setFadeShape (0.0f);
            }
            return;
        }

        if (delayFade_ >= 0 && ++delayFade_ >= fadeLen_)
            delayFade_ = -1;

        if (delayFade_ < 0 && std::fabs (delayTarget_ - delayCur_) >= 0.5)
        {
            delayFrom_ = delayCur_;
            delayCur_  = delayTarget_;
            delayFade_ = 0;
        }
    }

    bool onTraversal() const noexcept { return onTraversal_; }
    double delayTarget() const noexcept { return delayTarget_; }
    double delayCurrent() const noexcept { return delayCur_; }
    const SpliceTraversal& traversal() const noexcept { return traversal_; }
    const Xing& xing() const noexcept { return xing_; }

    /** Longest delay the Delay control reaches in each range, in samples. */
    double maxDelaySamples() const noexcept
    {
        const double full = static_cast<double> (words_ - DelayMemory::kEndGuard);
        const double lo   = static_cast<double> (DelayMemory::kMinDelay);
        return range_ == DelayRange::Short ? lo + (full - lo) / 10.0 : full;
    }

private:
    bool wantsTraversal() const noexcept
    {
        return mode_ == Mode::Pitch || params_.freeze;
    }

    void updatePath() noexcept
    {
        const bool want = wantsTraversal();
        if (want == onTraversal_)
            return;

        if (want)
        {
            traversal_.placeAt (delayCur_);
        }
        else
        {
            delayFrom_ = traversal_.primaryDelay();
            delayCur_  = delayTarget_;
            delayFade_ = 0;
        }
        onTraversal_ = want;
    }

    void updateTargets() noexcept
    {
        const double lo = static_cast<double> (DelayMemory::kMinDelay);
        delayTarget_ = lo + std::clamp (params_.delay, 0.0, 1.0) * (maxDelaySamples() - lo);
    }

    /** Both ends land on exact integers, so crosspoint 2 at 1.0 gives an integer
        delay and transparency can be asserted as bit equality. */
    void applyRegion() noexcept
    {
        const double base = static_cast<double> (DelayMemory::kMinDelay);
        const double span = static_cast<double> (words_ - DelayMemory::kEndGuard) - base;
        traversal_.setRegion (base + params_.crosspoint1 * span,
                              base + params_.crosspoint2 * span);
    }

    /** Vibrato in delay mode moves the read position; the pitch deviation that
        produces is A * omega, so A is sized to give the same peak semitones as
        pitch mode's direct modulation. */
    double vibratoDelayOffset() const noexcept
    {
        if (params_.vibratoDepth <= 0.0)
            return 0.0;

        const double omega = 2.0 * M_PI * params_.vibratoRate / fs_;
        const double amp   = (std::exp2 (params_.vibratoDepth / 12.0) - 1.0) / omega;
        return -amp * std::sin (lfoPhase_);
    }

    SpliceTraversal traversal_;
    Xing            xing_;
    ChannelParams   params_ {};
    Mode            mode_   = Mode::Pitch;
    DelayRange      range_  = DelayRange::Long;
    bool            xingOn_ = true;

    int    words_   = 8192;
    int    fadeLen_ = 96;
    double fs_      = 26455.0;

    bool   onTraversal_ = true;
    double delayTarget_ = 0.0, delayCur_ = 0.0, delayFrom_ = 0.0;
    int    delayFade_   = -1;
    double lfoPhase_    = 0.0;
};

} // namespace the89th
