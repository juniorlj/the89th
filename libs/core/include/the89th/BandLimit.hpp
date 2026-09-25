#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace the89th
{

/** Anti-alias and reconstruction filters around the converter.

    The spec gives the response only as 20 Hz to 5 / 10 / 20 kHz, +0/-3 dB. The
    topology is not published. This models it as an 8th-order Chebyshev type I
    lowpass with 0.5 dB ripple on each side of the converter, which is the kind
    of steep, ringing filter a 1978 converter needed, and which rings at the band
    edge the way the low clock settings are described as doing.

    Two filters sit in series, one before the A/D and one after the D/A, so each
    is designed to be 1.5 dB down at the band edge and the pair lands on the
    published -3 dB. Runs at host rate, standing in for the analog stages.

    Order, ripple and the per-filter edge attenuation are assumptions. Change
    kOrder, kRippleDb and kEdgeDb here and every derived coefficient follows. */
class BandLimitFilter
{
public:
    static constexpr int    kOrder    = 8;
    static constexpr int    kSections = kOrder / 2;
    static constexpr double kRippleDb = 0.5;
    static constexpr double kEdgeDb   = 1.5;   // per filter; two in series make -3 dB

    void design (double sampleRate, double edgeHz)
    {
        const double fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        const double f3 = std::min (edgeHz, 0.45 * fs);

        const double eps = std::sqrt (std::pow (10.0, kRippleDb / 10.0) - 1.0);

        // Where the ripple band must end so that |H| is kEdgeDb down at f3.
        const double cAtEdge = std::sqrt (std::pow (10.0, kEdgeDb / 10.0) - 1.0) / eps;
        const double xEdge   = cAtEdge >= 1.0 ? std::cosh (std::acosh (cAtEdge) / kOrder) : 1.0;

        // Prewarp so the digital filter hits the band edge exactly.
        const double k   = 2.0 * fs;
        const double wa3 = k * std::tan (M_PI * f3 / fs);
        const double wp  = wa3 / xEdge;

        const double mu = std::asinh (1.0 / eps) / kOrder;

        double gain = 1.0 / std::sqrt (1.0 + eps * eps);  // even order: DC sits in a ripple trough

        for (int s = 0; s < kSections; ++s)
        {
            const double theta = M_PI * (2.0 * s + 1.0) / (2.0 * kOrder);
            const double sigma = -std::sinh (mu) * std::sin (theta) * wp;
            const double omega =  std::cosh (mu) * std::cos (theta) * wp;

            // Analog section: w0^2 / (s^2 - 2 sigma s + w0^2), then bilinear.
            const double w02 = sigma * sigma + omega * omega;
            const double a1s = -2.0 * sigma;

            const double a0 = k * k + a1s * k + w02;
            auto& c = coeffs_[static_cast<std::size_t> (s)];
            c.b0 = w02 / a0;
            c.b1 = 2.0 * w02 / a0;
            c.b2 = w02 / a0;
            c.a1 = 2.0 * (w02 - k * k) / a0;
            c.a2 = (k * k - a1s * k + w02) / a0;
        }

        gain_ = gain;
    }

    void reset() noexcept
    {
        for (auto& st : state_)
            st = {};
    }

    float process (float x) noexcept
    {
        double v = static_cast<double> (x) * gain_;
        for (std::size_t s = 0; s < static_cast<std::size_t> (kSections); ++s)
        {
            const auto& c  = coeffs_[s];
            auto&       st = state_[s];
            const double y = c.b0 * v + st.z1;
            st.z1 = c.b1 * v - c.a1 * y + st.z2;
            st.z2 = c.b2 * v - c.a2 * y;
            v = y;
        }
        return static_cast<float> (v);
    }

    /** Magnitude response in dB at f, from the coefficients. For tests. */
    double responseDb (double f, double sampleRate) const
    {
        const std::complex<double> z = std::polar (1.0, 2.0 * M_PI * f / sampleRate);
        const std::complex<double> zi = 1.0 / z;
        std::complex<double> h = gain_;
        for (const auto& c : coeffs_)
            h *= (c.b0 + c.b1 * zi + c.b2 * zi * zi) / (1.0 + c.a1 * zi + c.a2 * zi * zi);
        return 20.0 * std::log10 (std::abs (h));
    }

private:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { double z1 = 0, z2 = 0; };

    std::array<Coeffs, kSections> coeffs_ {};
    std::array<State,  kSections> state_  {};
    double gain_ = 1.0;
};

/** Pre- and de-emphasis, engaged in delay mode only.

    Pre-emphasis lifts the treble before the converter and de-emphasis puts it
    back after, so the converter's noise gets turned down along with the lift.
    The hardware bypasses it in pitch mode because a transposed replay would
    shift the curve and leave the response unbalanced.

    The curve is not published. This uses the 50/15 us shelf, the standard for
    PCM gear of the period; it lifts the top end by up to 10.5 dB. Assumption,
    in one place: kT1 and kT2. */
class Emphasis
{
public:
    static constexpr double kT1 = 50e-6;
    static constexpr double kT2 = 15e-6;

    void design (double sampleRate)
    {
        // Pre: (1 + s T1) / (1 + s T2). De is its inverse. Bilinear, prewarped
        // at the geometric centre of the two corners.
        const double fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        const double fc = 1.0 / (2.0 * M_PI * std::sqrt (kT1 * kT2));
        const double k  = 2.0 * M_PI * fc / std::tan (M_PI * fc / fs);

        const double n0 = 1.0 + k * kT1, n1 = 1.0 - k * kT1;
        const double d0 = 1.0 + k * kT2, d1 = 1.0 - k * kT2;

        pre_ = { n0 / d0, n1 / d0, d1 / d0 };
        de_  = { d0 / n0, d1 / n0, n1 / n0 };
    }

    void reset() noexcept { preZ_ = deZ_ = 0.0; }

    float pre (float x) noexcept { return run (pre_, preZ_, x); }
    float de  (float x) noexcept { return run (de_,  deZ_,  x); }

private:
    struct FirstOrder { double b0 = 1, b1 = 0, a1 = 0; };

    static float run (const FirstOrder& c, double& z, float x) noexcept
    {
        const double y = c.b0 * x + z;
        z = c.b1 * x - c.a1 * y;
        return static_cast<float> (y);
    }

    FirstOrder pre_ {}, de_ {};
    double preZ_ = 0.0, deZ_ = 0.0;
};

} // namespace the89th
