#pragma once

#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

namespace test_support
{

inline std::vector<float> sine (int n, double freqHz, double sampleRate, float amp = 0.9f)
{
    std::vector<float> v (static_cast<std::size_t> (n));
    const double w = 2.0 * M_PI * freqHz / sampleRate;
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t> (i)] = amp * static_cast<float> (std::sin (w * i));
    return v;
}

inline std::vector<float> noise (int n, unsigned seed = 1234u, float amp = 0.9f)
{
    std::vector<float> v (static_cast<std::size_t> (n));
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-amp, amp);
    for (auto& x : v)
        x = d (rng);
    return v;
}

/** Normalised autocorrelation period estimate with parabolic refinement.

    Robust across a splice, which a zero-crossing count is not: a spliced
    waveform picks up a few spurious crossings that skew the count. */
inline double estimatePeriod (const float* x, int n, int minLag, int maxLag)
{
    if (maxLag >= n)
        maxLag = n - 1;

    std::vector<double> r (static_cast<std::size_t> (maxLag + 1), 0.0);
    double best    = -2.0;
    int    bestLag = minLag;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double num = 0.0, e1 = 0.0, e2 = 0.0;
        for (int i = 0; i + lag < n; ++i)
        {
            const double a = x[i];
            const double b = x[i + lag];
            num += a * b;
            e1  += a * a;
            e2  += b * b;
        }

        const double d = std::sqrt (e1 * e2);
        const double c = d > 0.0 ? num / d : 0.0;
        r[static_cast<std::size_t> (lag)] = c;

        if (c > best)
        {
            best    = c;
            bestLag = lag;
        }
    }

    // A pure sine correlates just as well at every multiple of its period, so
    // the global argmax lands on an arbitrary octave: a 880 Hz tone reads as
    // 440 Hz whenever lag 60 edges out lag 30 by a rounding error. Take instead
    // the shortest lag that is a local peak within tolerance of the best.
    const double threshold = 0.90 * best;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
    {
        const double c = r[static_cast<std::size_t> (lag)];
        if (c >= threshold
            && c >= r[static_cast<std::size_t> (lag - 1)]
            && c >= r[static_cast<std::size_t> (lag + 1)])
        {
            bestLag = lag;
            break;
        }
    }

    if (bestLag > minLag && bestLag < maxLag)
    {
        const double a = r[static_cast<std::size_t> (bestLag - 1)];
        const double b = r[static_cast<std::size_t> (bestLag)];
        const double c = r[static_cast<std::size_t> (bestLag + 1)];
        const double denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1e-12)
            return bestLag + 0.5 * (a - c) / denom;
    }

    return bestLag;
}

inline double estimateFreq (const float* x, int n, double sampleRate,
                            double minHz = 100.0, double maxHz = 4000.0)
{
    const int minLag = static_cast<int> (sampleRate / maxHz);
    const int maxLag = static_cast<int> (sampleRate / minHz);
    return sampleRate / estimatePeriod (x, n, std::max (2, minLag), maxLag);
}

inline float maxAbsDelta (const float* x, int from, int to)
{
    float m = 0.0f;
    for (int i = from + 1; i < to; ++i)
        m = std::max (m, std::abs (x[i] - x[i - 1]));
    return m;
}

} // namespace test_support
