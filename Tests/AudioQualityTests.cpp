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
                          const dualdeck::VoiceSettings& settings, int frames,
                          std::vector<float>* right = nullptr)
{
    core.stop(); core.render(nullptr, nullptr, 1);
    core.trigger(1, &clip, settings);
    std::vector<float> result(frames);
    if (right) right->resize(frames);
    for (int i = 0; i < frames; i += 128)
        core.render(result.data() + i, right ? right->data() + i : nullptr, std::min(128, frames - i));
    return result;
}
}

int main()
{
    using namespace dualdeck;
    constexpr int rate = 48000;
    auto core = std::make_unique<AudioCore>();
    core->setVolume(1); core->prepare(rate);
    // A short stereo hit must sound once at its original level; even at
    // maximum Mix, the delayed copy must already be quieter. Feedback zero
    // still gives one echo, rather than a second full-volume hit.
    struct DelayCase { float feedback, mix, first, second; };
    const DelayCase delayCases[] {{0, 1, 0.4f, 0}, {0.35f, 1, 0.26f, 0.091f},
                                  {0.8f, 1, 0.08f, 0.064f}, {0, 0.5f, 0.2f, 0},
                                  {0.35f, 0, 0, 0}};
    for (int outputRate : {44100, 48000, 96000})
    {
        core->prepare(outputRate);
        ClipData hit; hit.sampleRate = outputRate; hit.samples.resize(32, 0.0f);
        hit.samples[0] = 0.8f; hit.samples[1] = -0.4f;
        const int echoFrame = outputRate / 10;
        for (const auto& test : delayCases)
        {
            VoiceSettings settings; settings.fadeIn = settings.fadeOut = 0;
            settings.delayTimeMs = 100; settings.delayFeedback = test.feedback; settings.delayMix = test.mix;
            std::vector<float> right;
            const auto output = render(*core, hit, settings, 2 * echoFrame + 1, &right);
            std::cout << "delay rate=" << outputRate << " feedback=" << test.feedback
                      << " mix=" << test.mix << " dry=" << output[0]
                      << " first=" << output[echoFrame] << " second=" << output[2 * echoFrame] << '\n';
            require(std::abs(output[0] - 0.8f) < 0.000001f && std::abs(right[0] + 0.4f) < 0.000001f,
                    "delay preserves the original stereo hit at full level");
            require(std::abs(output[echoFrame] - test.first) < 0.000001f
                    && std::abs(right[echoFrame] + test.first * 0.5f) < 0.000001f,
                    "the first delay echo is attenuated even at maximum Mix and zero Feedback");
            require(std::abs(output[2 * echoFrame] - test.second) < 0.000001f
                    && std::abs(right[2 * echoFrame] + test.second * 0.5f) < 0.000001f,
                    "delay Feedback sets the decay after the quieter first echo");
            bool silentBetweenEchoes = true;
            for (int i = 1; i < 2 * echoFrame; ++i)
                if (i != echoFrame) silentBetweenEchoes &= output[i] == 0 && right[i] == 0;
            require(silentBetweenEchoes, "delay produces no extra attacks before or between scheduled echoes");
        }
    }
    core->prepare(rate);
    // The first dry attack precedes every delay/reverb reflection. Raising
    // their mix must not reduce it, even with multiple effects enabled.
    ClipData attack; attack.sampleRate = rate; attack.samples.resize(rate * 2, 0.0f);
    attack.samples[0] = 0.8f; attack.samples[1] = -0.4f;
    bool attacksPreserved = true;
    for (int kind = 0; kind < 4; ++kind)
    {
        VoiceSettings settings; settings.fadeIn = settings.fadeOut = 0;
        settings.delayMix = kind == 0 || kind == 2 ? 1.0f : 0.0f;
        settings.reverbMix = kind == 1 || kind == 2 ? 1.0f : 0.0f;
        settings.pingPongMix = kind == 3 ? 1.0f : 0.0f;
        std::vector<float> right;
        const auto output = render(*core, attack, settings, 128, &right);
        std::cout << "initialAttack effect=" << kind << " expected=0.8 actual=" << output[0] << '\n';
        attacksPreserved &= std::abs(output[0] - 0.8f) < 0.000001f && std::abs(right[0] + 0.4f) < 0.000001f;
    }
    require(attacksPreserved, "maximum effect mix preserves the first dry attack at its original level");
    ClipData quiet, loud;
    quiet.sampleRate = loud.sampleRate = rate;
    quiet.samples.resize(rate * 6); loud.samples.resize(rate * 6);
    for (int i = 0; i < rate * 3; ++i)
    {
        // Below the limiter threshold, wet output must remain proportional.
        // Above it, limiting the sum is intentional, not a linearity failure.
        const float sample = static_cast<float>(0.009 * std::sin(freeze_test::tau * 1000 * i / rate));
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
                "maximum wet/feedback effects remain linear below the limiter threshold");
        require(peak > 0.12f, "maximum wet mix adds audible repetitions instead of normalising them away");
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

    // A limiter that recovers between each pair of bass peaks modulates the
    // gain within the waveform and adds harmonics even after settling. Exercise
    // the real final mixer (including microphone input), with no mocked DSP.
    core->resetAfterDeviceStop();
    for (int outputRate : {44100, 48000, 96000, 192000})
    {
        core->prepare(outputRate);
        std::vector<float> left(outputRate), right(outputRate), micL(outputRate), micR(outputRate);
        for (int i = 0; i < outputRate; ++i)
        {
            micL[i] = static_cast<float>(2 * std::sin(freeze_test::tau * 50 * i / outputRate));
            micR[i] = -0.7f * micL[i];
        }
        for (int i = 0; i < outputRate; i += 128)
            core->render(left.data() + i, right.data() + i, std::min(128, outputRate - i),
                         micL.data() + i, micR.data() + i, 1);
        double sine = 0, cosine = 0, power = 0, residual = 0;
        float peak = 0, stereoError = 0;
        bool finite = true;
        const int first = outputRate / 2, length = outputRate - first;
        for (int i = 0; i < outputRate; ++i)
        {
            finite &= std::isfinite(left[i]) && std::isfinite(right[i]);
            peak = std::max(peak, std::max(std::abs(left[i]), std::abs(right[i])));
            stereoError = std::max(stereoError, std::abs(right[i] + 0.7f * left[i]));
            if (i < first) continue;
            sine += left[i] * std::sin(freeze_test::tau * 50 * i / outputRate);
            cosine += left[i] * std::cos(freeze_test::tau * 50 * i / outputRate);
            power += left[i] * left[i];
        }
        sine *= 2.0 / length; cosine *= 2.0 / length;
        for (int i = first; i < outputRate; ++i)
        {
            const double predicted = sine * std::sin(freeze_test::tau * 50 * i / outputRate)
                                   + cosine * std::cos(freeze_test::tau * 50 * i / outputRate);
            residual += (left[i] - predicted) * (left[i] - predicted);
        }
        const double distortion = std::sqrt(residual / power);
        std::cout << "limiter rate=" << outputRate << " peak=" << peak
                  << " fundamentalResidual=" << distortion << " stereoError=" << stereoError << '\n';
        require(finite && peak <= 0.98001f && stereoError < 0.000001f,
                "overload protection limits stereo peaks with one shared gain");
        require(distortion < 0.001,
                "a settled bass overload is not distorted by gain recovery between consecutive peaks");
        std::fill(micL.begin(), micL.end(), 0.1f); std::fill(micR.begin(), micR.end(), -0.07f);
        for (int i = 0; i < outputRate; i += 128)
            core->render(left.data() + i, right.data() + i, std::min(128, outputRate - i),
                         micL.data() + i, micR.data() + i, 1);
        std::cout << "limiter recovery rate=" << outputRate << " expected=0.1 actual=" << left.back() << '\n';
        require(std::abs(left.back() - 0.1f) < 0.00002f && std::abs(right.back() + 0.07f) < 0.00002f,
                "limiter gradually restores a later quiet signal to its original level");
    }

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
