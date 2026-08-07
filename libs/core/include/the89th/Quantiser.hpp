#pragma once

namespace the89th
{

/** SEAM: the flying-comma converter.

    The hardware stores gain-ranged floating-point words, not linear PCM, so its
    error is constant relative to level instead of constant in absolute terms.
    Working back from the RAM organisation (13 devices of 16384x1 bit ganged into
    16384 words) the word is about 13 bits wide, split between mantissa and
    exponent. That is what "quasi-16-bit" in the research means: 13 bits of
    storage buying 16-bit-class dynamic range through the exponent.

    Phase 0 stores raw floats. store() runs on the way into memory and load() on
    the way out, which is where a real quantiser belongs. */
struct NoQuantiser
{
    static float store (float x) noexcept { return x; }
    static float load  (float x) noexcept { return x; }
};

} // namespace the89th
