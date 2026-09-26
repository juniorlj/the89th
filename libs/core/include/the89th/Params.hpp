#pragma once

#include <array>

#include "Spec.hpp"

namespace the89th
{

/** Each channel's Delay / Pitch-Shifter buttons. Every photographed panel has
    a set per side, so the two channels choose their mode independently; the
    rear connector's "pitch mode" input sets both at once.

    Delay: the channel reads at a fixed delay set by its Delay control, with
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

/** The keyboard's Left / Right / Biphonic switch: which channels it plays.
    Biphonic is two voices, one per channel. */
enum class KeyChannels
{
    Off,
    Left,
    Right,
    Biphonic
};

/** Push/Play sounds only while a key is down. Sustain starts the note on the
    key and lets the envelope decide how long it lasts. */
enum class KeyPlay
{
    PushPlay,
    Sustain
};

/** The per-side ON switches of Memory Synchro and Reverse Synchro. */
enum class Sides
{
    Off,
    Left,
    Right,
    Both
};

constexpr bool onSide (Sides s, int channel) noexcept
{
    return s == Sides::Both || (channel == 0 ? s == Sides::Left : s == Sides::Right);
}

/** The KB 2000, the keyboard controller sold with the machine. Its panel and
    brochure give the sections and what each does; they give no scales, so
    every time and range here is a choice (docs/clone-status.md lists them).
    Everything is off by default, and off is bypassed outright. */
struct KeyboardParams
{
    // Pitch ratio settings. A key replaces the Pitch pots of the channels it
    // plays, as the machine's external pitch clock did.
    KeyChannels channels = KeyChannels::Off;
    KeyPlay     play     = KeyPlay::PushPlay;
    int    root       = 60;    // the key that plays at unity
    double trimCents  = 0.0;   // Trimmer: tunes the whole keyboard
    double glideSeconds = 0.0; // Slope: glissando time from one note to the next
    double addedDelay = 0.0;   // 0..1: pushes the region deeper, a serial delay

    // Envelope: two generators, one VCA per channel. Off is a plain gate.
    bool   envelope = false;
    double attackSeconds  = 0.01;
    double holdSeconds    = 0.5;   // Sustain mode: how long a note stays up
    double releaseSeconds = 0.3;

    // Vibrato: three parameters, each moved from its base by a modulator
    // that each note's attack starts.
    bool   vibrato = false;
    double vibRateHz   = 5.0;
    double vibSharpness = 0.0;     // 0 sine .. 1 near square
    double vibDepth    = 0.5;      // semitones
    double vibModRate  = 0.0;      // -1..1: the modulator's pull on each
    double vibModSharpness = 0.0;
    double vibModDepth = 0.0;
    double vibAttackSeconds  = 0.5;
    double vibReleaseSeconds = 0.5;

    // Memory Synchro: each note starts reading the latched memory at the
    // attack point, runs to the end point, then loops from the return point
    // while the note lasts. Positions run 0 (oldest) to 1 (newest) through
    // the memory. Speed is the reading speed: 1 is as recorded, 0 is Free,
    // where reading follows the pitch as tape would.
    Sides  memorySynchro = Sides::Off;
    double attackPoint = 0.0;
    double returnPoint = 0.5;
    double endPoint    = 1.0;
    double speed       = 1.0;

    // Reverse Synchro: on live input only, each attack in the sound restarts
    // the traversal, after the added delay, so reversed segments keep the
    // original's tempo. The noise gate mutes the side below the threshold.
    Sides  reverseSynchro = Sides::Off;
    bool   noiseGate = false;
    double thresholdDb = -30.0;
    double reverseDelaySeconds = 0.0;

    bool any() const noexcept
    {
        return channels != KeyChannels::Off || reverseSynchro != Sides::Off;
    }
};

struct ChannelParams
{
    Mode mode = Mode::Pitch;

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

    /** Memory latch: stop writing, keep looping the crosspoint region. One
        button per side on the panel; quasi-stereo has one write, so there
        either side's latch holds both. */
    bool freeze = false;
};

struct EngineParams
{
    ChannelParams left;
    ChannelParams right;

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

    KeyboardParams keys;

    /** Moves both crosspoints together, as a share of the region's length. */
    double    scrubDepth = 0.0;  // 0..1
    double    scrubRate  = 0.5;  // Hz
    ScrubMode scrubMode  = ScrubMode::Lfo;

    static constexpr double kLowCutOffHz  = 20.0;
    static constexpr double kHighCutOffHz = 20000.0;
};

} // namespace the89th
