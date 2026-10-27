#include "AudioCore.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
constexpr double tau = 6.283185307179586;
int checks = 0, failures = 0;
void require(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::vector<float> render(dualdeck::AudioCore& core, const dualdeck::ClipData& clip,
                         int rate, float mix, float decay, std::vector<float>* right = nullptr)
{
    core.stop(); core.render(nullptr, nullptr, 1); core.setVolume(1); core.prepare(rate);
    dualdeck::VoiceSettings settings; settings.fadeOut = 0;
    settings.reverbMix = mix; settings.reverbDecay = decay;
    core.trigger(1, &clip, settings);
    std::vector<float> output(rate * 2);
    if (right) right->resize(output.size());
    for (int i = 0; i < static_cast<int>(output.size()); i += 128)
        core.render(output.data() + i, right ? right->data() + i : nullptr,
                    std::min(128, static_cast<int>(output.size()) - i));
    return output;
}
double power(const std::vector<float>& pcm, int first, int last)
{
    double sum = 0;
    for (int i = first; i < last; ++i) sum += static_cast<double>(pcm[i]) * pcm[i];
    return sum / (last - first);
}
double density(const std::vector<float>& pcm, int first, int last)
{
    float peak = 0;
    for (int i = first; i < last; ++i) peak = std::max(peak, std::abs(pcm[i]));
    int active = 0;
    for (int i = first; i < last; ++i) active += std::abs(pcm[i]) > peak * 0.0001f;
    return static_cast<double>(active) / (last - first);
}
}
int main()
{
    using namespace dualdeck;
    auto core = std::make_unique<AudioCore>();
    ClipData impulse; impulse.sampleRate = 48000; impulse.samples = {0.5f, -0.35f};
    for (int rate : {44100, 48000, 96000, 192000})
    {
        std::vector<float> right;
        const auto longTail = render(*core, impulse, rate, 1, 2, &right);
        const double occupied = density(longTail, rate / 5, rate / 2);
        std::cout << "rate=" << rate << " tailDensity=" << occupied << '\n';
        require(occupied > 0.5, "a late reverb tail is diffuse rather than a sparse train of clicks");
        require(std::abs(longTail[0] - 0.5f) < 0.000001f && std::abs(right[0] + 0.35f) < 0.000001f,
                "diffusion preserves the first stereo dry transient at full mix");
        require(power(longTail, rate / 10, rate) > 1e-12 && power(right, rate / 10, rate) > 1e-12,
                "opposite-phase stereo retains wet energy in both channels");
        bool finite = true; float peak = 0;
        for (float sample : longTail) { finite &= std::isfinite(sample); peak = std::max(peak, std::abs(sample)); }
        require(finite && peak <= 0.98001f, "diffused tail stays finite and peak protected");
        const auto shortTail = render(*core, impulse, rate, 1, 0.2f);
        require(power(shortTail, rate / 2, rate) < power(longTail, rate / 2, rate) * 0.1,
                "short reverb decay reduces late energy independently of diffusion");
        const auto dry = render(*core, impulse, rate, 0, 2);
        const int dryFrames = static_cast<int>(std::ceil(rate / impulse.sampleRate));
        require(dry[0] == 0.5f && power(dry, dryFrames, rate) == 0,
                "zero reverb mix bypasses wet processing without a tail");
        double ratios[2] {};
        for (int kind = 0; kind < 2; ++kind)
        {
            ClipData burst; burst.sampleRate = rate; burst.samples.resize((rate / 20) * 2);
            for (int i = 0; i < burst.frames(); ++i)
            {
                const float sample = static_cast<float>(0.1 * std::sin(tau * (kind ? 10000 : 600) * i / rate));
                burst.samples[2 * i] = burst.samples[2 * i + 1] = sample;
            }
            const auto tail = render(*core, burst, rate, 1, 2);
            ratios[kind] = power(tail, rate * 6 / 10, rate * 9 / 10)
                / std::max(1e-30, power(tail, rate / 10, rate / 5));
            core->resetAfterDeviceStop();
        }
        std::cout << "rate=" << rate << " lowDecayRatio=" << ratios[0] << " highDecayRatio=" << ratios[1] << '\n';
        require(ratios[1] < ratios[0] * 0.2,
                "high-frequency reverb energy decays faster than low-frequency energy");

        // Reuse the same first voice slot after a sounding reverb, without a
        // prepare/clear of the backing buffers. Epochs must hide the old tail.
        const auto filled = render(*core, impulse, rate, 1, 2);
        core->stop(); core->render(nullptr, nullptr, 1);
        ClipData silent; silent.sampleRate = rate; silent.samples = {0, 0};
        VoiceSettings settings; settings.fadeOut = 0; settings.reverbMix = 1;
        core->trigger(2, &silent, settings);
        std::vector<float> reused(rate);
        core->render(reused.data(), nullptr, rate);
        require(power(reused, 0, rate) == 0, "a reused silent pad does not inherit another pad's reverb state");
        core->resetAfterDeviceStop();
    }
    std::cout << (failures ? "FAILED: " : "PASS: ") << checks << " reverb checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
