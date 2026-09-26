#pragma once

#include <cmath>
#include <cstdint>

#include "Params.hpp"

namespace the89th
{

/** The scrub's motion: where the crosspoint region sits, from -1 to +1.

    LFO swings smoothly back and forth. Random picks a new spot each cycle and
    eases to it along a half cosine, so it wanders without jumping. The random
    sequence comes from a fixed seed, so a render is repeatable. */
class ScrubLfo
{
public:
    void reset() noexcept
    {
        phase_ = 0.0;
        rng_   = 0x2545F491u;
        from_  = 0.0;
        to_    = next();
    }

    /** Moves n samples on at rate Hz and returns the position. */
    double advance (int n, double rateHz, double sampleRate, ScrubMode mode) noexcept
    {
        phase_ += static_cast<double> (n) * rateHz / sampleRate;
        while (phase_ >= 1.0)
        {
            phase_ -= 1.0;
            from_ = to_;
            to_   = next();
        }

        if (mode == ScrubMode::Random)
        {
            const double ease = 0.5 - 0.5 * std::cos (M_PI * phase_);
            return from_ + (to_ - from_) * ease;
        }
        return std::sin (2.0 * M_PI * phase_);
    }

private:
    /** xorshift32: plenty for a wander, and the same on every platform. */
    double next() noexcept
    {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return static_cast<double> (rng_) / 2147483647.5 - 1.0;
    }

    double        phase_ = 0.0;
    std::uint32_t rng_   = 0x2545F491u;
    double        from_  = 0.0, to_ = 0.0;
};

} // namespace the89th
