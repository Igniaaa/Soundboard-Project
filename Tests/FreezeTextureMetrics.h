#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace freeze_test
{
constexpr double tau = 6.283185307179586;
// Deterministic PCM with a broadband or band-limited stochastic component,
// optionally mixed with a voiced harmonic. The right channel is anti-phase.
inline std::vector<float> source(int rate, int kind, unsigned seed = 1234567u)
{
    std::vector<float> pcm(rate * 4);
    double low = 0.0, lower = 0.0;
    for (int i = 0; i < rate * 2; ++i)
    {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const double noise = 0.3 * (static_cast<double>(seed) / 2147483648.0 - 1.0);
        low += (1.0 - std::exp(-tau * 6000.0 / rate)) * (noise - low);
        lower += (1.0 - std::exp(-tau * 1800.0 / rate)) * (noise - lower);
        const float sample = static_cast<float>(kind == 0 ? noise :
            2.0 * (low - lower) + (kind == 2 ? 0.1 * std::sin(tau * 233.0 * i / rate) : 0.0));
        pcm[2 * i] = sample;
        pcm[2 * i + 1] = -0.7f * sample;
    }
    return pcm;
}

// A long-window spectral flatness measurement detects the artificial narrow
// lines a frozen noise frame acquires. Measure inside the source's noise band.
inline double flatness(const std::vector<float>& audio, int rate, int begin)
{
    constexpr int n = 16384;
    std::vector<std::complex<double>> bins(n);
    for (int i = 0; i < n; ++i)
        bins[i] = audio[begin + i] * (0.5 - 0.5 * std::cos(tau * i / n));
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(bins[i], bins[j]);
    }
    for (int length = 2; length <= n; length *= 2)
    {
        const auto increment = std::polar(1.0, -tau / length);
        for (int base = 0; base < n; base += length)
        {
            std::complex<double> phase(1.0, 0.0);
            for (int j = 0; j < length / 2; ++j)
            {
                const auto a = bins[base + j], b = bins[base + j + length / 2] * phase;
                bins[base + j] = a + b; bins[base + j + length / 2] = a - b;
                phase *= increment;
            }
        }
    }
    double power = 0.0, logarithm = 0.0;
    const int first = 2000 * n / rate, last = 6000 * n / rate;
    for (int i = first; i < last; ++i)
    {
        const double p = std::norm(bins[i]);
        power += p; logarithm += std::log(std::max(p, 1.0e-30));
    }
    return std::exp(logarithm / (last - first)) / (power / (last - first));
}
}
