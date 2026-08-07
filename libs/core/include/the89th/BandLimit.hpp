#pragma once

namespace the89th
{

/** SEAM: anti-alias and reconstruction filtering.

    A converter running at 52.91 kHz with 5/10/20 kHz band options needs a
    filter on the way in and another on the way out. Their topologies are not
    published; the audible targets are the +0/-3 dB band edges and the
    high-frequency ringing the low clock settings produce.

    Phase 0 passes signal through untouched, so the low-rate settings alias
    rather than ring. Both are Phase 2, and they sit either side of RateAdapter. */
struct NoBandLimit
{
    void prepare (double /*sampleRate*/, double /*cutoffHz*/) noexcept {}
    void reset() noexcept {}
    float process (float x) noexcept { return x; }
};

} // namespace the89th
