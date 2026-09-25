#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace the89th
{

/** Kaiser-windowed sinc, tabulated, for reading a stream at fractional times.

    Why not cubic: the band edge always sits at 0.378 of the machine's internal
    rate (5k/13227.5, 10k/26455, 20k/52910), because the clock scales with the
    bandwidth. A linear read loses 4.3 dB there and a cubic about 1.5 dB, which
    would put the overall band edge well below the published -3 dB. 64 taps
    stay flat past 0.4 of the rate. */
class SincKernel
{
public:
    static constexpr int kTaps   = 64;
    static constexpr int kHalf   = kTaps / 2;
    static constexpr int kPhases = 512;

    SincKernel()
    {
        constexpr double cutoff = 0.9;   // of Nyquist
        constexpr double beta   = 8.6;   // ~85 dB sidelobes
        const double i0beta = besselI0 (beta);

        for (int ph = 0; ph <= kPhases; ++ph)
        {
            const double frac = static_cast<double> (ph) / kPhases;
            double sum = 0.0;
            auto& row = table_[static_cast<std::size_t> (ph)];

            for (int k = 0; k < kTaps; ++k)
            {
                const double t = static_cast<double> (k - kHalf + 1) - frac;   // tap k sits at floor(pos) - kHalf + 1 + k
                const double x = t / kHalf;
                const double w = std::fabs (x) >= 1.0 ? 0.0
                               : besselI0 (beta * std::sqrt (1.0 - x * x)) / i0beta;
                const double s = t == 0.0 ? 1.0 : std::sin (M_PI * cutoff * t) / (M_PI * cutoff * t);
                row[static_cast<std::size_t> (k)] = static_cast<float> (cutoff * s * w);
                sum += cutoff * s * w;
            }

            // Unity DC gain at every phase, or a slow sweep through the phases
            // would ripple the level.
            for (auto& c : row)
                c = static_cast<float> (c / sum);
        }
    }

    /** Value of `ring` at fractional index `pos`. Reads floor(pos)-31 .. floor(pos)+32. */
    float read (const float* ring, long long mask, double pos) const noexcept
    {
        const double    fl   = std::floor (pos);
        const long long base = static_cast<long long> (fl) - kHalf + 1;
        const double    ph   = (pos - fl) * kPhases;
        const int       p0   = static_cast<int> (ph);
        const float     mu   = static_cast<float> (ph - p0);

        const auto& a = table_[static_cast<std::size_t> (p0)];
        const auto& b = table_[static_cast<std::size_t> (p0 + 1)];

        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k)
        {
            const float c = a[static_cast<std::size_t> (k)]
                          + mu * (b[static_cast<std::size_t> (k)] - a[static_cast<std::size_t> (k)]);
            acc += c * ring[static_cast<std::size_t> ((base + k) & mask)];
        }
        return acc;
    }

    static const SincKernel& instance()
    {
        static const SincKernel k;
        return k;
    }

private:
    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    std::array<std::array<float, kTaps>, kPhases + 1> table_ {};
};

/** Host rate to the machine's internal rate and back, both channels in lockstep.

    The machine has to run at the converter clock. Emulating a bandwidth switch
    at host rate by rescaling the delay would keep the delay ranges and lose the
    octave jumps, which come from replaying stored addresses at a new clock.

    Both channels step together, one internal sample at a time, because in
    quasi-stereo they share one memory and one write, and the feedback merges
    both outputs. Stepping them one after the other cannot express that.

    One time base, in host samples, drives both directions, so the two sides
    cannot drift apart: the machine only runs far enough ahead to feed the next
    output sample.

    Band-limiting is not done here. The anti-alias filter sits in front at host
    rate and the reconstruction filter after, in BandLimit. */
class RateAdapter
{
public:
    static constexpr int       kRingSize = 8192;
    static constexpr long long kMask     = kRingSize - 1;
    static constexpr int       kHalf     = SincKernel::kHalf;

    void prepare (double hostRate, double internalRateHz)
    {
        // Build the kernel table here, off the audio thread. Left lazy, the
        // first audio block paid for it: measured at 86% of a 128-sample block.
        (void) SincKernel::instance();

        hostRate_ = hostRate > 0.0 ? hostRate : 48000.0;
        for (auto& r : in_)  r.assign (static_cast<std::size_t> (kRingSize), 0.0f);
        for (auto& r : out_) r.assign (static_cast<std::size_t> (kRingSize), 0.0f);
        setInternalRate (internalRateHz);
        reset();
    }

    /** Safe mid-stream. A bandwidth switch changes only the clock.

        The output read trails machine production by lead(), which depends on the
        clock. Moving the read position by the change keeps that trail exact, so
        the latency reported to the host stays true after a switch. It moves at
        the moment of the switch only, which is an octave jump anyway. */
    void setInternalRate (double r) noexcept
    {
        const double leadBefore = lead();
        internalRate_ = r > 0.0 ? r : hostRate_;
        ratio_        = internalRate_ / hostRate_;
        invRatio_     = hostRate_ / internalRate_;
        outPosInternal_ -= lead() - leadBefore;
    }

    void reset() noexcept
    {
        for (auto& r : in_)  std::fill (r.begin(), r.end(), 0.0f);
        for (auto& r : out_) std::fill (r.begin(), r.end(), 0.0f);
        inWritten_           = 0;
        machineIndex_        = 0;
        nextMachineTimeHost_ = 0.0;
        outPosInternal_      = -lead();
    }

    /** step (inL, inR, outL, outR) runs once per internal sample, in order. */
    template <class Fn>
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, Fn&& step)
    {
        const auto& k = SincKernel::instance();

        for (int i = 0; i < n; ++i)
        {
            const auto slot = static_cast<std::size_t> (inWritten_ & kMask);
            in_[0][slot] = inL[i];
            in_[1][slot] = inR[i];
            ++inWritten_;

            // The kernel reads kHalf samples past floor(pos), so stay that far back.
            const double horizon = static_cast<double> (inWritten_ - 1 - kHalf);
            const double needed  = outPosInternal_ + kHalf + 1.0;

            while (static_cast<double> (machineIndex_) < needed
                   && nextMachineTimeHost_ <= horizon)
            {
                const float xl = k.read (in_[0].data(), kMask, nextMachineTimeHost_);
                const float xr = k.read (in_[1].data(), kMask, nextMachineTimeHost_);
                float yl = 0.0f, yr = 0.0f;
                step (xl, xr, yl, yr);

                const auto o = static_cast<std::size_t> (machineIndex_ & kMask);
                out_[0][o] = yl;
                out_[1][o] = yr;
                ++machineIndex_;
                nextMachineTimeHost_ += invRatio_;
            }

            // Never read past what the machine has produced. With the lead set in
            // reset() this holds by construction; the clamp is a backstop.
            // Phase 0 read ahead here and picked up output from one ring lap
            // earlier, which a steady tone hides and an impulse exposes.
            outPosInternal_ = std::min (outPosInternal_, static_cast<double> (machineIndex_ - kHalf - 1));

            outL[i] = k.read (out_[0].data(), kMask, outPosInternal_);
            outR[i] = k.read (out_[1].data(), kMask, outPosInternal_);
            outPosInternal_ += ratio_;
        }
    }

    /** Conversion latency only; the machine's own delay is the effect. */
    double latencySamples() const noexcept { return lead() * invRatio_; }

    double internalSampleRate() const noexcept { return internalRate_; }

private:
    /** How far the output read trails machine production, in internal samples:
        the output kernel's lookahead, plus the input kernel's lookahead converted
        from host samples, plus a sample of margin for the fractional phases. */
    double lead() const noexcept
    {
        return (kHalf + 1.0) + (kHalf + 1.0) * ratio_ + 2.0;
    }

    std::array<std::vector<float>, 2> in_, out_;

    double hostRate_     = 48000.0;
    double internalRate_ = 26455.0;
    double ratio_        = 1.0;
    double invRatio_     = 1.0;

    long long inWritten_    = 0;
    long long machineIndex_ = 0;
    double nextMachineTimeHost_ = 0.0;
    double outPosInternal_      = 0.0;
};

} // namespace the89th
