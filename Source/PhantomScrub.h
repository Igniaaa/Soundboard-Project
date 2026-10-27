#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace dualdeck
{
// A virtual playhead always runs at 1x, independently of cursor direction.
// Waveform similarity aligns its replacements before a smooth overlap. Both
// channels use the same offset and envelope, including anti-phase stereo.
class PhantomScrub
{
public:
    void prepare(double rate, int hopFrames)
    {
        outputRate = rate;
        overlapFrames = std::max(1, hopFrames);
        searchFrames = std::max(1, static_cast<int>(std::round(rate * 0.024)));
        attackOverlapFrames = std::clamp(static_cast<int>(std::round(rate * 0.002)), 1, overlapFrames);
    }

    void start(const float* pcm, int count, double rate, double position)
    {
        samples = pcm;
        frames = count;
        step = rate / outputRate;
        readPosition = previousPosition = clampPosition(position);
        fadeRemaining = 0;
        randomState = 0x9e3779b9u;
    }

    void retarget(double desired, bool attack)
    {
        desired = clampPosition(desired);
        if (!attack && std::abs(desired - readPosition) < 2.0 * step) return;
        float correlation = 1.0f;
        const double aligned = attack ? desired : align(desired, correlation, 0.0, frames - 1.0, searchFrames);
        replace(aligned, attack, correlation);
    }

    // Keep both overlapping readers inside the frozen analysis neighbourhood.
    // Noise needs varying grains; a similarity search alone eventually locks
    // onto the same short loop and turns a breath/cymbal into a pitched buzz.
    void hold(double begin, double end, float texture)
    {
        const double first = clampPosition(begin);
        const double last = std::max(first, clampPosition(end - 2.0 * overlapFrames * step));
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        const double random = static_cast<double>(randomState) / 4294967296.0;
        const double desired = first + (last - first) * (0.5 + (random - 0.5) * texture);
        float correlation = 1.0f;
        const int search = static_cast<int>(searchFrames * (1.0f - texture) * (1.0f - texture));
        const double aligned = align(desired, correlation, first, last, search);
        replace(aligned, false, correlation);
    }

private:
    void replace(double aligned, bool attack, float correlation)
    {
        if (std::abs(aligned - readPosition) < 0.5 * step) return;
        previousPosition = readPosition;
        readPosition = aligned;
        fadeLength = attack ? attackOverlapFrames : overlapFrames;
        fadeRemaining = fadeLength;
        overlapCorrelation = attack ? 1.0f : std::clamp(correlation, 0.0f, 1.0f);
    }

public:
    std::pair<float, float> next()
    {
        auto sample = sampleAt(readPosition);
        readPosition = clampPosition(readPosition + step);
        if (fadeRemaining > 0)
        {
            const auto previous = sampleAt(previousPosition);
            previousPosition = clampPosition(previousPosition + step);
            const float fraction = static_cast<float>(fadeLength - fadeRemaining) / fadeLength;
            const float weight = 0.5f - 0.5f * std::cos(3.14159265358979323846f * fraction);
            // Compensate only for uncorrelated overlaps. Aligned waves retain
            // unity gain rather than gaining 3 dB at the middle of each splice.
            const float gain = 1.0f / std::sqrt(1.0f - 2.0f * weight * (1.0f - weight)
                                                               * (1.0f - overlapCorrelation));
            sample.first = (previous.first + (sample.first - previous.first) * weight) * gain;
            sample.second = (previous.second + (sample.second - previous.second) * weight) * gain;
            --fadeRemaining;
        }
        return sample;
    }

    double position() const noexcept { return readPosition; }

    std::pair<float, float> sampleAt(double position) const noexcept
    {
        position = clampPosition(position);
        const int first = static_cast<int>(position);
        const int second = std::min(first + 1, frames - 1);
        const float fraction = static_cast<float>(position - first);
        const auto safe = [this](int frame, int channel)
        {
            const float value = samples[static_cast<std::size_t>(frame) * 2 + channel];
            return std::isfinite(value) ? std::clamp(value, -8.0f, 8.0f) : 0.0f;
        };
        const float left = safe(first, 0), right = safe(first, 1);
        return {left + fraction * (safe(second, 0) - left),
                right + fraction * (safe(second, 1) - right)};
    }

private:
    double clampPosition(double position) const noexcept
    {
        return std::clamp(position, 0.0, static_cast<double>(frames - 1));
    }

    double align(double desired, float& correlation, double first, double last, int search) const
    {
        // A bounded coarse-to-fine search keeps the callback cost independent
        // of file length. Do not sum L/R: opposite-phase content must survive.
        constexpr int points = 48;
        std::array<std::pair<float, float>, points> reference;
        double referencePower = 0.0;
        const double stride = static_cast<double>(overlapFrames) / points;
        for (int i = 0; i < points; ++i)
        {
            reference[i] = sampleAt(readPosition + i * stride * step);
            referencePower += reference[i].first * reference[i].first
                            + reference[i].second * reference[i].second;
        }
        if (referencePower < 1.0e-9) return desired;
        const auto score = [&](int offset, float& similarity)
        {
            double dot = 0.0, power = 0.0;
            const double candidate = std::clamp(desired + offset * step, first, last);
            for (int i = 0; i < points; ++i)
            {
                const auto sample = sampleAt(candidate + i * stride * step);
                dot += reference[i].first * sample.first + reference[i].second * sample.second;
                power += sample.first * sample.first + sample.second * sample.second;
            }
            similarity = power > 1.0e-9 ? static_cast<float>(dot / std::sqrt(referencePower * power)) : 0.0f;
            return similarity - 0.001f * static_cast<float>(std::abs(offset)) / searchFrames;
        };
        int bestOffset = 0;
        float bestScore = score(0, correlation);
        // Bound the number of full stereo comparisons at every sample rate.
        // The previous fixed four-sample stride scanned up to 2305 candidates
        // per hop at 192 kHz, concentrating work in one small audio buffer.
        const int coarse = std::max(4, (2 * search + 47) / 48);
        for (int offset = -search; offset <= search; offset += coarse)
        {
            float similarity = 0.0f;
            const float value = score(offset, similarity);
            if (value > bestScore) { bestScore = value; bestOffset = offset; correlation = similarity; }
        }
        for (int radius = coarse; radius > 1;)
        {
            const int centre = bestOffset;
            const int refinementStep = std::max(1, radius / 4);
            for (int offset = std::max(-search, centre - radius);
                 offset <= std::min(search, centre + radius); offset += refinementStep)
            {
                float similarity = 0.0f;
                const float value = score(offset, similarity);
                if (value > bestScore) { bestScore = value; bestOffset = offset; correlation = similarity; }
            }
            radius = refinementStep;
        }
        return std::clamp(desired + bestOffset * step, first, last);
    }

    const float* samples = nullptr;
    int frames = 0, overlapFrames = 1, searchFrames = 1, attackOverlapFrames = 1;
    int fadeLength = 1, fadeRemaining = 0;
    double outputRate = 48000.0, step = 1.0, readPosition = 0.0, previousPosition = 0.0;
    float overlapCorrelation = 1.0f;
    std::uint32_t randomState = 0x9e3779b9u;
};
}


