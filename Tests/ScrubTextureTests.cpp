#include "SpectralScrub.h"
#include "FreezeTextureMetrics.h"
#include <cstdlib>
#include <iostream>

namespace
{
int checks = 0, failures = 0;
void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

// A stochastic resonance supplies non-periodic material for the short/long
// file comparison, including stops after dragging in either direction.
std::vector<float> resonance(int rate)
{
    std::vector<float> pcm(rate * 4);
    std::uint32_t seed = 1234567u;
    const double angle = freeze_test::tau * 900 / rate;
    const double decay = std::exp(-freeze_test::tau * 40 / rate);
    double real = 0.0, imaginary = 0.0;
    for (int i = 0; i < rate * 2; ++i)
    {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        const double next = decay * (real * std::cos(angle) - imaginary * std::sin(angle))
            + (seed / 2147483648.0 - 1.0) * 0.04;
        imaginary = decay * (real * std::sin(angle) + imaginary * std::cos(angle));
        real = next;
        pcm[2 * i] = static_cast<float>(real);
        pcm[2 * i + 1] = -0.7f * pcm[2 * i];
    }
    return pcm;
}

double coherentAmplitude(const std::vector<float>& audio, int rate, double frequency)
{
    double real = 0.0, imaginary = 0.0;
    for (int i = rate; i < rate * 2; ++i)
    {
        const double phase = freeze_test::tau * frequency * i / rate;
        real += audio[i] * std::cos(phase);
        imaginary += audio[i] * std::sin(phase);
    }
    return 2.0 * std::hypot(real, imaginary) / rate;
}
}

int main()
{
    constexpr int rate = 48000;
    const auto shortPcm = resonance(rate);
    std::vector<float> longPcm(rate * 180 * 2);
    for (int offset : {60, 178})
        std::copy(shortPcm.begin(), shortPcm.end(), longPcm.begin() + offset * rate * 2);

    for (int outputRate : {44100, 48000, 96000, 192000})
    {
        // A voiced sound and its overtones must survive the noisy background
        // with stable phase. Maximising flatness alone rewards a less coherent
        // sound, which the listening feedback explicitly rejected.
        auto voiced = freeze_test::source(rate, 2);
        for (int i = 0; i < rate * 2; ++i)
        {
            const float harmonics = static_cast<float>(0.07 * std::sin(freeze_test::tau * 466 * i / rate)
                + 0.05 * std::sin(freeze_test::tau * 699 * i / rate));
            voiced[2 * i] += harmonics;
            voiced[2 * i + 1] -= 0.7f * harmonics;
        }
        dualdeck::SpectralScrub harmonicScrub;
        harmonicScrub.prepare(outputRate);
        harmonicScrub.start(voiced.data(), rate * 2, rate, 0.5);
        std::vector<float> harmonicOutput(outputRate * 2);
        double harmonicStereoError = 0.0;
        bool harmonicFinite = true;
        for (auto& value : harmonicOutput)
        {
            const auto sample = harmonicScrub.next(0.5);
            value = sample.first;
            harmonicFinite &= std::isfinite(sample.first) && std::isfinite(sample.second);
            harmonicStereoError = std::max(harmonicStereoError,
                static_cast<double>(std::abs(sample.second + 0.7f * sample.first)));
        }
        const double fundamental = coherentAmplitude(harmonicOutput, outputRate, 233);
        const double overtone = coherentAmplitude(harmonicOutput, outputRate, 466);
        std::cout << "harmonics rate=" << outputRate << " fundamental=" << fundamental
            << " overtone=" << overtone << '\n';
        // Nominal source amplitudes are 0.10 and 0.07. Require at least
        // approximately 70% to remain coherent rather than dispersed in noise.
        require(fundamental > 0.07 && fundamental < 0.13,
            "a noisy voiced freeze retains a coherent fundamental without excessive gain");
        require(overtone > 0.05 && overtone < 0.10,
            "a noisy voiced freeze retains a coherent second harmonic without excessive gain");
        require(harmonicFinite && harmonicStereoError < 0.00004,
            "spectral harmonic balance preserves finite stereo audio");
        for (double speed : {0.0, -0.125, 0.125})
        {
            std::vector<float> shortOutput;
            for (int offset : {0, 60, 178})
            {
                dualdeck::SpectralScrub scrub;
                scrub.prepare(outputRate);
                const double target = offset + 0.5;
                scrub.start(offset == 0 ? shortPcm.data() : longPcm.data(),
                    offset == 0 ? rate * 2 : rate * 180, rate, target - speed * 0.25);
                // Actual mouse events, followed by a held cursor at the same
                // fragment in both directions. Include the settled freeze.
                const int updateFrames = outputRate / 60;
                for (int i = 0; i < outputRate / 4; ++i)
                    scrub.next(target - speed * 0.25
                        + speed * ((i / updateFrames) * updateFrames) / outputRate);
                std::vector<float> audio(outputRate * 2);
                double stereoError = 0.0, power = 0.0;
                bool finite = true;
                for (int i = 0; i < outputRate * 2; ++i)
                {
                    const auto sample = scrub.next(target);
                    audio[i] = sample.first;
                    finite &= std::isfinite(sample.first) && std::isfinite(sample.second);
                    stereoError = std::max(stereoError, static_cast<double>(std::abs(sample.second + 0.7f * sample.first)));
                    if (i >= outputRate) power += sample.first * sample.first;
                }
                std::cout << "rate=" << outputRate << " offset=" << offset << " speed=" << speed
                    << '\n';
                require(finite && stereoError < 0.00004 && power / outputRate > 0.0002 && power / outputRate < 0.04,
                    "freeze after either drag direction keeps finite stereo audio at a bounded level");
                if (offset == 0) shortOutput = audio;
                else
                {
                    double difference = 0.0;
                    for (int i = 0; i < outputRate * 2; ++i)
                        difference = std::max(difference, static_cast<double>(std::abs(audio[i] - shortOutput[i])));
                    require(difference < 0.00004,
                        "the same fragment retains its sound at 60 and 178 seconds in a long file");
                }
            }
        }
    }
    std::cout << checks << " texture checks, " << failures << " failures\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
