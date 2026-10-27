#include "AudioCore.h"
#include "PadPressState.h"
#include "FreezeTextureMetrics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>

using namespace dualdeck;
int passed = 0;
void require(bool value, const char* message)
{
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    ++passed;
}
ClipData constant(int frames, float amplitude)
{
    ClipData clip; clip.sampleRate = 1000; clip.samples.resize(frames * 2, amplitude); return clip;
}
ClipData tone(int frames, double frequency)
{
    ClipData clip; clip.sampleRate = 48000; clip.samples.resize(frames * 2);
    for (int i = 0; i < frames; ++i)
    {
        const auto sample = static_cast<float>(0.2 * std::sin(6.283185307179586 * frequency * i / clip.sampleRate));
        clip.samples[i * 2] = clip.samples[i * 2 + 1] = sample;
    }
    return clip;
}
double rms(const float* samples, int begin, int end)
{
    double sum = 0;
    for (int i = begin; i < end; ++i) sum += samples[i] * samples[i];
    return std::sqrt(sum / (end - begin));
}
double scrubHarmonicEnergy(const std::vector<float>& audio, double rate, int begin,
                           std::initializer_list<double> frequencies)
{
    const int length = static_cast<int>(audio.size()) - begin;
    std::vector<double> windowed(length);
    double power = 0.0, harmonicPower = 0.0;
    for (int i = 0; i < length; ++i)
    {
        windowed[i] = audio[begin + i] * (0.5 - 0.5 * std::cos(6.283185307179586 * i / length));
        power += windowed[i] * windowed[i];
    }
    for (const double frequency : frequencies)
    {
        const int center = static_cast<int>(std::round(frequency * length / rate));
        for (int bin = center - 3; bin <= center + 3; ++bin)
        {
            double real = 0.0, imaginary = 0.0;
            for (int i = 0; i < length; ++i)
            {
                const double phase = 6.283185307179586 * bin * i / length;
                real += windowed[i] * std::cos(phase);
                imaginary += windowed[i] * std::sin(phase);
            }
            harmonicPower += 2.0 * (real * real + imaginary * imaginary) / length;
        }
    }
    return power > 1.0e-12 ? harmonicPower / power : 0.0;
}
int main()
{
    PadPressState presses;
    presses.press(7, PlayMode::holdLoop);
    presses.press(7, PlayMode::overlap);
    require(presses.isHeld(7), "hold press stays tracked while the clip loads");
    require(presses.releaseNeedsStop(7), "release stops the original hold loop after a mode change");
    require(!presses.isHeld(7) && !presses.releaseNeedsStop(7), "release clears the original press");
    presses.press(8, PlayMode::overlap);
    require(!presses.releaseNeedsStop(8), "release does not stop a non-loop press");
    presses.press(9, PlayMode::holdLoop);
    presses.clear();
    require(!presses.isHeld(9), "stop all clears pending held loops");
    float left[1024] {}, right[1024] {};
    auto clip = constant(100, 0.2f);
    VoiceSettings settings; settings.gain = 1; settings.fadeOut = 0;
    AudioCore core; core.setVolume(1); core.prepare(1000);
    core.trigger(1, &clip, settings); core.render(left, right, 20);
    require(std::abs(left[0] - 0.2f) < 0.0001f && left[0] == right[0], "stereo and gain");
    require(clip.activeReferences.load() == 1, "active voice pins its clip");
    core.trigger(1, &clip, settings); core.render(left, right, 20);
    require(core.activeVoices() == 1, "restart replaces previous voice");
    settings.mode = PlayMode::overlap;
    core.trigger(1, &clip, settings); core.render(left, right, 1);
    require(core.activeVoices() == 2 && left[0] > 0.2f && left[0] < 0.4f,
            "spam keeps both voices while balancing their combined level");
    core.stop(); core.render(left, right, 1);
    require(core.activeVoices() == 0 && left[0] == 0, "panic stop");
    require(clip.activeReferences.load() == 0, "stop releases clip pin in audio callback");
    core.trigger(2, &clip, settings); core.stop(); core.render(left, right, 1);
    require(core.activeVoices() == 0, "panic discards queued starts");
    settings.mode = PlayMode::toggleLoop;
    core.trigger(2, &clip, settings); core.render(left, right, 300);
    require(core.activeVoices() == 1 && left[299] > 0, "loop wraps");
    core.trigger(2, &clip, settings); core.render(left, right, 1);
    require(core.activeVoices() == 0, "toggle loop turns off");
    settings.mode = PlayMode::holdLoop; settings.fadeOut = 0.01;
    core.trigger(3, &clip, settings); core.render(left, right, 1); core.release(3); core.render(left, right, 12);
    require(left[0] > left[8] && left[11] == 0 && core.activeVoices() == 0, "hold release fades and stops");
    settings.fadeOut = 0; settings.mode = PlayMode::restart;
    auto ramp = constant(100, 0);
    for (int i = 0; i < 100; ++i) ramp.samples[i * 2] = ramp.samples[i * 2 + 1] = i / 1000.0f;
    core.trigger(4, &ramp, settings); core.render(left, right, 10); core.pause(true); core.render(left, right, 500);
    require(left[0] == 0 && left[499] == 0, "DJ pause emits silence");
    core.pause(false); core.render(left, right, 1);
    require(std::abs(left[0] - 0.01f) < 0.00001f, "DJ resume keeps exact position");
    core.stop(); settings.trimStart = 0.02; settings.trimEnd = 0.025;
    core.trigger(5, &ramp, settings); core.render(left, right, 7);
    require(std::abs(left[0] - 0.02f) < 0.00001f && left[5] == 0, "trim bounds");
    core.stop(); settings.mode = PlayMode::holdLoop; settings.trimStart = 0.02; settings.trimEnd = 0.025;
    core.trigger(999, &ramp, settings); core.render(left, right, 8);
    require(std::abs(left[5] - 0.02f) < 0.00001f && std::abs(left[7] - 0.022f) < 0.00001f,
            "A-B preview repeats only the selected segment");
    core.release(999); core.render(left, right, 1);
    require(core.activeVoices() == 0, "A-B preview stops independently");
    core.stop(); settings.mode = PlayMode::restart; settings.trimStart = 0.04; settings.trimEnd = 0.05;
    core.trigger(998, &ramp, settings); core.render(left, right, 1);
    require(std::abs(left[0] - 0.04f) < 0.00001f, "scrub grain starts at selected waveform position");
    core.release(998); core.render(left, right, 1);
    require(core.activeVoices() == 0, "closing editor stops a scrub grain");
    AudioCore editorPlayback; editorPlayback.setVolume(1); editorPlayback.prepare(1000);
    auto backgroundPad = constant(100, 0.1f);
    VoiceSettings backgroundSettings; backgroundSettings.fadeOut = 0;
    VoiceSettings editorSettings; editorSettings.fadeOut = 0; editorSettings.trimStart = 0.02;
    editorPlayback.trigger(10, &backgroundPad, backgroundSettings);
    editorPlayback.trackVoice(997);
    editorPlayback.trigger(997, &ramp, editorSettings);
    editorPlayback.render(left, right, 1);
    require(std::abs(left[0] - 0.12f) < 0.00001f, "editor playback mixes with another pad");
    require(std::abs(editorPlayback.trackedPositionSeconds(997) - 0.021) < 0.00001,
            "playhead follows the rendered editor voice");
    editorPlayback.release(997); editorPlayback.render(left, right, 1);
    require(std::abs(left[0] - 0.1f) < 0.00001f && editorPlayback.activeVoices() == 1,
            "editor seek leaves other pads playing");
    require(editorPlayback.trackedPositionSeconds(997) < 0,
            "playhead reports when the editor voice has stopped");
    editorSettings.trimStart = 0.04;
    editorPlayback.trigger(997, &ramp, editorSettings); editorPlayback.render(left, right, 1);
    require(std::abs(left[0] - 0.14f) < 0.00001f,
            "editor playback resumes at scrubbed position");
    AudioCore stationaryScrub; stationaryScrub.setVolume(1); stationaryScrub.prepare(1000);
    VoiceSettings scrubSettings; scrubSettings.mode = PlayMode::scrub;
    scrubSettings.gain = 1; scrubSettings.fadeIn = 0; scrubSettings.fadeOut = 0.01;
    auto scrubClip = constant(200, 0);
    for (int i = 0; i < 200; ++i)
        scrubClip.samples[i * 2] = scrubClip.samples[i * 2 + 1] = i / 1000.0f;
    stationaryScrub.setScrubTarget(0.08);
    stationaryScrub.trackVoice(998);
    stationaryScrub.trigger(998, &scrubClip, scrubSettings);
    stationaryScrub.render(left, right, 500);
    require(stationaryScrub.activeVoices() == 1 && scrubClip.activeReferences.load() == 1,
            "a held scrub keeps sounding without mouse movement");
    stationaryScrub.setScrubTarget(0.16);
    stationaryScrub.render(left, right, 600);
    require(stationaryScrub.activeVoices() == 1 && scrubClip.activeReferences.load() == 1
            && std::abs(stationaryScrub.trackedPositionSeconds(998) - 0.16) < 0.001,
            "moving the scrub target forward keeps one continuous voice");
    stationaryScrub.setScrubTarget(0.04);
    stationaryScrub.render(left, right, 600);
    require(stationaryScrub.activeVoices() == 1
            && std::abs(stationaryScrub.trackedPositionSeconds(998) - 0.04) < 0.001,
            "moving the scrub target backward follows the selected audio");
    stationaryScrub.release(998); stationaryScrub.render(left, right, 20);
    require(stationaryScrub.activeVoices() == 0, "releasing a stationary scrub stops its voice");
    ClipData heldNotes;
    heldNotes.sampleRate = 48000;
    heldNotes.samples.resize(96000 * 2);
    for (int frame = 0; frame < 96000; ++frame)
    {
        const double frequency = frame < 48000 ? 233.0 : 661.0;
        const float sample = static_cast<float>(0.2 * std::sin(6.283185307179586 * frequency * frame / 48000.0));
        heldNotes.samples[frame * 2] = heldNotes.samples[frame * 2 + 1] = sample;
    }
    SpectralScrub freeze;
    freeze.prepare(48000);
    freeze.start(heldNotes.samples.data(), heldNotes.frames(), heldNotes.sampleRate, 0.5);
    std::vector<float> held(24576), moved(24576);
    for (auto& sample : held) sample = freeze.next(0.5).first;
    for (auto& sample : moved) sample = freeze.next(1.5).first;
    auto risingCrossings = [](const std::vector<float>& audio)
    {
        int crossings = 0;
        for (int i = 16385; i < 24576; ++i)
            if (audio[i - 1] <= 0 && audio[i] > 0) ++crossings;
        return crossings;
    };
    require(rms(held.data(), 16384, 24576) > 0.04 && risingCrossings(held) > 34
            && risingCrossings(held) < 46, "zero-speed scrub sustains the note at its original pitch");
    require(risingCrossings(moved) > 105 && risingCrossings(moved) < 122,
            "spectral scrub changes note when the target moves");
    ClipData movingDetail;
    movingDetail.sampleRate = 48000;
    movingDetail.samples.resize(48000 * 2);
    for (int frame = 0; frame < 48000; ++frame)
    {
        const double time = static_cast<double>(frame) / 48000.0;
        const float sample = static_cast<float>(0.12 * std::sin(6.283185307179586 * 220.0 * time)
            + 0.09 * std::sin(6.283185307179586 * 913.0 * time)
            + 0.05 * std::sin(6.283185307179586 * 1761.0 * time));
        movingDetail.samples[frame * 2] = movingDetail.samples[frame * 2 + 1] = sample;
    }
    for (const double movementSpeed : {1.0, 0.5, -0.5, 0.02, -1.0})
    {
        const double startSeconds = movementSpeed < 0 ? 0.8 : 0.1;
        freeze.start(movingDetail.samples.data(), movingDetail.frames(), movingDetail.sampleRate, startSeconds);
        std::vector<float> moving(24000);
        for (int frame = 0; frame < 24000; ++frame)
            moving[frame] = freeze.next(startSeconds
                + movementSpeed * static_cast<double>(frame) / 48000.0).first;
        // Test the audible result, including overlaps: the old test skipped
        // every splice and required precisely the short grains causing buzz.
        require(rms(moving.data(), 4800, 24000) > 0.04
                && scrubHarmonicEnergy(moving, 48000, 4800, {220.0, 913.0, 1761.0}) > 0.97,
                "forward, slow and reverse scrubbing preserve harmonic energy including grain overlaps");
    }
    for (const int updateFrames : {800, 1600})
    {
        freeze.start(movingDetail.samples.data(), movingDetail.frames(), movingDetail.sampleRate, 0.8);
        std::vector<float> stepped(24000);
        for (int i = 0; i < 24000; ++i)
            stepped[i] = freeze.next(0.8 - 0.5 * ((i / updateFrames) * updateFrames) / 48000.0).first;
        require(scrubHarmonicEnergy(stepped, 48000, 4800, {220.0, 913.0, 1761.0}) > 0.97,
                "30 and 60 Hz mouse updates retain pitch without a train of grain artifacts");
    }
    auto smoothTone = tone(96000, 233.0);
    freeze.start(smoothTone.samples.data(), smoothTone.frames(), smoothTone.sampleRate, 0.8);
    float previousSmooth = 0.0f, greatestSmoothStep = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const int update = (i / 800) * 800;
        const double target = update < 16000 ? 0.8 - 0.5 * update / 48000.0
            : update < 32000 ? 0.633333333333 + 0.5 * (update - 16000) / 48000.0 : 0.8;
        const float sample = freeze.next(target).first;
        if (i > 4800) greatestSmoothStep = std::max(greatestSmoothStep, std::abs(sample - previousSmooth));
        previousSmooth = sample;
    }
    require(greatestSmoothStep < 0.0075f,
            "phantom overlaps, direction changes and settling add no sharp steps to a sustained tone");
    for (int i = 0; i < smoothTone.frames(); ++i) smoothTone.samples[i * 2 + 1] *= -0.7f;
    freeze.start(smoothTone.samples.data(), smoothTone.frames(), smoothTone.sampleRate, 0.8);
    float stereoError = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const auto sample = freeze.next(i < 16000 ? 0.8 - 0.5 * ((i / 800) * 800) / 48000.0 : 0.63);
        stereoError = std::max(stereoError, std::abs(sample.second + 0.7f * sample.first));
    }
    require(stereoError < 0.00002f,
            "motion and zero-speed synthesis preserve stereo level and opposite-phase relationships");
    freeze.start(movingDetail.samples.data(), movingDetail.frames(), movingDetail.sampleRate, 0.5);
    std::vector<float> frozenChord(48000);
    for (auto& sample : frozenChord) sample = freeze.next(0.5).first;
    require(scrubHarmonicEnergy(frozenChord, 48000, 12000, {220.0, 913.0, 1761.0}) > 0.99,
            "phase-locked zero-speed chords retain their harmonics");
    // A frozen stochastic spectrum must remain noise-like rather than turning
    // each random spectral maximum into a permanent whistle. Include a tonal
    // component and different source/output rates, plus a long held cursor.
    for (int outputRate : {44100, 48000, 96000, 192000})
    {
        freeze.prepare(outputRate);
        for (int kind : {0, 1, 2})
        {
            const auto textured = freeze_test::source(48000, kind);
            freeze.start(textured.data(), static_cast<int>(textured.size()) / 2, 48000, 0.5);
            std::vector<float> sound(outputRate * 2);
            float error = 0.0f, peak = 0.0f;
            for (auto& sample : sound)
            {
                const auto stereo = freeze.next(0.5);
                sample = stereo.first;
                error = std::max(error, std::abs(stereo.second + 0.7f * stereo.first));
                peak = std::max(peak, std::abs(sample));
            }
            require(freeze_test::flatness(sound, outputRate, outputRate) > 0.18,
                    "held broadband, filtered and mixed noise retain spectral texture instead of narrow whistles");
            const double level = rms(sound.data(), outputRate, outputRate * 2);
            require(level > 0.06 && level < 0.22 && peak < 0.8f,
                    "a long textured freeze retains bounded level without fading or growing");
            require(error < 0.00003f,
                    "randomised held phantom preserves opposite-phase stereo");
            if (kind == 2)
                require(scrubHarmonicEnergy(sound, outputRate, outputRate, {233.0}) > 0.20,
                        "a held note embedded in noise keeps a coherent pitched component");
        }
    }
    freeze.prepare(48000);
    auto separateStereo = tone(96000, 233.0);
    for (int i = 0; i < separateStereo.frames(); ++i)
        separateStereo.samples[i * 2 + 1] = static_cast<float>(0.2 * std::sin(6.283185307179586 * 661.0 * i / 48000.0));
    freeze.start(separateStereo.samples.data(), separateStereo.frames(), separateStereo.sampleRate, 0.5);
    std::vector<float> heldLeft(24000), heldRight(24000);
    for (int i = 0; i < 24000; ++i)
    {
        const auto sample = freeze.next(0.5);
        heldLeft[i] = sample.first; heldRight[i] = sample.second;
    }
    require(scrubHarmonicEnergy(heldLeft, 48000, 4800, {233.0}) > 0.99
            && scrubHarmonicEnergy(heldRight, 48000, 4800, {661.0}) > 0.99,
            "shared stereo oscillators retain different notes panned fully left and right");
    double correlation = 0, firstPower = 0, secondPower = 0;
    for (int i = 10000; i < 16000; ++i)
    {
        correlation += held[i] * held[i + 4800];
        firstPower += held[i] * held[i];
        secondPower += held[i + 4800] * held[i + 4800];
    }
    require(correlation / std::sqrt(firstPower * secondPower) < 0.5,
            "held audio is synthesized continuously instead of repeating a short waveform");
    freeze.start(heldNotes.samples.data(), heldNotes.frames(), heldNotes.sampleRate, 0.25);
    std::vector<float> elasticDrag(24576);
    for (int i = 0; i < static_cast<int>(elasticDrag.size()); ++i)
    {
        const double fraction = i < 12288 ? static_cast<double>(i) / 12288.0
                                          : static_cast<double>(24576 - i) / 12288.0;
        elasticDrag[i] = freeze.next(0.25 + 0.5 * fraction).first;
    }
    double quietestDragBlock = 1.0;
    for (int offset = 2048; offset < static_cast<int>(elasticDrag.size()); offset += 512)
        quietestDragBlock = std::min(quietestDragBlock, rms(elasticDrag.data(), offset, offset + 512));
    require(quietestDragBlock > 0.02,
            "continuous forward and reverse scrub keeps a sustained sound audible");
    auto rateProbe = tone(48000 * 3, 1000.0);
    for (const int outputRate : {44100, 96000})
    {
        freeze.prepare(outputRate);
        freeze.start(rateProbe.samples.data(), rateProbe.frames(), rateProbe.sampleRate, 1.5);
        for (int i = 0; i < outputRate / 4; ++i) freeze.next(1.5);
        int crossings = 0;
        float previous = 0.0f;
        for (int i = 0; i < outputRate / 4; ++i)
        {
            const float sample = freeze.next(1.5).first;
            if (previous <= 0.0f && sample > 0.0f) ++crossings;
            previous = sample;
        }
        require(std::abs(crossings - 250) <= 2,
                "zero-speed scrub preserves pitch across clip and output sample rates");
    }
    freeze.prepare(48000);
    freeze.start(heldNotes.samples.data(), heldNotes.frames(), heldNotes.sampleRate, 0.25);
    int jumpedCrossings = 0;
    float previousJumpSample = 0.0f;
    for (int i = 0; i < 8192; ++i)
    {
        const float sample = freeze.next(1.75).first;
        if (i >= 4096 && previousJumpSample <= 0.0f && sample > 0.0f) ++jumpedCrossings;
        previousJumpSample = sample;
    }
    require(jumpedCrossings >= 50 && jumpedCrossings <= 62,
            "a distant scrub jump reaches the selected note promptly");
    ClipData impacts = constant(48000, 0.0f);
    impacts.sampleRate = 48000;
    for (const int onset : {24000, 33600})
        for (int offset = 0; offset < 2000; ++offset)
        {
            const float sample = static_cast<float>(0.6 * std::exp(-offset / 600.0)
                * (offset % 2 == 0 ? 1.0 : -1.0));
            impacts.samples[(onset + offset) * 2] = impacts.samples[(onset + offset) * 2 + 1] = sample;
        }
    freeze.start(impacts.samples.data(), impacts.frames(), impacts.sampleRate, 0.5);
    float attackError = 0.0f;
    for (int offset = 0; offset < 2048; ++offset)
        attackError = std::max(attackError,
            std::abs(freeze.next(0.5).first - impacts.samples[(24000 - 144 + offset) * 2]));
    require(attackError < 0.00001f, "scrub plays the original transient on mouse-down");
    for (int offset = 2048; offset < 5120; ++offset) freeze.next(0.5);
    float movedAttackError = 0.0f;
    for (int offset = 0; offset < 400; ++offset)
    {
        const auto sample = freeze.next(0.7).first;
        if (offset >= 96)
            movedAttackError = std::max(movedAttackError,
                std::abs(sample - impacts.samples[(33600 - 144 + offset) * 2]));
    }
    require(movedAttackError < 0.00001f,
            "moving onto a hit preserves its original attack and playback speed");
    // Freeze at the actual file boundaries, not just well inside a tone.
    // Include high-rate outputs: a fixed-size FFT loses bass resolution there.
    for (const int outputRate : {44100, 48000, 96000, 192000})
    {
        freeze.prepare(outputRate);
        for (const double frequency : {55.0, 233.0})
        {
            auto edgeTone = tone(96000, frequency);
            for (const double position : {0.0, 2.0})
            {
                freeze.start(edgeTone.samples.data(), edgeTone.frames(), edgeTone.sampleRate, position);
                std::vector<float> edgeSound(outputRate / 2);
                for (auto& sample : edgeSound) sample = freeze.next(position).first;
                const double level = rms(edgeSound.data(), outputRate / 4, outputRate / 2);
                double mean = 0;
                for (int i = outputRate / 4; i < outputRate / 2; ++i) mean += edgeSound[i];
                mean /= outputRate / 4;
                require(level > 0.13 && level < 0.15 && std::abs(mean) < 0.005,
                        "freeze at either file edge retains level without a DC pedestal");
                require(scrubHarmonicEnergy(edgeSound, outputRate, outputRate / 4, {frequency}) > 0.98,
                        "freeze at either file edge retains bass and pitch across output rates");
            }
        }
    }
    freeze.prepare(48000);
    auto upcomingNote = tone(96000, 661.0);
    std::fill(upcomingNote.samples.begin(), upcomingNote.samples.begin() + 25440 * 2, 0.0f);
    freeze.start(upcomingNote.samples.data(), upcomingNote.frames(), upcomingNote.sampleRate, 0.5);
    std::vector<float> silenceHold(12000);
    for (auto& sample : silenceHold) sample = freeze.next(0.5).first;
    require(rms(silenceHold.data(), 0, 12000) < 0.000001,
            "holding silence does not preview the next note 30 ms beyond the cursor");
    // Stop a drag before the upcoming note. Sparse mouse updates must not let
    // the temporary PCM voice run freely through the next phrase.
    freeze.start(upcomingNote.samples.data(), upcomingNote.frames(), upcomingNote.sampleRate, 0.4);
    for (int i = 0; i < 4800; ++i) freeze.next(0.4 + i / 48000.0);
    for (auto& sample : silenceHold) sample = freeze.next(0.5).first;
    require(rms(silenceHold.data(), 0, 12000) < 0.000001,
            "stopping a drag in silence does not leak the following note");
    auto finePosition = tone(8000, 440.0);
    finePosition.sampleRate = 8000;
    for (int i = 0; i < 8000; ++i)
    {
        const float sample = i < 4000 ? 0.0f
            : static_cast<float>(0.2 * std::sin(6.283185307179586 * 440.0 * i / 8000));
        finePosition.samples[2 * i] = finePosition.samples[2 * i + 1] = sample;
    }
    freeze.start(finePosition.samples.data(), finePosition.frames(), finePosition.sampleRate, 0.49);
    for (auto& sample : silenceHold) sample = freeze.next(0.49).first;
    const double beforeFineMove = rms(silenceHold.data(), 6000, 12000);
    for (auto& sample : silenceHold) sample = freeze.next(0.4925).first;
    require(rms(silenceHold.data(), 6000, 12000) > beforeFineMove + 0.005,
            "a final move smaller than 24 source samples updates the frozen sound");
    auto settlingTone = tone(96000, 233.0);
    for (const double speed : {-1.0, -0.5, 0.02, 0.5, 1.0})
    {
        freeze.start(settlingTone.samples.data(), settlingTone.frames(), settlingTone.sampleRate, 1.0);
        std::vector<float> stopped(24000);
        for (int i = 0; i < 24000; ++i)
            stopped[i] = freeze.next(1.0 + speed * std::min((i / 800) * 800, 12000) / 48000.0).first;
        double quietest = 1.0;
        for (int i = 12000; i < 23000; i += 128)
            quietest = std::min(quietest, rms(stopped.data(), i, i + 960));
        require(quietest > 0.12,
                "settling from forward and reverse motion to freeze does not cancel the held note");
    }
    const float oneFrame[] {0.1f, -0.1f};
    freeze.start(oneFrame, 1, 48000.0, 0.0);
    bool finiteShortScrub = true;
    for (int i = 0; i < 4096; ++i)
    {
        const auto sample = freeze.next(100.0);
        finiteShortScrub &= std::isfinite(sample.first) && std::isfinite(sample.second);
    }
    require(finiteShortScrub, "spectral scrub handles a one-frame file and a target past its end");
    const float invalidFrame[] {std::numeric_limits<float>::quiet_NaN(), 1.0e30f};
    freeze.start(invalidFrame, 1, 48000.0, 0.0);
    bool finiteInvalidScrub = true;
    for (int i = 0; i < 2048; ++i)
    {
        const auto sample = freeze.next(std::numeric_limits<double>::quiet_NaN());
        finiteInvalidScrub &= std::isfinite(sample.first) && std::isfinite(sample.second);
    }
    require(finiteInvalidScrub, "invalid PCM and scrub coordinates cannot poison the audio callback");
    auto briefClip = constant(10, 0.1f);
    VoiceSettings briefSettings; briefSettings.fadeOut = 0;
    stationaryScrub.trackVoice(997);
    stationaryScrub.trigger(997, &briefClip, briefSettings);
    stationaryScrub.render(left, right, 50);
    require(stationaryScrub.trackedVoiceStarted() && stationaryScrub.trackedPositionSeconds(997) < 0,
            "short playback reports completion even between UI timer ticks");
    stationaryScrub.setScrubTarget(0.08);
    stationaryScrub.trackVoice(998);
    stationaryScrub.trigger(998, &scrubClip, scrubSettings);
    VoiceSettings competingPad; competingPad.mode = PlayMode::overlap; competingPad.fadeOut = 0;
    for (int id = 1; id <= 32; ++id) stationaryScrub.trigger(id, &scrubClip, competingPad);
    stationaryScrub.render(left, right, 1);
    require(stationaryScrub.activeVoices() == 32 && stationaryScrub.trackedPositionSeconds(998) >= 0,
            "pad voice stealing preserves an active editor scrub");
    stationaryScrub.stop(); stationaryScrub.render(left, right, 1);
    auto replacementClip = constant(200, 0.05f);
    stationaryScrub.trigger(998, &scrubClip, scrubSettings);
    stationaryScrub.render(left, right, 1);
    stationaryScrub.trigger(999, &replacementClip, scrubSettings);
    stationaryScrub.render(left, right, 1);
    require(stationaryScrub.activeVoices() == 1 && scrubClip.activeReferences.load() == 0
            && replacementClip.activeReferences.load() == 1,
            "a new scrub replaces the old clip and releases its cache pin");
    stationaryScrub.resetAfterDeviceStop();
    require(replacementClip.activeReferences.load() == 0,
            "closing an output releases the spectral scrub clip");
    settings.trimStart = 0.05; settings.trimEnd = 0.02;
    core.trigger(5, &ramp, settings); core.render(left, right, 1);
    require(left[0] == 0, "invalid trim is silent");
    core.stop(); settings.trimStart = settings.trimEnd = 0; settings.speed = 2;
    core.trigger(6, &ramp, settings); core.render(left, right, 3);
    require(std::abs(left[2] - 0.004f) < 0.00001f, "speed changes sample step");
    core.stop(); core.prepare(2000); settings.speed = 1;
    core.trigger(6, &ramp, settings); core.render(left, right, 3);
    require(std::abs(left[2] - 0.001f) < 0.00001f, "device sample-rate conversion");
    core.stop(); core.prepare(1000); settings.mode = PlayMode::overlap;
    for (int i = 0; i < 50; ++i) require(core.trigger(i, &clip, settings), "voice command accepted");
    core.render(left, right, 4);
    require(core.activeVoices() == 32, "bounded voice stealing");
    for (int i = 0; i < 4; ++i) require(std::isfinite(left[i]) && std::abs(left[i]) <= 0.98001f && left[i] == right[i], "stereo peak protection");
    AudioCore headphone; headphone.setVolume(0); headphone.prepare(1000); headphone.trigger(1, &clip, settings); headphone.render(left, right, 4);
    require(left[0] == 0, "independent muted output");
    // Master values are absolute levels, not multipliers of the stored pad gain.
    auto longClip = constant(20000, 0.2f);
    AudioCore balancedSpam;
    balancedSpam.setVolume(1); balancedSpam.prepare(1000);
    VoiceSettings spamSettings; spamSettings.mode = PlayMode::overlap; spamSettings.fadeOut = 0;
    balancedSpam.trigger(1, &longClip, spamSettings);
    balancedSpam.trigger(1, &longClip, spamSettings);
    balancedSpam.render(left, right, 300);
    require(balancedSpam.activeVoices() == 2 && std::abs(left[299] - 0.2f) < 0.01f,
            "same-pad spam keeps both copies near one copy's level");
    balancedSpam.trigger(1, &longClip, spamSettings);
    balancedSpam.render(left, right, 300);
    require(balancedSpam.activeVoices() == 3 && std::abs(left[299] - 0.2f) < 0.01f,
            "repeated spam retains every copy without accumulating loudness");
    auto separatePad = constant(20000, 0.1f);
    balancedSpam.trigger(2, &separatePad, spamSettings);
    balancedSpam.render(left, right, 300);
    require(std::abs(left[299] - 0.3f) < 0.01f,
            "balancing one pad leaves another pad's level independent");
    balancedSpam.stop(); balancedSpam.render(left, right, 1);
    balancedSpam.trigger(1, &longClip, spamSettings);
    balancedSpam.render(left, right, 300);
    require(balancedSpam.activeVoices() == 1 && std::abs(left[299] - 0.2f) < 0.0001f,
            "stop resets same-pad balancing before the next playback");
    AudioCore cancellingSpam;
    cancellingSpam.setVolume(1); cancellingSpam.prepare(1000);
    auto positive = constant(100, 0.2f), negative = constant(50, -0.2f);
    cancellingSpam.trigger(1, &positive, spamSettings);
    cancellingSpam.trigger(1, &negative, spamSettings);
    cancellingSpam.render(left, right, 51);
    require(std::abs(left[49]) < 0.0001f && std::abs(left[50] - 0.2f) < 0.0001f,
            "phase cancellation cannot be recovered, and ending a copy does not boost the survivor");
    ClipData bass;
    bass.sampleRate = 1000;
    bass.samples.resize(2100 * 2);
    for (int frame = 0; frame < 2100; ++frame)
    {
        const float sample = static_cast<float>(1.2 * std::exp(-frame / 1400.0)
            * std::sin(6.283185307179586 * 33.0 * frame / 1000.0));
        bass.samples[frame * 2] = bass.samples[frame * 2 + 1] = sample;
    }
    AudioCore bassSpam;
    bassSpam.setVolume(0.5f); bassSpam.prepare(1000);
    const std::array<int, 7> bassPresses {0, 430, 910, 1380, 1820, 2270, 2730};
    std::array<double, 40> bassWindowPower {};
    int nextPress = 0;
    for (int frame = 0; frame < 4000; ++frame)
    {
        if (nextPress < static_cast<int>(bassPresses.size()) && frame == bassPresses[static_cast<std::size_t>(nextPress)])
        {
            require(bassSpam.trigger(1, &bass, spamSettings), "bass spam command accepted");
            ++nextPress;
        }
        bassSpam.render(left, right, 1);
        bassWindowPower[static_cast<std::size_t>(frame / 100)] += left[0] * left[0] / 100.0;
    }
    double bassMean = 0, bassVariance = 0;
    for (auto& power : bassWindowPower) { power = std::sqrt(power); bassMean += power / bassWindowPower.size(); }
    for (const auto level : bassWindowPower)
        bassVariance += (level - bassMean) * (level - bassMean) / bassWindowPower.size();
    require(std::sqrt(bassVariance) < 0.12 && bassSpam.activeVoices() > 0,
            "phase-shifted bass retriggers retain all voices with bounded loudness variation");
    VoiceSettings level; level.gain = 0.8f; level.fadeOut = 0;
    AudioCore mic, phones;
    mic.setVolume(0.25f); phones.setVolume(0.5f);
    mic.prepare(1000); phones.prepare(1000);
    mic.trigger(1, &longClip, level); phones.trigger(1, &longClip, level);
    mic.render(left, right, 1);
    require(std::abs(left[0] - 0.05f) < 0.00001f, "microphone master replaces stored pad gain by default");
    phones.render(left, right, 1);
    require(std::abs(left[0] - 0.1f) < 0.00001f, "headphone master is independent");
    level.overrideMasterVolume = true;
    for (auto* endpoint : {&mic, &phones})
    {
        endpoint->stop(); endpoint->render(left, right, 1);
        endpoint->trigger(1, &longClip, level); endpoint->render(left, right, 1);
        require(std::abs(left[0] - 0.16f) < 0.00001f, "selected pad gain overrides either output master");
        endpoint->setVolume(0); endpoint->render(left, right, 200);
        require(std::abs(left[199] - 0.16f) < 0.00001f, "override is not multiplied by a zero master");
    }
    AudioCore individual; individual.setVolume(0.1f); individual.prepare(1000);
    level.mode = PlayMode::overlap;
    individual.trigger(1, &longClip, level);
    level.gain = 0.3f; individual.trigger(2, &longClip, level);
    individual.render(left, right, 1);
    require(std::abs(left[0] - 0.22f) < 0.00001f, "overlapping sounds retain distinct absolute pad gains");
    individual.stop(); level.gain = 0; individual.trigger(3, &longClip, level); individual.render(left, right, 1);
    require(left[0] == 0, "zero individual gain is silent");
    individual.stop(); individual.render(left, right, 1);
    level.overrideMasterVolume = false;
    individual.trigger(4, &longClip, level); individual.render(left, right, 1);
    require(std::abs(left[0] - 0.02f) < 0.00001f, "pad with override disabled uses master despite zero stored gain");
    individual.stop(); individual.render(left, right, 1);
    level.overrideMasterVolume = true; level.gain = 0.8f;
    individual.trigger(5, &longClip, level);
    level.overrideMasterVolume = false;
    individual.trigger(6, &longClip, level);
    individual.render(left, right, 1);
    require(std::abs(left[0] - 0.18f) < 0.00001f, "two simultaneous pads choose override independently");
    AudioCore pitchCore; pitchCore.setVolume(1); pitchCore.setPitch(12); pitchCore.prepare(1000);
    VoiceSettings pitchSettings; pitchSettings.fadeOut = 0;
    pitchCore.trigger(1, &ramp, pitchSettings); pitchCore.render(left, right, 3);
    require(std::abs(left[2] - 0.004f) < 0.00001f, "global pitch +12 semitones doubles sample step");
    pitchCore.stop(); pitchCore.setPitch(-12); pitchCore.prepare(1000);
    pitchCore.trigger(1, &ramp, pitchSettings); pitchCore.render(left, right, 3);
    require(std::abs(left[2] - 0.001f) < 0.00001f, "global pitch -12 semitones halves sample step");
    pitchCore.stop(); pitchCore.setPitch(0); pitchCore.prepare(1000);
    pitchSettings.speed = 1.5; pitchCore.trigger(1, &ramp, pitchSettings); pitchCore.render(left, right, 3);
    require(std::abs(left[2] - 0.003f) < 0.00001f, "neutral global pitch preserves pad speed");
    pitchCore.stop(); pitchCore.setPitch(12); pitchCore.prepare(1000);
    pitchCore.trigger(1, &ramp, pitchSettings); pitchCore.render(left, right, 3);
    require(std::abs(left[2] - 0.006f) < 0.00001f, "global pitch combines with pad speed");
    auto longRamp = constant(5000, 0);
    for (int i = 0; i < 5000; ++i) longRamp.samples[i * 2] = longRamp.samples[i * 2 + 1] = i * 0.0001f;
    pitchCore.stop(); pitchCore.setPitch(0); pitchCore.prepare(1000); pitchSettings.speed = 1;
    pitchCore.trigger(1, &longRamp, pitchSettings); pitchCore.render(left, right, 20);
    pitchCore.setPitch(12); pitchCore.render(left, right, 200);
    require(std::abs((left[199] - left[198]) - 0.0002f) < 0.000001f, "pitch updates already playing voices");
    pitchCore.setPitch(0); pitchCore.render(left, right, 200);
    require(std::abs((left[199] - left[198]) - 0.0001f) < 0.000001f, "disabling global pitch restores original step");
    AudioCore invalid; invalid.setVolume(1); invalid.prepare(1000);
    auto invalidClip = constant(10, std::numeric_limits<float>::infinity());
    invalid.trigger(1, &invalidClip, settings); invalid.render(left, right, 4);
    require(std::isfinite(left[0]) && std::isfinite(right[0]), "invalid samples do not escape to audio device");
    CommandQueue<8> queue;
    for (int i = 0; i < 7; ++i) require(queue.push({Command::start, static_cast<std::uint64_t>(i), 0, nullptr, {}}), "queue push");
    require(!queue.push({}), "queue full is reported");
    Command command;
    for (int i = 0; i < 7; ++i) require(queue.pop(command) && command.id == static_cast<std::uint64_t>(i), "queue FIFO");
    require(!queue.pop(command), "queue empty");
    CommandQueue<256> concurrent;
    std::atomic<bool> ordered {true};
    std::thread producer([&] { for (int i = 0; i < 100000; ++i) { Command c; c.id = i; while (!concurrent.push(c)) std::this_thread::yield(); } });
    std::thread consumer([&] { for (int i = 0; i < 100000; ++i) { Command c; while (!concurrent.pop(c)) std::this_thread::yield(); if (c.id != static_cast<std::uint64_t>(i)) ordered = false; } });
    producer.join(); consumer.join(); require(ordered, "concurrent command order over 100000 events");
    AudioCore pinnedQueue;
    const auto priorReferences = clip.activeReferences.load();
    for (int i = 0; i < 255; ++i) require(pinnedQueue.trigger(i, &clip, settings), "pinned command queue accepts capacity");
    require(!pinnedQueue.trigger(256, &clip, settings), "full pinned command queue rejects trigger");
    require(clip.activeReferences.load() == priorReferences + 255, "rejected queue entry releases its pin");
    pinnedQueue.stop(); pinnedQueue.render(nullptr, nullptr, 1);
    require(clip.activeReferences.load() == priorReferences, "stale queued commands release clip pins");
    AudioCore closedDevice;
    closedDevice.prepare(1000);
    closedDevice.trigger(1, &clip, settings);
    closedDevice.render(left, right, 1);
    closedDevice.trigger(2, &clip, settings);
    require(clip.activeReferences.load() == priorReferences + 2, "closed device has a voice and queued start");
    closedDevice.resetAfterDeviceStop();
    require(clip.activeReferences.load() == priorReferences && closedDevice.activeVoices() == 0,
            "closing a device releases active and queued clips without another callback");
    AudioCore effects;
    effects.setVolume(1); effects.prepare(1000);
    auto impulse = constant(4, 0);
    impulse.samples[0] = impulse.samples[1] = 0.2f;
    VoiceSettings effectSettings; effectSettings.fadeOut = 0; effectSettings.delayMix = 0.5f;
    effectSettings.requestedAtNs = AudioCore::nowNs();
    effects.trigger(1, &impulse, effectSettings); effects.render(left, right, 225);
    require(std::abs(left[220] - 0.0325f) < 0.001f && effects.lastOnsetLatencyMs() >= 0,
            "delay tail and software onset measurement");
    effects.stop(); effects.render(left, right, 1);
    effectSettings.delayMix = 0; effectSettings.filterMix = 1; effectSettings.filterCutoffHz = 100;
    effects.trigger(1, &impulse, effectSettings); effects.render(left, right, 3);
    require(left[0] > 0 && left[0] < 0.2f && left[1] > 0, "per-voice low-pass filter");
    effects.stop(); effects.render(left, right, 1);
    effectSettings.filterMix = 0;
    effects.trigger(1, &impulse, effectSettings); effects.render(left, right, 3);
    require(std::abs(left[0] - 0.2f) < 0.0001f && left[1] == 0, "zero filter mix keeps dry signal");
    effects.stop(); effects.render(left, right, 1);
    effectSettings.filterMix = 0.5f;
    effects.trigger(1, &impulse, effectSettings); effects.render(left, right, 3);
    require(left[0] > 0.14f && left[0] < 0.2f && left[1] > 0, "filter mix blends dry and wet signal");
    effects.stop(); effects.render(left, right, 1);
    AudioCore frequencyEffects;
    frequencyEffects.setVolume(1); frequencyEffects.prepare(48000);
    auto lowTone = tone(1024, 100), highTone = tone(1024, 5000);
    VoiceSettings spectral; spectral.fadeOut = 0; spectral.filterCutoffHz = 500; spectral.filterMix = 1;
    frequencyEffects.trigger(1, &lowTone, spectral); frequencyEffects.render(left, right, 1024);
    const auto lowThroughLowPass = rms(left, 512, 1024);
    frequencyEffects.stop(); frequencyEffects.render(left, right, 1);
    frequencyEffects.trigger(1, &highTone, spectral); frequencyEffects.render(left, right, 1024);
    const auto highThroughLowPass = rms(left, 512, 1024);
    require(lowThroughLowPass > highThroughLowPass * 4.0, "low-pass attenuates high frequencies");
    frequencyEffects.stop(); frequencyEffects.render(left, right, 1);
    spectral.filterMix = 0; spectral.highPassCutoffHz = 1000; spectral.highPassMix = 1;
    frequencyEffects.trigger(1, &lowTone, spectral); frequencyEffects.render(left, right, 1024);
    const auto lowThroughHighPass = rms(left, 512, 1024);
    frequencyEffects.stop(); frequencyEffects.render(left, right, 1);
    frequencyEffects.trigger(1, &highTone, spectral); frequencyEffects.render(left, right, 1024);
    const auto highThroughHighPass = rms(left, 512, 1024);
    require(highThroughHighPass > lowThroughHighPass * 4.0, "high-pass attenuates low frequencies");
    frequencyEffects.stop(); frequencyEffects.render(left, right, 1);
    spectral.highPassMix = 0;
    frequencyEffects.trigger(1, &lowTone, spectral); frequencyEffects.render(left, right, 1024);
    require(rms(left, 512, 1024) > lowThroughHighPass * 4.0, "zero high-pass mix keeps dry signal");
    frequencyEffects.stop(); frequencyEffects.render(left, right, 1);
    effectSettings.filterMix = 0; effectSettings.delayTimeMs = 50;
    effectSettings.delayFeedback = 0; effectSettings.delayMix = 0.5f;
    effects.trigger(1, &impulse, effectSettings); effects.render(left, right, 110);
    require(std::abs(left[50] - 0.05f) < 0.001f && std::abs(left[100]) < 0.0001f,
            "delay time and zero feedback give one attenuated repeat");
    effects.stop(); effects.render(left, right, 1);
    effectSettings.delayFeedback = 0.5f;
    effects.trigger(1, &impulse, effectSettings); effects.render(left, right, 110);
    require(std::abs(left[50] - 0.025f) < 0.001f && std::abs(left[100] - 0.0125f) < 0.001f,
            "delay feedback controls subsequent repeats");
    effects.stop(); effects.render(left, right, 1);
    VoiceSettings newEffect; newEffect.fadeOut = 0;
    newEffect.reverbMix = 1;
    effects.trigger(1, &impulse, newEffect); effects.render(left, right, 800);
    require(left[0] == 0.2f && rms(left, 20, 800) > 0.0001,
            "reverb retains the dry impulse and adds a diffuse audible tail");
    const double longReverbTail = rms(left, 300, 800);
    effects.stop(); effects.render(left, right, 1);
    newEffect.reverbDecay = 0.2f;
    effects.trigger(1, &impulse, newEffect); effects.render(left, right, 800);
    require(rms(left, 300, 800) < longReverbTail * 0.35, "short reverb decay reduces late tail energy");
    effects.stop(); effects.render(left, right, 1);
    newEffect.reverbMix = 0; newEffect.pingPongMix = 1; newEffect.pingPongTimeMs = 50;
    newEffect.pingPongFeedback = 0.5f;
    auto leftImpulse = constant(4, 0); leftImpulse.samples[0] = 0.2f;
    effects.trigger(1, &leftImpulse, newEffect); effects.render(left, right, 110);
    require(std::abs(left[50] - 0.1f) < 0.001f && right[50] == 0 && std::abs(right[100] - 0.05f) < 0.001f,
            "ping-pong delay alternates stereo channels");
    effects.stop(); effects.render(left, right, 1);
    newEffect.pingPongMix = 0; newEffect.chorusMix = 1;
    effects.trigger(1, &impulse, newEffect); effects.render(left, right, 50);
    float chorusTail = 0;
    for (int i = 15; i < 50; ++i) chorusTail += std::abs(left[i]);
    require(chorusTail > 0.01f, "chorus adds a modulated short-delay copy");
    effects.stop(); effects.render(left, right, 1);
    auto loud = constant(1000, 0.6f);
    newEffect.chorusMix = 0; newEffect.compressorEnabled = true;
    effects.trigger(1, &loud, newEffect); effects.render(left, right, 200);
    require(left[199] < 0.3f && left[199] > 0, "compressor reduces sustained level above threshold");
    effects.stop(); effects.render(left, right, 1);
    newEffect.compressorEnabled = false; newEffect.tremoloEnabled = true;
    newEffect.tremoloDepth = 1; newEffect.tremoloSpeedHz = 1;
    effects.trigger(1, &loud, newEffect); effects.render(left, right, 800);
    require(left[250] > left[0] * 1.9f && left[250] > left[750] * 1.9f,
            "tremolo changes amplitude at the selected rate");
    effects.stop(); effects.render(left, right, 1);
    newEffect.tremoloEnabled = false; newEffect.distortionEnabled = true;
    newEffect.distortionDrive = 50;
    effects.trigger(1, &impulse, newEffect); effects.render(left, right, 2);
    require(left[0] > 0.2f && left[0] <= 0.98f, "distortion increases saturation under limiter");
    effects.stop(); effects.render(left, right, 1);
    float micLeft[2] {0.2f, 0.2f}, micRight[2] {0.1f, 0.1f};
    effects.render(left, right, 2, micLeft, micRight, 0.5f);
    require(std::abs(left[0] - 0.1f) < 0.0001f && std::abs(right[0] - 0.05f) < 0.0001f,
            "microphone signal joins output mix before limiter");
    std::cout << "PASS: " << passed << " checks (playback, loop, release, pause, trim, resampling, limiter, queue)\n";
}
