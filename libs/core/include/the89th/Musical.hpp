#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace the89th::musical
{

/** The hardware's pitch span: two octaves down to one up. */
constexpr double kMinRatio = 0.25;
constexpr double kMaxRatio = 2.0;

enum class Scale
{
    Off,
    Chromatic,
    Major,
    Minor,
    Pentatonic
};

/** Steps of each scale within an octave, counted from unison. */
inline const std::array<int, 12>& scaleMask (Scale s) noexcept
{
    static const std::array<int, 12> chromatic  { 1,1,1,1,1,1,1,1,1,1,1,1 };
    static const std::array<int, 12> major      { 1,0,1,0,1,1,0,1,0,1,0,1 };
    static const std::array<int, 12> minor      { 1,0,1,1,0,1,0,1,1,0,1,0 };
    static const std::array<int, 12> pentatonic { 1,0,1,0,1,0,0,1,0,1,0,0 };
    switch (s)
    {
        case Scale::Major:      return major;
        case Scale::Minor:      return minor;
        case Scale::Pentatonic: return pentatonic;
        case Scale::Off:
        case Scale::Chromatic:  break;
    }
    return chromatic;
}

/** Nearest step of the scale to a shift in semitones. Ties go to the lower
    step, so a knob resting between two notes doesn't flicker. */
inline double snapSemitones (double semis, Scale s) noexcept
{
    if (s == Scale::Off)
        return semis;

    const auto& mask = scaleMask (s);
    double best = semis, bestDist = 1e9;
    const int lo = static_cast<int> (std::floor (semis)) - 12;
    for (int n = lo; n <= lo + 24; ++n)
    {
        const int degree = ((n % 12) + 12) % 12;
        if (! mask[static_cast<std::size_t> (degree)])
            continue;
        const double d = std::abs (semis - n);
        if (d < bestDist - 1e-9)
        {
            bestDist = d;
            best     = n;
        }
    }
    return best;
}

/** Pitch knob ratio, snapped, plus fine tuning, held inside the hardware's span. */
inline double pitchRatio (double knobRatio, Scale s, double fineCents) noexcept
{
    const double semis = snapSemitones (12.0 * std::log2 (std::max (knobRatio, 1e-6)), s);
    const double ratio = s == Scale::Off ? knobRatio : std::exp2 (semis / 12.0);
    // exp2 (0) is exactly 1, so zero cents leaves the ratio untouched.
    return std::clamp (ratio * std::exp2 (fineCents / 1200.0), kMinRatio, kMaxRatio);
}

// ─── Tempo sync ─────────────────────────────────────────────────────────────

struct Note
{
    double      quarters;  // length in quarter notes
    const char* name;
};

/** Shortest to longest. A synced knob's bottom position is not a note: it
    stays the shortest delay the machine has. */
inline constexpr std::array<Note, 17> kNotes { {
    { 1.0 / 16.0, "1/64"  },
    { 1.0 / 12.0, "1/32T" },
    { 1.0 / 8.0,  "1/32"  },
    { 1.0 / 6.0,  "1/16T" },
    { 3.0 / 16.0, "1/32D" },
    { 1.0 / 4.0,  "1/16"  },
    { 1.0 / 3.0,  "1/8T"  },
    { 3.0 / 8.0,  "1/16D" },
    { 1.0 / 2.0,  "1/8"   },
    { 2.0 / 3.0,  "1/4T"  },
    { 3.0 / 4.0,  "1/8D"  },
    { 1.0,        "1/4"   },
    { 4.0 / 3.0,  "1/2T"  },
    { 3.0 / 2.0,  "1/4D"  },
    { 2.0,        "1/2"   },
    { 3.0,        "1/2D"  },
    { 4.0,        "1/1"   },
} };

constexpr int kSyncSteps = static_cast<int> (kNotes.size()) + 1;  // plus the bottom

/** Knob position 0..1 to a step: 0 is the bottom, 1.. are kNotes. */
inline int syncStep (double knob) noexcept
{
    return static_cast<int> (std::lround (std::clamp (knob, 0.0, 1.0) * (kSyncSteps - 1)));
}

/** Seconds for a step at a tempo; 0 for the bottom position. */
inline double syncSeconds (int step, double bpm) noexcept
{
    if (step <= 0)
        return 0.0;
    const auto& n = kNotes[static_cast<std::size_t> (std::min (step, kSyncSteps - 1) - 1)];
    return n.quarters * 60.0 / std::max (bpm, 1.0);
}

inline const char* syncName (int step) noexcept
{
    return step <= 0 ? "MIN" : kNotes[static_cast<std::size_t> (std::min (step, kSyncSteps - 1) - 1)].name;
}

} // namespace the89th::musical
