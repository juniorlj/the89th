#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "DelayMemory.hpp"
#include "SpliceTraversal.hpp"

namespace the89th
{

/** Signal-informed splicing.

    The original moves each splice so it lands where the waveform already
    matches, which changes the segment length a little every time. It did that
    in wired logic, and nobody outside the factory has published how. Robert
    Henke, whose re-creation is the reference, calls it autocorrelation and says
    his does not sound the same. So this is a model of the behaviour, not of the
    circuit.

    The model: at the moment a splice begins, search a window around where the
    incoming head would land for the offset whose memory best matches what the
    outgoing head is playing, by normalised cross-correlation, refined to a
    fraction of a sample. Move the incoming head there. Both heads then step
    through memory at the same rate from matched starting points, so they stay
    in phase through the fade.

    The fade shape follows the match: equal gain when the two heads carry the
    same waveform (equal power would bump that by 3 dB), equal power when they
    don't. The analog crossfade stage is unidentified, so the curve is derived
    from the signal rather than from a known circuit.

    Search and window are capped, so a splice costs at most about half a million
    multiply-adds, once, at the moment it starts. */
class Xing
{
public:
    static constexpr int kMaxSearch = 512;
    static constexpr int kMaxWindow = 512;
    static constexpr int kMinWindow = 96;

    struct Match
    {
        double shift       = 0.0;    // applied to the incoming head, in samples of delay
        float  correlation = 0.0f;   // at the chosen offset, 0..1
        int    candidates  = 0;      // offsets examined; 0 means no search was possible
    };

    Match match (const DelayMemory& mem, const SpliceTraversal& t) noexcept
    {
        last_ = {};

        const double dOut = t.primaryDelay();
        const double dIn  = t.secondary().delaySamples;

        const int lo = DelayMemory::kMinDelay;
        const int hi = mem.words() - DelayMemory::kEndGuard - 1;

        const double speed = std::max (std::fabs (t.signedRate()), 0.25);
        const int n = std::min (2 * std::clamp (static_cast<int> (t.activeFade() * speed), kMinWindow / 2, kMaxWindow / 2),
                                hi - lo + 1);
        if (n < 32)
            return last_;

        const int io = static_cast<int> (std::lround (dOut));
        const int ii = static_cast<int> (std::lround (dIn));

        // Centre the window on the outgoing head where memory allows. Near the
        // shallow bound it sits right behind the write head, with nothing
        // written ahead of it, so slide the window onto the side it has already
        // played. The same offset applies to every candidate, so the comparison
        // stays like for like.
        const int off = std::clamp (-n / 2, lo - io, hi - (n - 1) - io);
        if (io + off < lo || io + off + n - 1 > hi)
            return last_;

        const int reach = std::min (kMaxSearch, static_cast<int> (0.25 * t.regionLength()));

        // The incoming head must stay inside the crosspoint region for the
        // whole fade: handover clamps it into the region, and a clamp is a jump.
        // Launch puts it on the entry bound, so in practice this makes the
        // search one-sided, inward, which shortens the segment.
        const double travel = t.activeFade() * t.step();
        const double rl = t.regionLo(), rh = t.regionHi();
        const int inLo = static_cast<int> (std::ceil  (std::max (rl - dIn, rl - dIn - travel)));
        const int inHi = static_cast<int> (std::floor (std::min (rh - dIn, rh - dIn - travel)));

        const int tMin = std::max ({ -reach, lo - off - ii,           inLo });
        const int tMax = std::min ({  reach, hi - (n - 1) - off - ii, inHi });
        if (tMax - tMin < 2)
            return last_;

        // The outgoing window never moves, so read it once.
        double ex = 0.0;
        for (int j = 0; j < n; ++j)
        {
            x_[static_cast<std::size_t> (j)] = mem.atDelay (io + off + j);
            ex += static_cast<double> (x_[static_cast<std::size_t> (j)]) * x_[static_cast<std::size_t> (j)];
        }
        if (ex < 1e-12)
            return last_;  // silence: nothing to match

        // Incoming window energy slides along with the offset.
        double ey = 0.0;
        for (int j = 0; j < n; ++j)
        {
            const double y = mem.atDelay (ii + tMin + off + j);
            ey += y * y;
        }

        double best = -2.0, prev = 0.0, beforeBest = 0.0, afterBest = 0.0;
        int    bestTau = 0;
        bool   wantAfter = false;

        for (int tau = tMin; tau <= tMax; ++tau)
        {
            if (tau > tMin)
            {
                const double out = mem.atDelay (ii + tau - 1 + off);
                const double in  = mem.atDelay (ii + tau - 1 + off + n);
                ey += in * in - out * out;
            }

            double xy = 0.0;
            const int base = ii + tau + off;
            for (int j = 0; j < n; ++j)
                xy += static_cast<double> (x_[static_cast<std::size_t> (j)]) * mem.atDelay (base + j);

            const double rho = xy / std::sqrt (ex * std::max (ey, 1e-12));

            if (wantAfter)
            {
                afterBest = rho;
                wantAfter = false;
            }
            if (rho > best)
            {
                best       = rho;
                bestTau    = tau;
                beforeBest = prev;
                wantAfter  = true;
            }
            prev = rho;
        }

        // Parabolic refinement to a fraction of a sample.
        double frac = 0.0;
        if (bestTau > tMin && bestTau < tMax)
        {
            const double denom = beforeBest - 2.0 * best + afterBest;
            if (std::fabs (denom) > 1e-12)
                frac = std::clamp (0.5 * (beforeBest - afterBest) / denom, -0.5, 0.5);
        }

        // Correlation was measured between rounded positions. Place the incoming
        // head so its true offset from the outgoing one equals the matched lag.
        const double target = dOut + static_cast<double> (ii + bestTau - io) + frac;

        last_.shift       = target - dIn;
        last_.correlation = static_cast<float> (std::clamp (best, 0.0, 1.0));
        last_.candidates  = tMax - tMin + 1;
        return last_;
    }

    const Match& last() const noexcept { return last_; }

private:
    std::array<float, kMaxWindow> x_ {};
    Match last_ {};
};

} // namespace the89th
