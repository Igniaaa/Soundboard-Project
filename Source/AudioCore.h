#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <utility>
#include <vector>
#include "SpectralScrub.h"

namespace dualdeck
{
enum class PlayMode { restart, overlap, toggleLoop, holdLoop, scrub };
struct ClipData
{
    std::vector<float> samples; // Stereo, interleaved. Owned by the non-audio cache.
    double sampleRate = 48000.0;
    mutable std::atomic<unsigned> activeReferences {0}; // Pins queued commands and live voices against cache eviction.
    ClipData() = default;
    ClipData(const ClipData&) = delete;
    ClipData& operator=(const ClipData&) = delete;
    ClipData(ClipData&& other) noexcept : samples(std::move(other.samples)), sampleRate(other.sampleRate) {}
    ClipData& operator=(ClipData&& other) noexcept
    {
        samples = std::move(other.samples); sampleRate = other.sampleRate;
        activeReferences.store(0, std::memory_order_relaxed);
        return *this;
    }
    int frames() const { return static_cast<int>(samples.size() / 2); }
    double duration() const { return frames() / sampleRate; }
};
struct VoiceSettings
{
    PlayMode mode = PlayMode::restart;
    float gain = 0.85f;
    bool overrideMasterVolume = false;
    double speed = 1.0, trimStart = 0.0, trimEnd = 0.0, fadeIn = 0.0, fadeOut = 0.08;
    float filterCutoffHz = 3000.0f, filterMix = 0.0f;
    float highPassCutoffHz = 100.0f, highPassMix = 0.0f;
    float delayTimeMs = 220.0f, delayFeedback = 0.35f, delayMix = 0.0f;
    float reverbDecay = 2.0f, reverbMix = 0.0f;
    float compressorThresholdDb = -24.0f, compressorRatio = 4.0f;
    bool compressorEnabled = false;
    float tremoloSpeedHz = 1.6f, tremoloDepth = 0.5f;
    bool tremoloEnabled = false;
    float distortionDrive = 40.0f;
    bool distortionEnabled = false;
    float pingPongTimeMs = 250.0f, pingPongFeedback = 0.3f, pingPongMix = 0.0f;
    float chorusSpeedHz = 1.5f, chorusDepth = 0.7f, chorusMix = 0.0f;
    std::uint64_t requestedAtNs = 0; // Runtime measurement; never persisted.
};
struct Command
{
    enum Type { start, release } type = start;
    std::uint64_t id = 0, generation = 0;
    const ClipData* clip = nullptr;
    VoiceSettings settings;
};

// Exactly one producer (JUCE message thread) and one consumer (device callback).
template <std::size_t Capacity> class CommandQueue
{
public:
    bool push(const Command& c)
    {
        const auto w = write.load(std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == read.load(std::memory_order_acquire)) return false;
        commands[w] = c;
        write.store(next, std::memory_order_release);
        return true;
    }
    bool pop(Command& c)
    {
        const auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        c = commands[r];
        read.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    std::array<Command, Capacity> commands {};
    std::atomic<std::size_t> read {0}, write {0};
};

class AudioCore
{
public:
    static constexpr int maxVoices = 32;
    static std::uint64_t nowNs()
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    double lastOnsetLatencyMs() const { return lastOnsetMs.load(std::memory_order_relaxed); }
    void trackVoice(std::uint64_t id)
    {
        trackedId.store(0, std::memory_order_release);
        trackedEverStarted.store(false, std::memory_order_release);
        trackedId.store(id, std::memory_order_release);
        publishedId.store(0, std::memory_order_release);
    }
    bool trackedVoiceStarted() const { return trackedEverStarted.load(std::memory_order_acquire); }
    double trackedPositionSeconds(std::uint64_t id) const
    {
        return publishedId.load(std::memory_order_acquire) == id
            ? publishedSeconds.load(std::memory_order_relaxed) : -1.0;
    }
    bool trigger(std::uint64_t id, const ClipData* clip, const VoiceSettings& settings)
    {
        if (clip == nullptr) return false;
        clip->activeReferences.fetch_add(1, std::memory_order_acq_rel);
        if (queue.push({Command::start, id, generation.load(), clip, settings})) return true;
        clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
        return false;
    }
    void release(std::uint64_t id)
    {
        if (!queue.push({Command::release, id, generation.load(), nullptr, {}})) stop();
    }
    void stop() { generation.fetch_add(1); }
    // Call only after the device callback has stopped. A closed device cannot
    // consume stale commands or release the clips they keep in the cache.
    void resetAfterDeviceStop()
    {
        stop();
        for (auto& voice : voices) clearVoice(voice);
        groupCount = 0;
        Command command;
        while (queue.pop(command)) releaseClip(command.clip);
        consumedGeneration = generation.load();
        active.store(0);
        peak.store(0);
        spectralScrub.reset();
        publishedId.store(0, std::memory_order_release);
    }
    void pause(bool value) { paused.store(value); }
    void setVolume(float value) { targetVolume.store(std::clamp(value, 0.0f, 1.0f)); }
    void setPitch(double semitones)
    {
        targetPitch.store(std::pow(2.0, (std::isfinite(semitones) ? std::clamp(semitones, -12.0, 12.0) : 0.0) / 12.0));
    }
    void setScrubTarget(double seconds)
    {
        if (std::isfinite(seconds)) scrubTargetSeconds.store(std::max(0.0, seconds), std::memory_order_relaxed);
    }
    int activeVoices() const { return active.load(); }
    float meter() const { return peak.load(); }
    void prepare(double rate)
    {
        sampleRate = rate > 0 ? rate : 48000.0;
        delayCapacity = std::max(1, static_cast<int>(std::ceil(sampleRate)));
        delay.assign(static_cast<std::size_t>(maxVoices * delayCapacity), {});
        pingPong.assign(static_cast<std::size_t>(maxVoices * delayCapacity), {});
        reverbCapacity = 0;
        for (std::size_t line = 0; line < reverbFrames.size(); ++line)
        {
            reverbOffsets[line] = reverbCapacity;
            reverbFrames[line] = std::max(1, static_cast<int>(std::round(sampleRate * reverbTimesMs[line] / 1000.0)));
            reverbCapacity += reverbFrames[line];
        }
        int diffusionFrames = 0;
        for (std::size_t stage = 0; stage < reverbDiffuserFrames.size(); ++stage)
        {
            reverbDiffuserOffsets[stage] = reverbCapacity;
            reverbDiffuserFrames[stage] = std::max(1, static_cast<int>(std::round(sampleRate * reverbDiffuserTimesMs[stage] / 1000.0)));
            reverbCapacity += reverbDiffuserFrames[stage];
            diffusionFrames += reverbDiffuserFrames[stage];
        }
        reverbDiffusionTailFrames = diffusionFrames
            * static_cast<int>(std::ceil(std::log(0.0001) / std::log(reverbDiffuserGain)));
        reverbDampingAlpha = static_cast<float>(1.0 - std::exp(-6.283185307179586 * 6000.0 / sampleRate));
        reverb.assign(static_cast<std::size_t>(maxVoices * reverbCapacity), {});
        chorusCapacity = std::max(1, static_cast<int>(std::ceil(sampleRate * 0.05)));
        chorus.assign(static_cast<std::size_t>(maxVoices * chorusCapacity), {});
        spectralScrub.prepare(sampleRate);
        volume = targetVolume.load();
        pitch = targetPitch.load();
        limiterGain = 1.0f;
        limiterHoldFrames = 0;
        for (auto& v : voices) clearVoice(v);
        groupCount = 0;
        active.store(0);
        publishedId.store(0, std::memory_order_release);
    }
    // No allocation, mutex, file I/O, object destruction or UI work in this method.
    void render(float* left, float* right, int count, const float* microphoneLeft = nullptr,
                const float* microphoneRight = nullptr, float microphoneGain = 0.0f)
    {
        if (left) std::fill_n(left, count, 0.0f);
        if (right) std::fill_n(right, count, 0.0f);
        const auto currentGeneration = generation.load();
        if (currentGeneration != consumedGeneration)
        {
            for (auto& v : voices) clearVoice(v);
            groupCount = 0;
            consumedGeneration = currentGeneration;
        }
        Command command;
        for (int i = 0; i < 256 && queue.pop(command); ++i)
        {
            if (command.generation == currentGeneration)
            {
                if (command.type == Command::start && command.id == trackedId.load(std::memory_order_acquire))
                    trackedEverStarted.store(true, std::memory_order_release);
                apply(command);
            }
            else releaseClip(command.clip);
        }
        // Build pad groups once per callback. Keep each group's smoothing state
        // across callbacks without allocating or looking up IDs per sample.
        std::array<VoiceGroup, maxVoices> currentGroups {};
        int currentGroupCount = 0;
        for (auto& v : voices)
        {
            if (!v.clip) continue;
            int index = 0;
            while (index < currentGroupCount && currentGroups[static_cast<std::size_t>(index)].id != v.id) ++index;
            if (index == currentGroupCount)
            {
                for (int old = 0; old < groupCount; ++old)
                    if (groups[static_cast<std::size_t>(old)].id == v.id)
                    {
                        currentGroups[static_cast<std::size_t>(index)] = groups[static_cast<std::size_t>(old)];
                        break;
                    }
                currentGroups[static_cast<std::size_t>(index)].id = v.id;
                ++currentGroupCount;
            }
            v.groupIndex = index;
        }
        float blockPeak = 0;
        const bool isPaused = paused.load();
        const float wantedVolume = targetVolume.load();
        const double wantedPitch = targetPitch.load();
        const float smooth = static_cast<float>(1.0 - std::exp(-1.0 / (0.01 * sampleRate)));
        const float recovery = static_cast<float>(1.0 - std::exp(-1.0 / (0.10 * sampleRate)));
        const int limiterHoldLength = std::max(1, static_cast<int>(std::ceil(0.02 * sampleRate)));
        const float energySmooth = static_cast<float>(1.0 - std::exp(-1.0 / (0.02 * sampleRate)));
        const float balanceSmooth = smooth;
        double scrubTarget = scrubTargetSeconds.load(std::memory_order_relaxed);
        for (int frame = 0; frame < count; ++frame)
        {
            if ((frame & 63) == 0)
                scrubTarget = scrubTargetSeconds.load(std::memory_order_relaxed);
            volume += (wantedVolume - volume) * smooth;
            pitch += (wantedPitch - pitch) * smooth;
            float l = 0, r = 0;
            if (!isPaused)
            {
                for (int group = 0; group < currentGroupCount; ++group)
                {
                    auto& current = currentGroups[static_cast<std::size_t>(group)];
                    current.sumL = current.sumR = current.loudestPower = 0.0;
                    current.contributors = 0;
                }
                for (auto& v : voices)
                {
                    if (!v.clip) continue;
                    bool tail = false;
                    if (!v.scrubLoop && v.position >= v.end)
                    {
                        if (v.loop) v.position = v.begin + std::fmod(v.position - v.begin, v.end - v.begin);
                        else if (v.tailFrames > 0) { tail = true; --v.tailFrames; }
                        else { clearVoice(v); continue; }
                    }
                    if (v.released && v.releaseFrames <= 0) { clearVoice(v); continue; }
                    float envelope = tail ? 1.0f : v.fadeInFrames > 0 ? static_cast<float>(std::min(1.0, v.age / v.fadeInFrames)) : 1.0f;
                    if (!tail && !v.loop && v.fadeOutFrames > 0)
                        envelope *= static_cast<float>(std::min(1.0, (v.end - v.position) / (v.step * pitch) / v.fadeOutFrames));
                    float releaseEnvelope = 1.0f;
                    if (v.released)
                    {
                        releaseEnvelope = static_cast<float>(v.releaseFrames / std::max(1.0, v.fadeOutFrames));
                        --v.releaseFrames;
                    }
                    float sourceL = 0, sourceR = 0;
                    if (!tail)
                    {
                        if (v.scrubLoop) renderScrub(v, scrubTarget, sourceL, sourceR);
                        else
                        {
                            const auto index = static_cast<int>(v.position);
                            const auto next = index + 1 < v.end ? index + 1 : (v.loop ? v.begin : index);
                            const auto fraction = static_cast<float>(v.position - index);
                            const auto& s = v.clip->samples;
                            sourceL = s[index * 2] + fraction * (s[next * 2] - s[index * 2]);
                            sourceR = s[index * 2 + 1] + fraction * (s[next * 2 + 1] - s[index * 2 + 1]);
                        }
                        if (v.filterMix > 0.0f)
                        {
                            v.filterL += v.filterAlpha * (sourceL - v.filterL);
                            v.filterR += v.filterAlpha * (sourceR - v.filterR);
                            sourceL += (v.filterL - sourceL) * v.filterMix;
                            sourceR += (v.filterR - sourceR) * v.filterMix;
                        }
                        if (v.highPassMix > 0.0f)
                        {
                            v.highPassLowL += v.highPassAlpha * (sourceL - v.highPassLowL);
                            v.highPassLowR += v.highPassAlpha * (sourceR - v.highPassLowR);
                            sourceL -= v.highPassLowL * v.highPassMix;
                            sourceR -= v.highPassLowR * v.highPassMix;
                        }
                    }
                    if (v.distortionEnabled)
                    {
                        sourceL = std::tanh(sourceL * v.distortionGain) / std::tanh(v.distortionGain);
                        sourceR = std::tanh(sourceR * v.distortionGain) / std::tanh(v.distortionGain);
                    }
                    if (v.tremoloEnabled)
                    {
                        const float level = 1.0f - v.tremoloDepth * (0.5f - 0.5f * std::sin(v.tremoloPhase));
                        sourceL *= level; sourceR *= level;
                        v.tremoloPhase += v.tremoloIncrement;
                        if (v.tremoloPhase >= 6.2831853f) v.tremoloPhase -= 6.2831853f;
                    }
                    if (v.compressorEnabled)
                    {
                        const float level = std::max(std::abs(sourceL), std::abs(sourceR));
                        const float smoothing = level > v.compressorEnvelope ? v.compressorAttack : v.compressorRelease;
                        v.compressorEnvelope += (level - v.compressorEnvelope) * smoothing;
                        if (v.compressorEnvelope > v.compressorThreshold)
                        {
                            const float reduction = std::pow(v.compressorEnvelope / v.compressorThreshold,
                                                                1.0f / v.compressorRatio - 1.0f);
                            sourceL *= reduction; sourceR *= reduction;
                        }
                    }
                    // Fade the source feeding the effects, so reaching EOF
                    // does not fade the wet tail to zero and abruptly restore
                    // it on the very next sample. Explicit release still gates
                    // the complete voice below.
                    sourceL *= envelope; sourceR *= envelope;
                    if (v.delayMix > 0.0f && !delay.empty())
                    {
                        auto& cell = delay[static_cast<std::size_t>(v.delaySlot * delayCapacity + v.delayCursor)];
                        const float delayedL = cell.epoch == v.delayEpoch ? cell.left : 0.0f;
                        const float delayedR = cell.epoch == v.delayEpoch ? cell.right : 0.0f;
                        // Bound feedback accumulation and keep the first echo
                        // at least 6 dB below the dry hit, even with zero
                        // feedback and maximum Mix. Leave the dry path intact.
                        const float inputGain = 1.0f - v.delayFeedback;
                        cell = {sourceL * inputGain + delayedL * v.delayFeedback,
                                sourceR * inputGain + delayedR * v.delayFeedback, v.delayEpoch};
                        sourceL += delayedL * v.delayMix * 0.5f;
                        sourceR += delayedR * v.delayMix * 0.5f;
                        if (++v.delayCursor == v.delayFrames) v.delayCursor = 0;
                    }
                    if (v.pingPongMix > 0.0f)
                    {
                        auto& cell = pingPong[static_cast<std::size_t>(v.delaySlot * delayCapacity + v.pingPongCursor)];
                        const float wetL = cell.epoch == v.delayEpoch ? cell.left : 0.0f;
                        const float wetR = cell.epoch == v.delayEpoch ? cell.right : 0.0f;
                        const float inputGain = 1.0f - v.pingPongFeedback;
                        cell = {sourceL * inputGain + wetR * v.pingPongFeedback,
                                sourceR * inputGain + wetL * v.pingPongFeedback, v.delayEpoch};
                        sourceL += wetL * v.pingPongMix;
                        sourceR += wetR * v.pingPongMix;
                        if (++v.pingPongCursor == v.pingPongFrames) v.pingPongCursor = 0;
                    }
                    if (v.chorusMix > 0.0f)
                    {
                        const auto base = v.delaySlot * chorusCapacity;
                        auto& write = chorus[static_cast<std::size_t>(base + v.chorusCursor)];
                        write = {sourceL, sourceR, v.delayEpoch};
                        const auto read = [&](float offset, bool rightChannel)
                        {
                            float position = static_cast<float>(v.chorusCursor) - offset;
                            while (position < 0) position += static_cast<float>(chorusCapacity);
                            const int a = static_cast<int>(position) % chorusCapacity;
                            const int b = (a + 1) % chorusCapacity;
                            const float fraction = position - std::floor(position);
                            const auto& first = chorus[static_cast<std::size_t>(base + a)];
                            const auto& second = chorus[static_cast<std::size_t>(base + b)];
                            const float x = first.epoch == v.delayEpoch ? (rightChannel ? first.right : first.left) : 0.0f;
                            const float y = second.epoch == v.delayEpoch ? (rightChannel ? second.right : second.left) : 0.0f;
                            return x + fraction * (y - x);
                        };
                        const float excursion = v.chorusDepth * static_cast<float>(sampleRate * 0.01);
                        const float baseDelay = static_cast<float>(sampleRate * 0.025);
                        sourceL += read(baseDelay + excursion * std::sin(v.chorusPhase), false) * v.chorusMix;
                        sourceR += read(baseDelay - excursion * std::sin(v.chorusPhase), true) * v.chorusMix;
                        v.chorusPhase += v.chorusIncrement;
                        if (v.chorusPhase >= 6.2831853f) v.chorusPhase -= 6.2831853f;
                        if (++v.chorusCursor == chorusCapacity) v.chorusCursor = 0;
                    }
                    if (v.reverbMix > 0.0f)
                    {
                        const auto base = v.delaySlot * reverbCapacity;
                        float wetL = 0.0f, wetR = 0.0f;
                        for (std::size_t line = 0; line < reverbFrames.size(); ++line)
                        {
                            auto& cell = reverb[static_cast<std::size_t>(base + reverbOffsets[line] + v.reverbCursors[line])];
                            const float delayedL = cell.epoch == v.delayEpoch ? cell.left : 0.0f;
                            const float delayedR = cell.epoch == v.delayEpoch ? cell.right : 0.0f;
                            const float feedback = v.reverbFeedback[line];
                            v.reverbDampedL[line] += reverbDampingAlpha * (delayedL - v.reverbDampedL[line]);
                            v.reverbDampedR[line] += reverbDampingAlpha * (delayedR - v.reverbDampedR[line]);
                            // Suppress inaudibly small feedback before it can
                            // enter slow subnormal arithmetic on a long tail.
                            if (std::abs(v.reverbDampedL[line]) < 1e-20f) v.reverbDampedL[line] = 0;
                            if (std::abs(v.reverbDampedR[line]) < 1e-20f) v.reverbDampedR[line] = 0;
                            cell = {sourceL * (1.0f - feedback) + v.reverbDampedL[line] * feedback,
                                    sourceR * (1.0f - feedback) + v.reverbDampedR[line] * feedback, v.delayEpoch};
                            wetL += line % 2 == 0 ? delayedL : delayedR;
                            wetR += line % 2 == 0 ? delayedR : delayedL;
                            if (++v.reverbCursors[line] == reverbFrames[line]) v.reverbCursors[line] = 0;
                        }
                        wetL *= 0.25f; wetR *= 0.25f;
                        // True Schroeder allpasses diffuse the reflections
                        // without normalising or delaying the dry path. Both
                        // channels have their own cells, including anti-phase
                        // inputs; epochs hide every previous voice's contents.
                        for (std::size_t stage = 0; stage < reverbDiffuserFrames.size(); ++stage)
                        {
                            auto& cell = reverb[static_cast<std::size_t>(base + reverbDiffuserOffsets[stage]
                                                                        + v.reverbDiffuserCursors[stage])];
                            const float delayedL = cell.epoch == v.delayEpoch ? cell.left : 0.0f;
                            const float delayedR = cell.epoch == v.delayEpoch ? cell.right : 0.0f;
                            const float diffusedL = delayedL - reverbDiffuserGain * wetL;
                            const float diffusedR = delayedR - reverbDiffuserGain * wetR;
                            const float storedL = wetL + reverbDiffuserGain * diffusedL;
                            const float storedR = wetR + reverbDiffuserGain * diffusedR;
                            cell = {std::abs(storedL) < 1e-20f ? 0.0f : storedL,
                                    std::abs(storedR) < 1e-20f ? 0.0f : storedR, v.delayEpoch};
                            wetL = diffusedL; wetR = diffusedR;
                            if (++v.reverbDiffuserCursors[stage] == reverbDiffuserFrames[stage])
                                v.reverbDiffuserCursors[stage] = 0;
                        }
                        sourceL += wetL * v.reverbMix;
                        sourceR += wetR * v.reverbMix;
                    }
                    const float gain = (v.overrideMasterVolume ? v.gain : volume) * releaseEnvelope;
                    const float outL = sourceL * gain, outR = sourceR * gain;
                    if (std::isfinite(outL) && std::isfinite(outR))
                    {
                        auto& group = currentGroups[static_cast<std::size_t>(v.groupIndex)];
                        group.sumL += outL; group.sumR += outR;
                        ++group.contributors;
                        group.loudestPower = std::max(group.loudestPower,
                            0.5 * (static_cast<double>(outL) * outL + static_cast<double>(outR) * outR));
                    }
                    if (!tail && !v.scrubLoop) v.position += v.step * pitch;
                    ++v.age;
                }
                for (int group = 0; group < currentGroupCount; ++group)
                {
                    auto& current = currentGroups[static_cast<std::size_t>(group)];
                    if (current.contributors <= 1 && current.gain > 1.0f)
                        current.gain = 1.0f;
                    if (current.contributors <= 1 && current.gain == 1.0f)
                    {
                        current.targetPower = current.mixedPower = 0.0f;
                        l += static_cast<float>(current.sumL);
                        r += static_cast<float>(current.sumR);
                        continue;
                    }
                    const double mixedPower = 0.5 * (current.sumL * current.sumL + current.sumR * current.sumR);
                    current.targetPower += (current.loudestPower - current.targetPower) * energySmooth;
                    current.mixedPower += (mixedPower - current.mixedPower) * energySmooth;
                    const float wanted = current.targetPower > 1.0e-8f
                        ? static_cast<float>(std::clamp(std::sqrt(current.targetPower / std::max(current.mixedPower, 1.0e-8)),
                                                        1.0 / maxVoices, 3.0)) : 1.0f;
                    current.gain += (wanted - current.gain) * balanceSmooth;
                    if (current.contributors <= 1 && std::abs(current.gain - 1.0f) < 0.0001f)
                    {
                        current.gain = 1.0f;
                        current.targetPower = current.mixedPower = 0.0f;
                    }
                    l += static_cast<float>(current.sumL * current.gain);
                    r += static_cast<float>(current.sumR * current.gain);
                }
            }
            if (!std::isfinite(l)) l = 0;
            if (!std::isfinite(r)) r = 0;
            if (microphoneLeft && microphoneGain > 0.0f)
            {
                const float ml = microphoneLeft[frame], mr = microphoneRight ? microphoneRight[frame] : ml;
                if (std::isfinite(ml)) l += ml * microphoneGain;
                if (std::isfinite(mr)) r += mr * microphoneGain;
            }
            const float p = std::max(std::abs(l), std::abs(r));
            const float required = p > 0.98f ? 0.98f / p : 1.0f;
            if (required < limiterGain) limiterGain = required;
            // Keep the gain steady between bass peaks. A tolerance of 0.1%
            // also catches peaks that fall between samples on later cycles.
            // Release only after the overload has actually subsided, rather
            // than modulating the gain at every zero crossing. No lookahead.
            if (required < 1.0f && required <= limiterGain * 1.001f)
                limiterHoldFrames = limiterHoldLength;
            else if (limiterHoldFrames > 0)
                --limiterHoldFrames;
            else
                limiterGain += (required - limiterGain) * recovery;
            l = static_cast<float>(l * limiterGain);
            r = static_cast<float>(r * limiterGain);
            if (left) left[frame] = l;
            if (right) right[frame] = r;
            blockPeak = std::max(blockPeak, std::max(std::abs(l), std::abs(r)));
        }
        int n = 0;
        bool scrubActive = false;
        for (const auto& v : voices) if (v.clip)
        {
            ++n;
            scrubActive |= v.scrubLoop;
        }
        if (!scrubActive) spectralScrub.reset();
        for (int group = 0; group < currentGroupCount; ++group)
            groups[static_cast<std::size_t>(group)] = currentGroups[static_cast<std::size_t>(group)];
        groupCount = currentGroupCount;
        active.store(n);
        peak.store(blockPeak);
        const auto watched = trackedId.load(std::memory_order_acquire);
        double position = -1.0;
        if (watched != 0)
            for (const auto& v : voices) if (v.clip && v.id == watched)
            { position = std::min(v.position / v.clip->sampleRate, v.clip->duration()); break; }
        publishedSeconds.store(position, std::memory_order_relaxed);
        publishedId.store(watched, std::memory_order_release);
    }
private:
    struct Voice
    {
        const ClipData* clip = nullptr;
        std::uint64_t id = 0, serial = 0;
        int begin = 0, end = 0;
        double position = 0, step = 1, age = 0, fadeInFrames = 0, fadeOutFrames = 0, releaseFrames = 0;
        float gain = 1;
        bool overrideMasterVolume = false;
        float filterAlpha = 1, filterMix = 0, filterL = 0, filterR = 0;
        float highPassAlpha = 1, highPassMix = 0, highPassLowL = 0, highPassLowR = 0;
        float delayMix = 0, delayFeedback = 0;
        int delaySlot = 0, delayCursor = 0, delayFrames = 1, tailFrames = 0;
        float reverbMix = 0;
        std::array<float, 4> reverbFeedback {};
        std::array<float, 4> reverbDampedL {}, reverbDampedR {};
        std::array<int, 4> reverbCursors {};
        std::array<int, 4> reverbDiffuserCursors {};
        float pingPongMix = 0, pingPongFeedback = 0;
        int pingPongCursor = 0, pingPongFrames = 1, pingPongTailFrames = 0;
        float chorusMix = 0, chorusDepth = 0, chorusPhase = 0, chorusIncrement = 0;
        int chorusCursor = 0;
        bool compressorEnabled = false, tremoloEnabled = false, distortionEnabled = false;
        float compressorThreshold = 1, compressorRatio = 1, compressorEnvelope = 0;
        float compressorAttack = 1, compressorRelease = 1;
        float tremoloDepth = 0, tremoloPhase = 0, tremoloIncrement = 0;
        float distortionGain = 1;
        std::uint32_t delayEpoch = 0;
        int groupIndex = 0;
        bool loop = false, scrubLoop = false, released = false;
    };
    struct VoiceGroup
    {
        std::uint64_t id = 0;
        double targetPower = 0, mixedPower = 0;
        double sumL = 0, sumR = 0, loudestPower = 0;
        float gain = 1;
        int contributors = 0;
    };
    struct DelayCell { float left = 0, right = 0; std::uint32_t epoch = 0; };
    static void releaseClip(const ClipData* clip)
    {
        if (clip != nullptr) clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
    }
    static void clearVoice(Voice& voice)
    {
        if (voice.clip != nullptr) releaseClip(voice.clip);
        voice.clip = nullptr;
    }
    void renderScrub(Voice& voice, double targetSeconds, float& left, float& right)
    {
        const auto sample = spectralScrub.next(targetSeconds);
        left = sample.first; right = sample.second;
        voice.position = std::clamp(targetSeconds, 0.0, voice.clip->duration()) * voice.clip->sampleRate;
    }
    void apply(const Command& c)
    {
        if (c.type == Command::release)
        {
            for (auto& v : voices) if (v.clip && v.id == c.id && !v.released)
            { v.released = true; v.releaseFrames = v.fadeOutFrames; }
            return;
        }
        bool found = false;
        if (c.settings.mode != PlayMode::overlap)
            for (auto& v : voices) if (v.clip && v.id == c.id) { clearVoice(v); found = true; }
        if (found && c.settings.mode == PlayMode::toggleLoop) { releaseClip(c.clip); return; }
        if (!c.clip || c.clip->frames() < 1) { releaseClip(c.clip); return; }
        const int begin = std::clamp(static_cast<int>(c.settings.trimStart * c.clip->sampleRate), 0, c.clip->frames());
        const int end = c.settings.trimEnd > 0 ? std::clamp(static_cast<int>(c.settings.trimEnd * c.clip->sampleRate), 0, c.clip->frames()) : c.clip->frames();
        if (begin >= end) { releaseClip(c.clip); return; }
        // SpectralScrub owns one analysis state per output. A second scrub must
        // replace the first before that state starts reading a different clip.
        if (c.settings.mode == PlayMode::scrub)
            for (auto& voice : voices) if (voice.clip && voice.scrubLoop) clearVoice(voice);
        Voice* slot = nullptr;
        for (auto& v : voices) if (!v.clip) { slot = &v; break; }
        if (!slot)
        {
            for (auto& voice : voices)
                if (!voice.scrubLoop && (!slot || voice.serial < slot->serial)) slot = &voice;
            if (!slot)
                slot = &*std::min_element(voices.begin(), voices.end(),
                    [](const Voice& a, const Voice& b) { return a.serial < b.serial; });
            clearVoice(*slot);
        }
        *slot = {};
        auto& v = *slot;
        v.begin = begin;
        v.end = end;
        v.clip = c.clip; v.id = c.id; v.serial = ++serial;
        v.position = v.begin;
        v.step = std::clamp(c.settings.speed, 0.5, 2.0) * c.clip->sampleRate / sampleRate;
        v.gain = std::clamp(c.settings.gain, 0.0f, 1.0f);
        v.overrideMasterVolume = c.settings.overrideMasterVolume;
        v.fadeInFrames = std::max(0.0, c.settings.fadeIn) * sampleRate;
        v.fadeOutFrames = std::max(0.0, c.settings.fadeOut) * sampleRate;
        v.scrubLoop = c.settings.mode == PlayMode::scrub;
        v.loop = c.settings.mode == PlayMode::toggleLoop || c.settings.mode == PlayMode::holdLoop || v.scrubLoop;
        if (v.scrubLoop)
        {
            const double target = std::clamp(scrubTargetSeconds.load(std::memory_order_relaxed), 0.0, c.clip->duration());
            v.position = target * c.clip->sampleRate;
            spectralScrub.start(c.clip->samples.data(), c.clip->frames(), c.clip->sampleRate, target);
        }
        v.filterMix = std::clamp(c.settings.filterMix, 0.0f, 1.0f);
        const auto maximumCutoff = std::max(20.0, std::min(18000.0, sampleRate * 0.45));
        const auto cutoff = std::clamp(std::isfinite(c.settings.filterCutoffHz)
            ? static_cast<double>(c.settings.filterCutoffHz) : 3000.0, 20.0, maximumCutoff);
        v.filterAlpha = static_cast<float>(1.0 - std::exp(-6.283185307179586 * cutoff / sampleRate));
        v.highPassMix = std::clamp(c.settings.highPassMix, 0.0f, 1.0f);
        const auto highPassCutoff = std::clamp(std::isfinite(c.settings.highPassCutoffHz)
            ? static_cast<double>(c.settings.highPassCutoffHz) : 100.0, 20.0, maximumCutoff);
        v.highPassAlpha = static_cast<float>(1.0 - std::exp(-6.283185307179586 * highPassCutoff / sampleRate));
        v.delayMix = std::clamp(c.settings.delayMix, 0.0f, 1.0f);
        v.delayFeedback = std::clamp(c.settings.delayFeedback, 0.0f, 0.8f);
        const auto delayTimeMs = std::isfinite(c.settings.delayTimeMs) ? c.settings.delayTimeMs : 220.0f;
        v.delayFrames = std::clamp(static_cast<int>(std::round(delayTimeMs * sampleRate / 1000.0)), 1, delayCapacity);
        v.delaySlot = static_cast<int>(slot - voices.data());
        v.delayEpoch = ++nextDelayEpoch;
        const int repeats = v.delayFeedback > 0.0f
            ? std::clamp(static_cast<int>(std::ceil(std::log(0.0001) / std::log(v.delayFeedback))), 1, 48) : 1;
        v.tailFrames = v.delayMix > 0.0f && !v.loop ? v.delayFrames * repeats : 0;
        v.reverbMix = std::clamp(c.settings.reverbMix, 0.0f, 1.0f);
        const float reverbDecay = std::clamp(c.settings.reverbDecay, 0.2f, 2.0f);
        for (std::size_t line = 0; line < reverbFrames.size(); ++line)
            v.reverbFeedback[line] = static_cast<float>(std::exp(-6.907755278982137 * reverbFrames[line]
                / (sampleRate * reverbDecay)));
        if (v.reverbMix > 0.0f && !v.loop)
            v.tailFrames += static_cast<int>(reverbDecay * sampleRate * (4.0 / 3.0)) + reverbDiffusionTailFrames;
        v.pingPongMix = std::clamp(c.settings.pingPongMix, 0.0f, 1.0f);
        v.pingPongFeedback = std::clamp(c.settings.pingPongFeedback, 0.0f, 0.8f);
        const auto pingPongTime = std::isfinite(c.settings.pingPongTimeMs) ? c.settings.pingPongTimeMs : 250.0f;
        v.pingPongFrames = std::clamp(static_cast<int>(std::round(pingPongTime * sampleRate / 1000.0)), 1, delayCapacity);
        const int pingPongRepeats = v.pingPongFeedback > 0.0f
            ? std::clamp(static_cast<int>(std::ceil(std::log(0.0001) / std::log(v.pingPongFeedback))), 1, 48) : 1;
        if (v.pingPongMix > 0.0f && !v.loop)
            v.tailFrames += v.pingPongFrames * pingPongRepeats;
        v.chorusMix = std::clamp(c.settings.chorusMix, 0.0f, 1.0f);
        v.chorusDepth = std::clamp(c.settings.chorusDepth, 0.0f, 1.0f);
        v.chorusIncrement = 6.2831853f * std::clamp(c.settings.chorusSpeedHz, 0.1f, 10.0f) / static_cast<float>(sampleRate);
        if (v.chorusMix > 0.0f && !v.loop)
            v.tailFrames += chorusCapacity;
        v.compressorEnabled = c.settings.compressorEnabled;
        v.compressorThreshold = std::pow(10.0f, std::clamp(c.settings.compressorThresholdDb, -48.0f, 0.0f) / 20.0f);
        v.compressorRatio = std::clamp(c.settings.compressorRatio, 1.0f, 20.0f);
        v.compressorAttack = static_cast<float>(1.0 - std::exp(-1.0 / (0.01 * sampleRate)));
        v.compressorRelease = static_cast<float>(1.0 - std::exp(-1.0 / (0.1 * sampleRate)));
        v.tremoloEnabled = c.settings.tremoloEnabled;
        v.tremoloDepth = std::clamp(c.settings.tremoloDepth, 0.0f, 1.0f);
        v.tremoloIncrement = 6.2831853f * std::clamp(c.settings.tremoloSpeedHz, 0.1f, 16.0f) / static_cast<float>(sampleRate);
        v.distortionEnabled = c.settings.distortionEnabled;
        v.distortionGain = 1.0f + std::clamp(c.settings.distortionDrive, 0.0f, 100.0f) * 0.2f;
        if (c.settings.requestedAtNs > 0)
            lastOnsetMs.store(static_cast<double>(nowNs() - c.settings.requestedAtNs) / 1000000.0,
                              std::memory_order_relaxed);
    }
    CommandQueue<256> queue;
    std::array<Voice, maxVoices> voices {};
    std::array<VoiceGroup, maxVoices> groups {};
    int groupCount = 0;
    std::vector<DelayCell> delay;
    std::vector<DelayCell> pingPong, reverb, chorus;
    int delayCapacity = 1;
    int reverbCapacity = 1, chorusCapacity = 1;
    static constexpr std::array<int, 4> reverbTimesMs {37, 53, 71, 89};
    std::array<int, 4> reverbFrames {}, reverbOffsets {};
    static constexpr std::array<double, 4> reverbDiffuserTimesMs {5.1, 7.7, 10.3, 12.7};
    static constexpr float reverbDiffuserGain = 0.65f;
    std::array<int, 4> reverbDiffuserFrames {}, reverbDiffuserOffsets {};
    int reverbDiffusionTailFrames = 0;
    float reverbDampingAlpha = 1;
    std::uint32_t nextDelayEpoch = 0;
    std::atomic<std::uint64_t> generation {0};
    std::uint64_t consumedGeneration = 0, serial = 0;
    std::atomic<bool> paused {false};
    std::atomic<double> targetPitch {1.0};
    std::atomic<int> active {0};
    std::atomic<float> targetVolume {0.7f}, peak {0};
    std::atomic<double> lastOnsetMs {0.0};
    std::atomic<double> scrubTargetSeconds {0.0};
    SpectralScrub spectralScrub;
    std::atomic<std::uint64_t> trackedId {0}, publishedId {0};
    std::atomic<bool> trackedEverStarted {false};
    std::atomic<double> publishedSeconds {-1.0};
    double sampleRate = 48000.0, pitch = 1.0;
    float volume = 0.7f;
    // Double precision lets the slow release keep converging at 96/192 kHz
    // instead of stalling once a float increment falls below one ULP.
    double limiterGain = 1.0;
    int limiterHoldFrames = 0;
};
}
