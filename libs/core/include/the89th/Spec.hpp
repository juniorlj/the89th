#pragma once

namespace the89th
{

/** Converter bandwidth. The front-panel switch divides the converter clock, so
    this selects the internal sample rate rather than a filter cutoff. */
enum class Bandwidth
{
    k5kHz  = 0,
    k10kHz = 1,
    k20kHz = 2
};

constexpr int rateDivisor (Bandwidth bw) noexcept
{
    switch (bw)
    {
        case Bandwidth::k5kHz:  return 4;
        case Bandwidth::k10kHz: return 2;
        case Bandwidth::k20kHz: return 1;
    }
    return 1;
}

/** Fixed machine constants.

    memoryWordsTotal is a word count, not a duration. 13 DRAMs organised
    16384x1 bit gang into 16384 words of 13 bits, which reproduces the
    documented 300/600/1200 ms maxima to within the manual's rounding. Because
    it never changes with sample rate, a bandwidth switch replays the same
    addresses at a different clock, which is the octave jump. */
struct Spec
{
    double converterClockHz = 52910.0;  // 18.9 us conversion time
    int    memoryWordsTotal = 16384;    // 2^14 words for the whole machine
    int    channels         = 2;        // true stereo splits memory and converter
    int    crossfadeSamples = 96;       // fixed splice fade length

    constexpr int memoryWordsPerChannel() const noexcept
    {
        return memoryWordsTotal / channels;
    }

    /** One converter, time-shared across channels. In true stereo each channel
        gets half the clock, and that is what puts 20 kHz out of reach. */
    constexpr Bandwidth maxBandwidth() const noexcept
    {
        return channels > 1 ? Bandwidth::k10kHz : Bandwidth::k20kHz;
    }

    constexpr Bandwidth effectiveBandwidth (Bandwidth requested) const noexcept
    {
        return requested > maxBandwidth() ? maxBandwidth() : requested;
    }
};

constexpr double internalRate (const Spec& s, Bandwidth bw) noexcept
{
    return s.converterClockHz / rateDivisor (s.effectiveBandwidth (bw));
}

constexpr double maxDelaySeconds (const Spec& s, Bandwidth bw) noexcept
{
    return s.memoryWordsPerChannel() / internalRate (s, bw);
}

} // namespace the89th
