#pragma once

#include <algorithm>
#include <cmath>
#include <functional>

#include "Params.hpp"

namespace the89th
{

/** Low cut, high cut and drive inside the feedback loop.

    Sits between a side's output and the memory it feeds, so whatever it does
    compounds on every repeat: a high cut darkens each pass a little more, drive
    thickens and then eats the tail.

    A modern addition, not part of the hardware. At its neutral settings each
    stage is skipped rather than set to something small, so the loop is
    bit-identical to the machine without it.

    Runs at the machine's internal clock, which is as low as 13.2 kHz, so the
    high cut is held below 0.45 of that clock. The converter clip that follows
    stays where it was, on the write into memory. */
class FeedbackTone
{
public:
    /** Two poles each: a gentle slope, so a repeat changes by a little and
        the change builds up over passes rather than all at once. */
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        float process (float x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return static_cast<float> (y);
        }

        void reset() noexcept { z1 = z2 = 0.0; }
    };

    /** Drive of 1 pushes the signal 20 dB into the curve. */
    static constexpr double kMaxDriveDb = 20.0;

    void setSampleRate (double fs) noexcept
    {
        if (same (fs, fs_))
            return;
        fs_ = fs > 0.0 ? fs : 26455.0;
        lowHz_ = highHz_ = -1.0;  // force a redesign at the new clock
    }

    void setParams (double lowCutHz, double highCutHz, double drive) noexcept
    {
        lowOn_ = lowCutHz > EngineParams::kLowCutOffHz;
        if (lowOn_ && ! same (lowCutHz, lowHz_))
            design (low_, lowCutHz, false);
        lowHz_ = lowCutHz;

        const double ceiling = 0.45 * fs_;
        highOn_ = highCutHz < EngineParams::kHighCutOffHz;
        if (highOn_ && ! same (highCutHz, highHz_))
            design (high_, std::min (highCutHz, ceiling), true);
        highHz_ = highCutHz;

        drive_ = std::clamp (drive, 0.0, 1.0);
        preGain_ = static_cast<float> (std::pow (10.0, drive_ * kMaxDriveDb / 20.0));
    }

    void reset() noexcept
    {
        low_.reset();
        high_.reset();
    }

    bool active() const noexcept { return lowOn_ || highOn_ || drive_ > 0.0; }

    float process (float x) noexcept
    {
        if (lowOn_)
            x = low_.process (x);
        if (highOn_)
            x = high_.process (x);
        if (drive_ > 0.0)
        {
            // Unity gain for small signals, so drive changes the colour of the
            // repeats rather than how many there are; loud ones are rounded
            // off and lose level, which also keeps a driven loop stable.
            x = std::tanh (preGain_ * x) / preGain_;
        }
        return x;
    }

private:
    /** Exact on purpose: redesign only when a setting really changed. */
    static bool same (double a, double b) noexcept { return std::equal_to<double> {} (a, b); }

    /** RBJ cookbook, Butterworth Q. */
    void design (Biquad& f, double hz, bool lowpass) noexcept
    {
        const double w0    = 2.0 * M_PI * hz / fs_;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * M_SQRT1_2);
        const double a0    = 1.0 + alpha;

        if (lowpass)
        {
            f.b0 = (1.0 - cosw) * 0.5 / a0;
            f.b1 = (1.0 - cosw) / a0;
        }
        else
        {
            f.b0 = (1.0 + cosw) * 0.5 / a0;
            f.b1 = -(1.0 + cosw) / a0;
        }
        f.b2 = f.b0;
        f.a1 = -2.0 * cosw / a0;
        f.a2 = (1.0 - alpha) / a0;
    }

    Biquad low_, high_;
    double fs_     = 26455.0;
    double lowHz_  = -1.0, highHz_ = -1.0;
    double drive_  = 0.0;
    float  preGain_ = 1.0f;
    bool   lowOn_  = false, highOn_ = false;
};

} // namespace the89th
