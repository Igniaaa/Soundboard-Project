#include "SpectralScrub.h"
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
constexpr double twoPi = 6.2831853071795864769;
int checks = 0;
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    ++checks;
}
double level(const std::vector<float>& audio, int first, int last)
{
    double power = 0;
    for (int i = first; i < last; ++i) power += audio[i] * audio[i];
    return std::sqrt(power / (last - first));
}
}

int main()
{
    // A rising note distinguishes time stretching from rate transposition:
    // pitch must follow the selected source time, independently of drag speed.
    // Use different PCM/device rates and real, sample-and-held mouse events.
    constexpr int sourceRate = 48000, frames = sourceRate * 5;
    std::vector<float> pcm(frames * 2);
    for (int i = 0; i < frames; ++i)
    {
        const double time = static_cast<double>(i) / sourceRate;
        const float sample = static_cast<float>(0.2 * std::sin(twoPi * (220 * time + 75 * time * time)));
        pcm[2 * i] = sample;
        pcm[2 * i + 1] = -0.7f * sample;
    }
    dualdeck::SpectralScrub scrub;
    double worstPitchError = 0;
    for (const int outputRate : {44100, 48000, 96000, 192000})
    {
        scrub.prepare(outputRate);
        for (const double speed : {0.125, 0.5, 1.0, 2.0, 4.0, -0.5, -2.0, -4.0})
        {
            for (const int mouseHz : {30, 60})
            {
                scrub.start(pcm.data(), frames, sourceRate, 2.0);
                std::vector<float> output(outputRate / 2);
                const int updateFrames = outputRate / mouseHz;
                double stereoError = 0;
                bool finite = true;
                for (int i = 0; i < static_cast<int>(output.size()); ++i)
                {
                    const double target = 2.0 + speed * ((i / updateFrames) * updateFrames) / outputRate;
                    const auto sample = scrub.next(target);
                    output[i] = sample.first;
                    finite &= std::isfinite(sample.first) && std::isfinite(sample.second);
                    stereoError = std::max(stereoError, static_cast<double>(std::abs(sample.second + 0.7f * sample.first)));
                }
                const int first = outputRate / 5, last = outputRate * 2 / 5;
                int crossings = 0;
                double expectedCycles = 0;
                for (int i = first; i < last; ++i)
                {
                    if (output[i - 1] <= 0 && output[i] > 0) ++crossings;
                    const double target = 2.0 + speed * ((i / updateFrames) * updateFrames) / outputRate;
                    expectedCycles += (220 + 150 * target) / outputRate;
                }
                const double errorHz = std::abs(crossings - expectedCycles) * outputRate / (last - first);
                worstPitchError = std::max(worstPitchError, errorHz);
                // The spectral window/overlap can lag a changing note by up
                // to 50 ms; crossing counts have 5 Hz resolution. This bound
                // still rejects rate transposition by hundreds of hertz.
                const double toleranceHz = 5.0 + 150.0 * std::abs(speed) * 0.05;
                if (errorHz > toleranceHz)
                    std::cerr << "rate=" << outputRate << " speed=" << speed << " mouse=" << mouseHz
                              << " pitchErrorHz=" << errorHz << '\n';
                require(finite && stereoError < 0.00004, "time-stretched motion remains finite and preserves stereo phase");
                require(errorHz <= toleranceHz, "a dragged rising note follows source pitch at slow, fast and reverse speeds");
                require(level(output, first, last) > 0.08 && level(output, first, last) < 0.21,
                        "time-stretched motion retains an audible, bounded level");
            }
        }
    }
    std::cout << "PASS: " << checks << " time-stretch checks; worst rising-note pitch error "
              << worstPitchError << " Hz (200 ms crossing-count windows)\n";
}
