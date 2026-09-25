#pragma once

#include <cmath>

namespace the89th
{

/** Raw floats, no converter. Kept for tests that isolate the delay path. */
struct NoQuantiser
{
    static float store (float x) noexcept { return x; }
    static float load  (float x) noexcept { return x; }
};

/** The flying-comma converter: a 13-bit gain-ranging word.

    The word width comes from the RAM: 13 DRAMs organised 16384x1 bit give
    13-bit words. The split comes from the published figures. Of the ways to
    divide 13 bits into sign, mantissa and exponent with 6 dB per exponent
    step, only sign + 9-bit mantissa + 3-bit exponent reaches the stated
    ~95 dB of dynamic range (96.3 dB) and a 16-bit linear equivalent, which is
    what "quasi-16-bit" describes. Splitting 1+10+2 gives 78 dB; 1+8+4 gives
    138 dB.

    So each sample is stored with about 9 bits of precision at whatever level
    it sits: roughly 56 dB of signal to noise, the same at -3 dBFS as at
    -60 dBFS. That constant relative error is the converter's character.

    The exponent picks one of eight ranges, each 6 dB below the next. Range r
    covers magnitudes below 2^(r-7) with a step of 2^(r-16), so the finest step
    is 2^-16 and full scale is 1.0. Anything at or above full scale clips to the
    largest code, which is the hardware folding at its own converter.

    Conversion happens once, on the way into memory. Reading back is exact,
    because the D/A outputs precisely the stored code. */
struct FlyingComma
{
    static constexpr int   kMantissaBits = 9;
    static constexpr int   kRanges       = 8;
    static constexpr float kFullScale    = 1.0f;
    static constexpr float kMaxCode      = 1.0f - 1.0f / 512.0f;  // top range, all mantissa bits set

    static float store (float x) noexcept
    {
        const float a = std::fabs (x);

        if (! (a < kMaxCode))  // also catches NaN, which the converter would rail on
            return std::copysign (kMaxCode, x);

        // a = m * 2^e with m in [0.5, 1). Range r is the smallest with a < 2^(r-7).
        int e = 0;
        std::frexp (a, &e);
        int r = e + 7;
        if (r < 0) r = 0;
        if (r > kRanges - 1) r = kRanges - 1;

        const float step = std::ldexp (1.0f, r - 16);
        float q = std::nearbyint (a / step) * step;
        if (q > kMaxCode)
            q = kMaxCode;

        return std::copysign (q, x);
    }

    static float load (float x) noexcept { return x; }

    /** Exponent a value would be stored with. Exposed for tests. */
    static int rangeOf (float x) noexcept
    {
        int e = 0;
        std::frexp (std::fabs (x), &e);
        const int r = e + 7;
        return r < 0 ? 0 : (r > kRanges - 1 ? kRanges - 1 : r);
    }
};

} // namespace the89th
