#pragma once

namespace the89th
{

/** Fractional readers over a power-of-two circular buffer.

    i1 is the (unmasked) index of the sample at floor(delay); frac walks from it
    toward older samples, so index decreases as frac rises.

    Both kernels are interpolating: at frac == 0 they return the stored sample
    bit-for-bit. That property is what makes the ratio-1.0 delay path
    transparent rather than merely close. */

/** What the hardware does: no interpolation at all.

    The read address comes from a TTL counter clocked at the pitch rate, and the
    converter takes whatever word that counter points at on each of its own fixed
    ticks. There is no multiplier anywhere in the design, so a fractional
    position is simply dropped. Moving heads therefore repeat or skip words,
    which adds the stepped, aliased grain the low clock settings are known for.
    A static delay reads exact words, so the delay path stays bit-exact. */
struct Truncate
{
    static float read (const float* buf, int mask, int i1, float) noexcept
    {
        return buf[i1 & mask];
    }
};

struct Linear
{
    static float read (const float* buf, int mask, int i1, float frac) noexcept
    {
        const float a = buf[i1 & mask];
        const float b = buf[(i1 - 1) & mask];
        return a + frac * (b - a);
    }
};

struct CatmullRom
{
    static float read (const float* buf, int mask, int i1, float frac) noexcept
    {
        const float p0 = buf[(i1 + 1) & mask];  // delay - 1, newer
        const float p1 = buf[ i1      & mask];  // delay
        const float p2 = buf[(i1 - 1) & mask];  // delay + 1
        const float p3 = buf[(i1 - 2) & mask];  // delay + 2

        const float c0 = p1;
        const float c1 = 0.5f * (p2 - p0);
        const float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};

} // namespace the89th
