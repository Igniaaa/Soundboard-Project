#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <utility>
#include <vector>
#include "PhantomScrub.h"

namespace dualdeck
{
// Variable-input-hop, fixed-output-hop time stretching: the cursor selects
// analysis frames while phase-locked oscillators advance at the source pitch,
// including reverse motion and zero speed. Waveform-aligned PCM grains retain
// texture and protect attacks. All storage is allocated in prepare(); start()
// and next() run on the audio callback.
class SpectralScrub
{
public:
    static constexpr int overlaps = 4;
    static constexpr int maximumFftSize = 8192;

    void prepare(double rate)
    {
        outputRate = std::isfinite(rate) && rate > 0 ? rate : 48000.0;
        // Preserve the analysis duration (and bass resolution) on 96/192 kHz
        // outputs. Storage changes only while the device is being prepared.
        fftOrder = outputRate > 128000.0 ? 13 : outputRate > 64000.0 ? 12 : 11;
        fftSize = 1 << fftOrder;
        hop = fftSize / overlaps;
        ringSize = fftSize * 2;
        jumpFadeFrames = std::clamp(static_cast<int>(std::round(outputRate * 0.02)), 1, fftSize / 2);
        phantom.prepare(outputRate, hop);
        window.resize(fftSize);
        reversed.resize(fftSize);
        twiddles.resize(fftSize / 2);
        for (int i = 0; i < fftSize; ++i)
        {
            window[i] = 0.5f - 0.5f * std::cos(twoPi * static_cast<float>(i) / fftSize);
            int reversedBits = 0;
            for (int bit = 0; bit < fftOrder; ++bit) reversedBits = (reversedBits << 1) | ((i >> bit) & 1);
            reversed[i] = reversedBits;
        }
        for (int i = 0; i < fftSize / 2; ++i)
            twiddles[i] = std::polar(1.0f, -twoPi * static_cast<float>(i) / fftSize);
        for (auto& channel : channels)
        {
            channel.previous.resize(fftSize);
            channel.current.resize(fftSize);
            channel.magnitude.resize(fftSize / 2 + 1);
            channel.targetMagnitude.resize(fftSize / 2 + 1);
            channel.analysisPhase.resize(fftSize / 2 + 1);
            channel.relativePhase.resize(fftSize / 2 + 1);
            channel.targetRelativePhase.resize(fftSize / 2 + 1);
            channel.ring.assign(ringSize, 0.0f);
        }
        referencePhase.resize(fftSize / 2 + 1);
        phase.resize(fftSize / 2 + 1);
        nextPhase.resize(fftSize / 2 + 1);
        frequency.resize(fftSize / 2 + 1);
        targetFrequency.resize(fftSize / 2 + 1);
        strength.resize(fftSize / 2 + 1);
        phaseDiffusion.resize(fftSize / 2 + 1);
        referenceChannel.resize(fftSize / 2 + 1);
        peakForBin.resize(fftSize / 2 + 1);
        active = false;
    }

    void reset() noexcept { active = false; }

    void start(const float* interleaved, int frameCount, double rate, double seconds)
    {
        active = interleaved != nullptr && frameCount > 0 && std::isfinite(rate)
            && rate > 0 && !window.empty();
        if (!active) return;
        samples = interleaved;
        frames = frameCount;
        sourceRate = rate;
        readCursor = 0;
        samplesUntilFrame = hop;
        randomState = 0x85ebca6bu;
        lastTargetFrame = sourceFrame(seconds);
        analysisPosition = lastTargetFrame;
        // A little pre-roll lets the output fade in before a selected attack.
        dryStep = sourceRate / outputRate;
        phantom.start(samples, frames, sourceRate,
                      std::max(0.0, static_cast<double>(lastTargetFrame) - sourceRate * 0.003));
        dryMix = 1.0f;
        // Only an actual onset needs a forward transient. Otherwise a held
        // cursor could play the next note before returning to the frozen one.
        const bool attack = hasAttack(lastTargetFrame);
        dryTargetMix = attack ? 1.0f : 0.0f;
        jumpFadeRemaining = 0;
        attackSamplesLeft = attack ? std::max(1, static_cast<int>(std::round(outputRate * 0.05))) : 0;
        motionHoldSamplesLeft = 0;
        lastAttackFrame = lastTargetFrame;
        dryFadeInStep = 1.0f / static_cast<float>(std::max(1.0, outputRate * 0.006));
        attackFadeInStep = 1.0f / static_cast<float>(std::max(1.0, outputRate * 0.002));
        dryFadeOutStep = 1.0f / static_cast<float>(std::max(1.0, outputRate * 0.012));
        for (auto& channel : channels) std::fill(channel.ring.begin(), channel.ring.end(), 0.0f);
        analyse(seconds, true);
        dryTargetMix = attack ? 1.0f : heldDryMix;
        // Prime the overlap-add ring so the held sound is present immediately.
        for (int frame = 1 - overlaps; frame <= 0; ++frame) synthesize(frame * hop);
    }

    std::pair<float, float> next(double targetSeconds)
    {
        if (!active) return {};
        if (samplesUntilFrame == 0)
        {
            const int wanted = sourceFrame(targetSeconds);
            const bool jump = std::abs(static_cast<double>(wanted) - analysisPosition) > sourceRate * 0.15;
            const bool moved = wanted != lastTargetFrame;
            if (moved)
            {
                const bool attack = hasAttack(wanted)
                    && std::abs(static_cast<double>(wanted) - lastAttackFrame) > sourceRate * 0.025;
                const double dryStart = std::max(0.0, static_cast<double>(wanted) - sourceRate * 0.003);
                phantom.retarget(dryStart, attack);
                motionHoldSamplesLeft = std::max(1, static_cast<int>(std::round(outputRate * 0.035)));
                if (attack)
                {
                    lastAttackFrame = wanted;
                    attackSamplesLeft = std::max(1, static_cast<int>(std::round(outputRate * 0.05)));
                }
                lastTargetFrame = wanted;
            }
            else
            {
                // Mouse updates arrive more slowly than audio hops. Keep the
                // local voice near the held cursor rather than free-running
                // into the next note while waiting for another mouse event.
                if (attackSamplesLeft == 0)
                {
                    if (motionHoldSamplesLeft > 0)
                        phantom.retarget(std::max(0.0, wanted - sourceRate * 0.003), false);
                    else
                    {
                        const double centre = analysisCentre(wanted);
                        // Give the frozen PCM texture 25% more source material.
                        // Keep the spectral window and moving readers unchanged;
                        // the phantom clamps this neighbourhood at file edges.
                        const double halfRange = fftSize * 0.625 * dryStep;
                        phantom.hold(centre - halfRange,
                                     centre + halfRange - dryStep, heldTexture);
                    }
                }
            }
            if (jump) relocate(wanted);
            else
            {
                // The signed input hop follows the cursor exactly. Output hops
                // and oscillator advances stay fixed, so dragging changes time
                // without resampling the notes or accumulating position lag.
                analysisPosition = wanted;
                if (sourceFrame(analysisPosition / sourceRate) != analysedFrame)
                    analyse(analysisPosition / sourceRate, false);
                synthesize(0);
            }
            // Apply the phase vocoder during motion as well as freeze. An
            // all-PCM moving mix would bypass spectral synthesis at each drag.
            dryTargetMix = attackSamplesLeft > 0 ? 1.0f : heldDryMix;
            samplesUntilFrame = hop;
        }
        float spectralLeft = channels[0].ring[readCursor];
        float spectralRight = channels[1].ring[readCursor];
        channels[0].ring[readCursor] = channels[1].ring[readCursor] = 0.0f;
        readCursor = (readCursor + 1) % ringSize;
        --samplesUntilFrame;
        if (jumpFadeRemaining > 0)
        {
            const int index = jumpFadeFrames - jumpFadeRemaining;
            const float weight = fadeWeight(index, jumpFadeFrames);
            spectralLeft = jumpTail[0][index] + (spectralLeft - jumpTail[0][index]) * weight;
            spectralRight = jumpTail[1][index] + (spectralRight - jumpTail[1][index]) * weight;
            --jumpFadeRemaining;
        }
        dryMix += std::clamp(dryTargetMix - dryMix,
                             -dryFadeOutStep, attackSamplesLeft > 0 ? attackFadeInStep : dryFadeInStep);
        const auto dry = phantom.next();
        const float blend = 0.5f - 0.5f * std::cos(0.5f * twoPi * dryMix);
        if (attackSamplesLeft > 0) --attackSamplesLeft;
        if (motionHoldSamplesLeft > 0) --motionHoldSamplesLeft;
        return {spectralLeft + (dry.first - spectralLeft) * blend,
                spectralRight + (dry.second - spectralRight) * blend};
    }

private:
    static constexpr float twoPi = 6.2831853071795864769f;
    static float fadeWeight(int index, int length) noexcept
    {
        return 0.5f - 0.5f * std::cos(0.5f * twoPi * static_cast<float>(index) / length);
    }
    bool hasAttack(int targetFrame) const noexcept
    {
        const int beforeFrames = std::max(1, static_cast<int>(std::round(sourceRate * 0.01)));
        const int afterFrames = std::max(1, static_cast<int>(std::round(sourceRate * 0.008)));
        const auto power = [this](int begin, int length)
        {
            double sum = 0.0;
            int count = 0;
            for (int frame = begin; frame < begin + length; frame += 8)
            {
                if (frame >= 0 && frame < frames)
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        const float sample = samples[frame * 2 + channel];
                        if (std::isfinite(sample)) sum += std::min(static_cast<double>(sample) * sample, 64.0);
                    }
                count += 2;
            }
            return count > 0 ? sum / count : 0.0;
        };
        const double before = power(targetFrame - beforeFrames, beforeFrames);
        const double after = power(targetFrame, afterFrames);
        return after > 0.002 * 0.002 && after > before * 4.0;
    }
    std::pair<float, float> readDry(double readPosition) const noexcept
    {
        return phantom.sampleAt(readPosition);
    }
    struct Channel
    {
        std::vector<std::complex<float>> previous, current;
        std::vector<float> magnitude, targetMagnitude, analysisPhase, relativePhase, targetRelativePhase, ring;
    };

    void relocate(int wanted)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int i = 0; i < jumpFadeFrames; ++i)
            {
                jumpTail[channel][i] = channels[channel].ring[(readCursor + i) % ringSize];
                if (i < jumpFadeRemaining)
                {
                    const int previousIndex = jumpFadeFrames - jumpFadeRemaining + i;
                    const float weight = fadeWeight(previousIndex, jumpFadeFrames);
                    jumpTail[channel][i] = oldJumpTail[channel][previousIndex]
                        + (jumpTail[channel][i] - oldJumpTail[channel][previousIndex]) * weight;
                }
            }
            std::fill(channels[channel].ring.begin(), channels[channel].ring.end(), 0.0f);
        }
        analysisPosition = wanted;
        analyse(analysisPosition / sourceRate, true);
        for (int frame = 1 - overlaps; frame <= 0; ++frame) synthesize(frame * hop);
        jumpFadeRemaining = jumpFadeFrames;
        oldJumpTail = jumpTail;
    }

    int sourceFrame(double seconds) const
    {
        const double safeSeconds = std::isfinite(seconds) ? seconds : 0.0;
        return std::clamp(static_cast<int>(std::round(std::clamp(safeSeconds, 0.0, frames / sourceRate) * sourceRate)),
                          0, frames - 1);
    }

    double analysisCentre(int frame) const noexcept
    {
        const double halfWindow = fftSize / 2 * dryStep;
        const double lastCentre = frames - 1.0 - (fftSize / 2 - 1) * dryStep;
        return lastCentre >= halfWindow ? std::clamp(static_cast<double>(frame), halfWindow, lastCentre)
                                        : static_cast<double>(frame);
    }

    void fft(std::vector<std::complex<float>>& data, bool inverse) const
    {
        for (int i = 0; i < fftSize; ++i)
            if (i < reversed[i]) std::swap(data[i], data[reversed[i]]);
        for (int length = 2; length <= fftSize; length *= 2)
        {
            const int half = length / 2;
            const int stride = fftSize / length;
            for (int base = 0; base < fftSize; base += length)
                for (int index = 0; index < half; ++index)
                {
                    const auto twiddle = inverse ? std::conj(twiddles[index * stride]) : twiddles[index * stride];
                    const auto even = data[base + index];
                    const auto odd = data[base + index + half] * twiddle;
                    data[base + index] = even + odd;
                    data[base + index + half] = even - odd;
                }
        }
        if (inverse)
            for (auto& sample : data) sample /= static_cast<float>(fftSize);
    }

    void analyse(double seconds, bool first)
    {
        // Carry the predicted output phases through a change of peak ownership
        // or stereo reference. Rebase them below before smoothing new offsets.
        if (!first)
            for (auto& channel : channels)
                for (int bin = 0; bin <= fftSize / 2; ++bin)
                    channel.relativePhase[bin] = std::remainder(channel.relativePhase[bin]
                        + phase[peakForBin[bin]], twoPi);
        analysedFrame = sourceFrame(seconds);
        // Fit the analysis window inside the clip at either edge. Repeating
        // the first/last PCM sample outside the file adds DC, halves the level
        // and corrupts the estimated frequency, especially on bass notes.
        const double halfWindow = fftSize / 2 * dryStep;
        const double centre = analysisCentre(analysedFrame);
        const bool forwardProbe = centre < halfWindow + hop * dryStep
            && centre + (fftSize / 2 - 1 + hop) * dryStep <= frames - 1.0;
        const double probeOffset = (forwardProbe ? hop : -hop) * dryStep;
        // Analyze on the output sample grid. The FFT bins then describe the
        // frequencies the output device will synthesize, regardless of the
        // clip's sample rate.
        for (int i = 0; i < fftSize; ++i)
        {
            const double position = centre
                + static_cast<double>(i - fftSize / 2) * dryStep;
            const auto current = readDry(position);
            const auto previous = readDry(position + probeOffset);
            channels[0].current[i] = current.first * window[i];
            channels[1].current[i] = current.second * window[i];
            channels[0].previous[i] = previous.first * window[i];
            channels[1].previous[i] = previous.second * window[i];
        }
        for (auto& channel : channels)
        {
            fft(channel.previous, false);
            fft(channel.current, false);
            for (int bin = 0; bin <= fftSize / 2; ++bin)
            {
                channel.targetMagnitude[bin] = std::abs(channel.current[bin]);
                channel.analysisPhase[bin] = std::arg(channel.current[bin]);
                if (first) channel.magnitude[bin] = channel.targetMagnitude[bin];
            }
        }
        float greatest = 0.0f;
        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            strength[bin] = std::hypot(channels[0].targetMagnitude[bin], channels[1].targetMagnitude[bin]);
            greatest = std::max(greatest, strength[bin]);
        }
        // Energy-weighted local spectral flatness distinguishes sustained
        // partials from breaths, friction and percussion. A global flatness
        // measure would miss band-limited noise. No frequency bins are removed.
        double totalPower = 0.0, texturedPower = 0.0, tonalPower = 0.0;
        phaseDiffusion[0] = phaseDiffusion[fftSize / 2] = 0.0f;
        constexpr int bandBins = 16;
        for (int begin = 1; begin < fftSize / 2; begin += bandBins)
        {
            const int end = std::min(begin + bandBins, fftSize / 2);
            double power = 0.0, logPower = 0.0;
            for (int bin = begin; bin < end; ++bin)
            {
                const double value = static_cast<double>(strength[bin]) * strength[bin];
                power += value;
                logPower += std::log(std::max(value, 1.0e-30));
            }
            totalPower += power;
            const double geometricPower = (end - begin) * std::exp(logPower / (end - begin));
            texturedPower += geometricPower;
            const double flatness = power > 1.0e-12 ? geometricPower / power : 0.0;
            tonalPower += power * std::clamp(1.0 - flatness * 10.0, 0.0, 1.0);
            const float diffusion = static_cast<float>(std::clamp((flatness - 0.1) * 2.5, 0.0, 1.0));
            for (int bin = begin; bin < end; ++bin) phaseDiffusion[bin] = diffusion;
        }
        heldTexture = totalPower > 1.0e-12
            ? static_cast<float>(std::clamp(1.8 * texturedPower / totalPower - 0.03, 0.0, 1.0)) : 0.0f;
        // A small PCM contribution remains even on tonal material; broadband
        // material uses mostly PCM so frozen random peaks do not become tones.
        heldDryMix = 0.09f + 0.83f * std::sqrt(heldTexture);
        // Do not sacrifice voiced partials when noise and a note coexist.
        // Noise-like spectral regions also diffuse in phase below, so they
        // need not be completely replaced with wandering PCM grains.
        if (tonalPower > totalPower * 0.1) heldDryMix = std::min(heldDryMix, 0.35f);
        // The strongest channel supplies each oscillator's frequency. The same
        // oscillator drives L/R, with their original relative phases preserved.
        const float phaseOffset = static_cast<float>((phantom.position() - centre) / dryStep
                                                      + fftSize / 2) / hop;
        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const int strongest = channels[1].targetMagnitude[bin] > channels[0].targetMagnitude[bin] ? 1 : 0;
            const int oldReference = referenceChannel[bin];
            if (first || channels[strongest].targetMagnitude[bin]
                         > 4.0f * channels[oldReference].targetMagnitude[bin])
                referenceChannel[bin] = strongest;
            const auto& channel = channels[referenceChannel[bin]];
            referencePhase[bin] = channel.analysisPhase[bin];
            if (!first && oldReference != referenceChannel[bin])
                phase[bin] = std::remainder(phase[bin] + referencePhase[bin]
                                           - channels[oldReference].analysisPhase[bin], twoPi);
            const float expected = twoPi * static_cast<float>(bin * hop) / fftSize;
            const float probePhase = std::arg(channel.previous[bin]);
            const float phaseDifference = forwardProbe ? probePhase - referencePhase[bin]
                                                       : referencePhase[bin] - probePhase;
            const float difference = std::remainder(phaseDifference - expected, twoPi);
            const float advance = strength[bin] > greatest * 0.001f ? expected + difference : expected;
            targetFrequency[bin] = std::remainder(advance, twoPi);
            if (first)
            {
                frequency[bin] = targetFrequency[bin];
                phase[bin] = std::remainder(referencePhase[bin]
                    + phaseOffset * advance - (overlaps - 1) * frequency[bin], twoPi);
            }
        }
        // Identity phase locking keeps all bins belonging to a spectral peak
        // together, preventing independent sidebands from beating at zero speed.
        int previousPeak = -1;
        for (int bin = 0; bin <= fftSize / 2; ++bin) peakForBin[bin] = bin;
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            if (strength[bin] <= greatest * 0.001f || strength[bin] < strength[bin - 1]
                || strength[bin] <= strength[bin + 1]) continue;
            const int begin = previousPeak < 0 ? 1 : (previousPeak + bin) / 2 + 1;
            if (previousPeak >= 0)
                for (int region = previousPeak + 1; region < begin; ++region) peakForBin[region] = previousPeak;
            for (int region = begin; region <= bin; ++region) peakForBin[region] = bin;
            previousPeak = bin;
        }
        if (previousPeak >= 0)
            for (int bin = previousPeak + 1; bin < fftSize / 2; ++bin) peakForBin[bin] = previousPeak;
        // Random spectral maxima are not sinusoidal partials. Locking a whole
        // noisy region to each such maximum creates a small bank of persistent
        // ringing tones, even if the group's phase is perturbed afterwards.
        // Let noise bins evolve independently, sharing their phase between L/R;
        // retain identity locking in the tonal regions.
        for (int bin = 1; bin < fftSize / 2; ++bin)
            if (phaseDiffusion[bin] > 0.0f) peakForBin[bin] = bin;
        for (auto& channel : channels)
            for (int bin = 0; bin <= fftSize / 2; ++bin)
            {
                channel.targetRelativePhase[bin] = std::remainder(channel.analysisPhase[bin]
                    - referencePhase[peakForBin[bin]], twoPi);
                if (first) channel.relativePhase[bin] = channel.targetRelativePhase[bin];
                else channel.relativePhase[bin] = std::remainder(channel.relativePhase[bin]
                    - phase[peakForBin[bin]], twoPi);
            }
    }

    void synthesize(int offset)
    {
        for (int bin = 0; bin <= fftSize / 2; ++bin)
            frequency[bin] += 0.5f * std::remainder(targetFrequency[bin] - frequency[bin], twoPi);
        for (auto& channel : channels)
        {
            for (int bin = 0; bin <= fftSize / 2; ++bin)
            {
                channel.magnitude[bin] += 0.5f * (channel.targetMagnitude[bin] - channel.magnitude[bin]);
                channel.relativePhase[bin] += 0.5f * std::remainder(channel.targetRelativePhase[bin]
                                                                  - channel.relativePhase[bin], twoPi);
                channel.relativePhase[bin] = std::remainder(channel.relativePhase[bin], twoPi);
                channel.current[bin] = std::polar(channel.magnitude[bin],
                    phase[peakForBin[bin]] + channel.relativePhase[bin]);
                if (bin > 0 && bin < fftSize / 2)
                    channel.current[fftSize - bin] = std::conj(channel.current[bin]);
            }
            channel.current[0] = {channel.current[0].real(), 0.0f};
            channel.current[fftSize / 2] = {channel.current[fftSize / 2].real(), 0.0f};
            fft(channel.current, true);
            for (int i = 0; i < fftSize; ++i)
            {
                const int destination = offset + i;
                if (destination < 0) continue;
                channel.ring[(readCursor + destination) % ringSize] += channel.current[i].real()
                    * window[i] * (8.0f / (3.0f * overlaps));
            }
        }
        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const int peak = peakForBin[bin];
            nextPhase[bin] = std::remainder(phase[peak] + frequency[peak]
                + referencePhase[bin] - referencePhase[peak], twoPi);
        }
        phase.swap(nextPhase);
        // Share each perturbation across the stereo pair and the peak's whole
        // region. Tonal bands receive no diffusion: pitch/phase locking stays
        // intact, while random spectral maxima no longer ring indefinitely.
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            if (peakForBin[bin] != bin || phaseDiffusion[bin] == 0.0f) continue;
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            const float random = static_cast<float>(static_cast<double>(randomState) / 4294967296.0 - 0.5);
            phase[bin] = std::remainder(phase[bin] + random * twoPi * phaseDiffusion[bin], twoPi);
        }
    }

    Channel channels[2];
    PhantomScrub phantom;
    std::array<std::array<float, maximumFftSize / 2>, 2> jumpTail {}, oldJumpTail {};
    std::vector<float> referencePhase, phase, nextPhase, frequency, targetFrequency, strength, phaseDiffusion;
    std::vector<int> referenceChannel, peakForBin;
    std::vector<float> window;
    std::vector<int> reversed;
    std::vector<std::complex<float>> twiddles;
    const float* samples = nullptr;
    int fftOrder = 11, fftSize = 2048, hop = 512, ringSize = 4096;
    int frames = 0, analysedFrame = 0, readCursor = 0, samplesUntilFrame = 0;
    int lastTargetFrame = 0, lastAttackFrame = 0, attackSamplesLeft = 0;
    int motionHoldSamplesLeft = 0;
    int jumpFadeFrames = 1, jumpFadeRemaining = 0;
    double dryStep = 1.0;
    double analysisPosition = 0.0;
    float dryMix = 0.0f, dryTargetMix = 0.0f;
    float heldDryMix = 0.09f, heldTexture = 0.0f;
    float dryFadeInStep = 0.0f, attackFadeInStep = 0.0f, dryFadeOutStep = 0.0f;
    double sourceRate = 48000.0, outputRate = 48000.0;
    bool active = false;
    std::uint32_t randomState = 0x85ebca6bu;
};
}

