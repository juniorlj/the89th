#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Interpolation.hpp"

namespace the89th
{

/** The machine's RAM.

    Sized in words, never in seconds. The word count is a hardware constant, so
    changing the converter clock replays the same addresses at a different rate
    and the pitch jumps. Nothing here knows about sample rate at all. */
class DelayMemory
{
public:
    static constexpr int kMinDelay = 2;  // headroom for the interpolator's newer tap
    static constexpr int kEndGuard = 3;  // headroom for its older taps

    void setSize (int words)
    {
        words_    = std::max (words, 32);
        capacity_ = nextPowerOfTwo (words_);
        mask_     = capacity_ - 1;
        buffer_.assign (static_cast<std::size_t> (capacity_), 0.0f);
        writeIndex_ = 0;
    }

    void clear() noexcept
    {
        std::fill (buffer_.begin(), buffer_.end(), 0.0f);
        writeIndex_ = 0;
    }

    /** Memory latch. Contents and write pointer both stand still, so feedback
        stops accumulating as well, which is what the hardware does. */
    void setWriteHeld (bool held) noexcept { writeHeld_ = held; }
    bool writeHeld() const noexcept        { return writeHeld_; }

    void write (float x) noexcept
    {
        if (writeHeld_)
            return;

        buffer_[static_cast<std::size_t> (writeIndex_)] = x;
        writeIndex_ = (writeIndex_ + 1) & mask_;
    }

    /** delaySamples counts back from the most recently written word, so a delay
        of 0 returns the sample just written. */
    template <class Interp>
    float read (double delaySamples) const noexcept
    {
        const double d    = clampDelay (delaySamples);
        const double fl   = std::floor (d);
        const int    i    = static_cast<int> (fl);
        const float  frac = static_cast<float> (d - fl);

        return Interp::read (buffer_.data(), mask_, writeIndex_ - 1 - i, frac);
    }

    double clampDelay (double d) const noexcept
    {
        const double lo = static_cast<double> (kMinDelay);
        const double hi = static_cast<double> (words_ - kEndGuard);
        return d < lo ? lo : (d > hi ? hi : d);
    }

    int   words()      const noexcept { return words_; }
    int   writeIndex() const noexcept { return writeIndex_; }
    float at (int i)   const noexcept { return buffer_[static_cast<std::size_t> (i & mask_)]; }

private:
    static int nextPowerOfTwo (int v) noexcept
    {
        int p = 1;
        while (p < v)
            p <<= 1;
        return p;
    }

    std::vector<float> buffer_;
    int  words_      = 0;
    int  capacity_   = 0;
    int  mask_       = 0;
    int  writeIndex_ = 0;
    bool writeHeld_  = false;
};

} // namespace the89th
