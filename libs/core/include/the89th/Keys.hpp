#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Musical.hpp"

namespace the89th::keys
{

/** Building blocks for the keyboard (Keyboard.hpp): which keys are held, and
    what ratio a key asks for. Nothing here touches audio. */

/** Held keys, last-note priority. Lifting the newest key falls back to the one
    held before it, as monophonic keyboards do. */
class NoteStack
{
public:
    static constexpr int kCapacity = 16;

    void press (int note) noexcept
    {
        release (note);  // a repeated key moves to the top rather than doubling
        if (count_ == kCapacity)
        {
            std::copy (notes_.begin() + 1, notes_.end(), notes_.begin());
            --count_;
        }
        notes_[static_cast<std::size_t> (count_++)] = note;
    }

    void release (int note) noexcept
    {
        auto* end = notes_.begin() + count_;
        auto* it  = std::find (notes_.begin(), end, note);
        if (it == end)
            return;
        std::copy (it + 1, end, it);
        --count_;
    }

    void clear() noexcept { count_ = 0; }

    bool active() const noexcept { return count_ > 0; }

    /** The key that sounds: the most recent one still held. */
    int current() const noexcept { return count_ > 0 ? notes_[static_cast<std::size_t> (count_ - 1)] : -1; }

    int held() const noexcept { return count_; }

    /** The k-th most recent key still held: 0 is the newest. */
    int recent (int k) const noexcept
    {
        return k >= 0 && k < count_ ? notes_[static_cast<std::size_t> (count_ - 1 - k)] : -1;
    }

private:
    std::array<int, kCapacity> notes_ {};
    int count_ = 0;
};

/** Pitch ratio for a key: the root plays at unity, a semitone per key, bend
    added on top, held inside the hardware's 0.25x to 2x. */
inline double ratio (int note, int root, double bendSemitones = 0.0) noexcept
{
    const double semis = static_cast<double> (note - root) + bendSemitones;
    return std::clamp (std::exp2 (semis / 12.0), musical::kMinRatio, musical::kMaxRatio);
}

/** Pitch-wheel value (0..16383, centre 8192) to semitones at the given range. */
inline double bendSemitones (int wheel, double range = 2.0) noexcept
{
    return range * static_cast<double> (wheel - 8192) / 8192.0;
}

} // namespace the89th::keys
