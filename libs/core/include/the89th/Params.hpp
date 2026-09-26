#pragma once

#include <array>

#include "Spec.hpp"

namespace the89th
{

/** The hardware's mode switch (also on the rear connector as "pitch mode",
    for both channels at once).

    Delay: each channel reads at a fixed delay set by its Delay control, with
    pre/de-emphasis around the converter.
    Pitch: the read heads traverse the crosspoint region at the pitch ratio,
    emphasis bypassed. */
enum class Mode
{
    Delay,
    Pitch
};

/** True stereo splits memory and converter between two independent channels.
    Quasi-stereo feeds one input to both, so a single write fills the whole
    16384-word memory at the full clock: longer delays, and 20 kHz becomes
    reachable. */
enum class StereoMode
{
    True,
    Quasi
};

/** Later units' short/long switch: short divides the delay scale by ten, for
    doubling and flanging. */
enum class DelayRange
{
    Long,
    Short
};

/** Where each side's repeats go in true stereo. Quasi-stereo has one write,
    so its two outputs always merge, whatever this says. */
enum class FeedbackRoute
{
    Normal,  // each side feeds itself, as on the hardware
    Cross,   // each side feeds the other: ping-pong
    Sum      // both feed both
};

/** Vibrato waveform. Square jumps up by the depth and back, a trill. */
enum class VibratoShape
{
    Sine,
    Square
};

/** How the scrub moves the crosspoint region. */
enum class ScrubMode
{
    Lfo,     // smooth back and forth
    Random   // glides to a new random spot each cycle
};

struct ChannelParams
{
    /** Delay mode only. 0..1 over the delay scale, which the range switch sets. */
    double delay = 1.0;

    /** Pitch mode, and the latch. Magnitude only, 0.25 to 2.0: direction comes
        from crosspoint order, as the hardware's pitch clock has no sign. */
    double pitchRatio = 1.0;

    /** Normalised 0..1 over the channel's memory. The read head starts at
        crosspoint 2 and splices back at crosspoint 1; crosspoint 1 deeper than
        crosspoint 2 plays in reverse. */
    double crosspoint1 = 0.0;
    double crosspoint2 = 1.0;

    /** The pitch shifter sits inside this loop, so repeats arpeggiate. */
    double feedback = 0.0;

    /** Later units' per-channel vibrato. Depth is peak pitch deviation in
        semitones, the same in both modes. */
    double vibratoDepth = 0.0;
    double vibratoRate  = 5.0;
    VibratoShape vibratoShape = VibratoShape::Sine;

    /** Memory latch: stop writing, keep looping the crosspoint region. */
    bool freeze = false;
};

struct EngineParams
{
    ChannelParams left;
    ChannelParams right;

    Mode       mode   = Mode::Pitch;
    StereoMode stereo = StereoMode::True;
    DelayRange range  = DelayRange::Long;

    /** Global: it is the converter clock, not a per-channel filter. */
    Bandwidth bandwidth = Bandwidth::k10kHz;

    double mix = 1.0;  // 0 dry, 1 wet

    // Modern controls. Every default is neutral, and neutral is bypassed
    // outright, so a default EngineParams is the hardware and nothing else.

    FeedbackRoute route = FeedbackRoute::Normal;

    /** Tone inside the feedback loop, so it compounds on every repeat. */
    double lowCutHz  = kLowCutOffHz;
    double highCutHz = kHighCutOffHz;
    double drive     = 0.0;  // 0..1

    /** Keyboard layer, per channel. The gate scales that channel's wet output:
        1 is open, 0 is the hardware's muted note-off. Pitch glide is how long a
        pitch change takes to land; a keyboard wants 0, as the hardware's pitch
        clock jumped. Defaults are neutral: open, and the engine's usual glide. */
    std::array<double, 2> gate              { 1.0, 1.0 };
    std::array<double, 2> pitchGlideSeconds { kDefaultPitchGlide, kDefaultPitchGlide };

    /** Moves both crosspoints together, as a share of the region's length. */
    double    scrubDepth = 0.0;  // 0..1
    double    scrubRate  = 0.5;  // Hz
    ScrubMode scrubMode  = ScrubMode::Lfo;

    static constexpr double kDefaultPitchGlide = 0.03;
    static constexpr double kLowCutOffHz  = 20.0;
    static constexpr double kHighCutOffHz = 20000.0;
};

} // namespace the89th
