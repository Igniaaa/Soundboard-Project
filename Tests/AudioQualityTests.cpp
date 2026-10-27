#include "AudioCore.h"
#include "FreezeTextureMetrics.h"
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
int checks = 0;
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    ++checks;
}
std::vector<float> render(dualdeck::AudioCore& core, const dualdeck::ClipData& clip,
                          const dualdeck::VoiceSettings& settings, int frames)
{
    core.stop(); core.render(nullptr, nullptr, 1);
    core.trigger(1, &clip, settings);
    std::vector<float> result(frames);
    for (int i = 0; i < frames; i += 128)
        core.render(result.data() + i, nullptr, std::min(128, frames - i));
    return result;
}
}

int main()
{
    using namespace dualdeck;
    constexpr int rate = 48000;
    auto core = std::make_unique<AudioCore>();
    core->setVolume(1); core->prepare(rate);
    ClipData quiet, loud;
    quiet.sampleRate = loud.sampleRate = rate;
    quiet.samples.resize(rate * 6); loud.samples.resize(rate * 6);
    for (int i = 0; i < rate * 3; ++i)
    {
        // A full-scale resonant note exposes limiter-induced distortion, not
        // just whether the output was clamped to a legal numeric range.
        const float sample = static_cast<float>(0.09 * std::sin(freeze_test::tau * 1000 * i / rate));
        quiet.samples[2 * i] = quiet.samples[2 * i + 1] = sample;
        loud.samples[2 * i] = loud.samples[2 * i + 1] = sample * 10;
    }
    for (int kind = 0; kind < 4; ++kind)
    {
        VoiceSettings settings; settings.fadeOut = 0;
        settings.delayTimeMs = 50; settings.delayFeedback = 0.8f;
        settings.delayMix = kind == 0 || kind == 2 ? 1.0f : 0.0f;
        settings.reverbDecay = 2; settings.reverbMix = kind == 1 || kind == 2 ? 1.0f : 0.0f;
        settings.pingPongMix = kind == 3 ? 1.0f : 0.0f;
        settings.pingPongFeedback = 0.8f; settings.pingPongTimeMs = 50;
        const auto low = render(*core, quiet, settings, rate * 3);
        const auto high = render(*core, loud, settings, rate * 3);
        float error = 0, peak = 0;
        for (std::size_t i = 0; i < high.size(); ++i)
        {
            error = std::max(error, std::abs(high[i] - 10 * low[i]));
            peak = std::max(peak, std::abs(high[i]));
        }
        std::cout << "effect=" << kind << " linearityError=" << error << " peak=" << peak << '\n';
        require(error < 0.00002f && peak < 0.91f,
                "maximum wet/feedback effects preserve a loud waveform without forcing limiter distortion");
    }
    // A constant source makes a gain discontinuity at EOF unambiguous. The
    // release of the source must not mute and then restore a ringing wet tail.
    ClipData block; block.sampleRate = rate; block.samples.resize(rate * 2, 0.5f);
    VoiceSettings tail; tail.delayMix = 1; tail.delayTimeMs = 50;
    tail.delayFeedback = 0.8f; tail.reverbMix = 1; tail.fadeOut = 0.08;
    const auto faded = render(*core, block, tail, rate + 100);
    require(std::abs(faded[rate] - faded[rate - 1]) < 0.002f,
            "delay plus reverb tail remains continuous across the source fade and EOF");
    tail.reverbMix = 0; tail.fadeOut = 0;
    const auto decay = render(*core, block, tail, rate * 4);
    require(decay[rate * 2] > 0.001f,
            "high-feedback delay is not cut after the previous eight-repeat limit");
    const auto ended = render(*core, block, tail, rate * 5);
    require(core->activeVoices() == 0 && std::abs(ended.back()) < 0.0001f,
            "an extended delay tail still terminates and releases the voice");

    SpectralScrub scrub; scrub.prepare(rate);
    for (int kind : {0, 1, 2})
    {
        const auto pcm = freeze_test::source(rate, kind);
        scrub.start(pcm.data(), static_cast<int>(pcm.size()) / 2, rate, 0.5);
        std::vector<float> audio(rate * 2);
        for (auto& sample : audio) sample = scrub.next(0.5).first;
        const double flatness = freeze_test::flatness(audio, rate, rate);
        std::vector<float> original(pcm.size() / 2);
        for (std::size_t i = 0; i < original.size(); ++i) original[i] = pcm[2 * i];
        const double sourceFlatness = freeze_test::flatness(original, rate, rate / 2);
        std::cout << "freeze=" << kind << " noiseBandFlatness=" << flatness
                  << " sourceFlatness=" << sourceFlatness << '\n';
        require(flatness > sourceFlatness * 0.8,
                "held noise retains the source's broad spectrum instead of sparse ringing lines");
    }
    core->resetAfterDeviceStop();
    std::cout << "PASS: " << checks << " audio quality checks\n";
}
