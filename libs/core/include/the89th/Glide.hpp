#pragma once

#include <algorithm>

namespace the89th
{

/** A straight-line move to a target over a fixed number of samples.

    Lands on the target exactly rather than approaching it, so once a control
    stops moving the engine sees the same number it would have without the
    glide, and bit-exact tests stay bit-exact. */
class Glide
{
public:
    void setLength (int samples) noexcept { length_ = std::max (1, samples); }

    /** Jump straight to v, for a fresh start where there is nothing to glide from. */
    void snap (double v) noexcept
    {
        current_ = target_ = v;
        remaining_ = 0;
    }

    void setTarget (double v) noexcept
    {
        if (v == target_)
            return;

        target_    = v;
        remaining_ = length_;
        increment_ = (target_ - current_) / static_cast<double> (length_);
    }

    /** Moves n samples along and returns where that lands. */
    double advance (int n) noexcept
    {
        if (remaining_ <= 0)
            return current_;

        if (n >= remaining_)
        {
            current_   = target_;
            remaining_ = 0;
        }
        else
        {
            current_   += increment_ * n;
            remaining_ -= n;
        }
        return current_;
    }

    double current() const noexcept { return current_; }
    double target()  const noexcept { return target_; }
    bool   moving()  const noexcept { return remaining_ > 0; }

private:
    double current_   = 0.0;
    double target_    = 0.0;
    double increment_ = 0.0;
    int    length_    = 1;
    int    remaining_ = 0;
};

} // namespace the89th
