#include "../Source/AudioEngine.h"
#include <thread>

namespace dualdeck
{
namespace
{
float fixtureSample(int frame, int channel)
{
    const float ramp = static_cast<float>(frame % 4096) / 32768.0f;
    const float marker = static_cast<float>((frame / 4096) % 7) * 0.015625f;
    return channel == 0 ? ramp + marker : -(ramp + marker) * 0.5f;
}
juce::Result writeFixture(const juce::File& file)
{
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
        .withSampleRate(48000).withNumChannels(2).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (!writer) return juce::Result::fail("Streaming fixture writer");
    juce::AudioBuffer<float> block(2, 4096);
    for (int first = 0; first < 144000; first += 4096)
    {
        const int count = std::min(4096, 144000 - first);
        for (int frame = 0; frame < count; ++frame) for (int channel = 0; channel < 2; ++channel)
            block.setSample(channel, frame, fixtureSample(first + frame, channel));
        if (!writer->writeFromAudioSampleBuffer(block, 0, count)) return juce::Result::fail("Streaming fixture write");
    }
    return juce::Result::ok();
}
bool checkRange(const std::shared_ptr<StreamedPcm>& source, int first, int count)
{
    if (!StreamingAudio::waitForRange(source, first, count)) return false;
    PcmReadView view;
    for (int frame = first; frame < first + count; ++frame)
    {
        if (!view.contains(frame)) view = source->acquire(frame);
        float left = 0, right = 0;
        if (!view.read(frame, left, right) || std::abs(left - fixtureSample(frame, 0)) > 1.0e-6f
            || std::abs(right - fixtureSample(frame, 1)) > 1.0e-6f) return false;
    }
    return true;
}
struct WaitingSource final : StreamedPcm
{
    std::array<float, 200> values{};
    bool available = false, broken = false;
    mutable unsigned requests = 0;
    WaitingSource() { values.fill(0.25f); }
    int frameCount() const noexcept override { return 100; }
    double rate() const noexcept override { return 48000; }
    std::size_t prefixBytes() const noexcept override { return 0; }
    PcmReadView acquire(int) const noexcept override { return available ? PcmReadView{values.data(), 0, 100} : PcmReadView{}; }
    void request(int, int) const noexcept override { ++requests; }
    bool failed() const noexcept override { return broken; }
};
}

juce::Result runStreamingTests(const juce::File& tempRoot)
{
    const auto root = tempRoot.getChildFile(juce::Uuid().toString());
    if (root.createDirectory().failed()) return juce::Result::fail("Streaming fixture directory");
    const auto file = root.getChildFile("stream.wav");
    auto result = writeFixture(file);
    if (result.failed()) return result;
    const auto originalSize = file.getSize(), originalModified = file.getLastModificationTime().toMilliseconds();
    {
        StreamingAudio backend(4 * StreamingAudio::pageFrames * 2 * sizeof(float));
        std::shared_ptr<StreamedPcm> source;
        result = backend.prepare(file, 0, source);
        if (result.failed()) return result;
        if (source->frameCount() != 144000 || source->rate() != 48000 || source->prefixBytes() != 192000
            || backend.pageBytes() != 0 || source->acquire(50000).data != nullptr)
            return juce::Result::fail("Preparation decoded more than the half-second head");
        for (int first = 0; first < 144000; first += 8192)
            if (!checkRange(source, first, std::min(8192, 144000 - first)))
                return juce::Result::fail("Incorrect samples across a streamed page boundary: " + juce::String(first));
        if (backend.pageBytes() > 4u * 8192u * 2u * sizeof(float))
            return juce::Result::fail("Streaming page pool exceeded its budget");
        if (!checkRange(source, 96001, 7000) || !checkRange(source, 23997, 1000))
            return juce::Result::fail("Backward/forward reads lost content after page recycling");
        std::shared_ptr<StreamedPcm> trimmed;
        result = backend.prepare(file, 2.0, trimmed);
        if (result.failed() || trimmed->acquire(96000).data == nullptr || trimmed->acquire(0).data != nullptr
            || !checkRange(trimmed, 96000, 100))
            return juce::Result::fail("Head did not start at the selected trim");
    }
    {
        StreamingAudio backend(2 * StreamingAudio::pageFrames * 2 * sizeof(float));
        std::shared_ptr<StreamedPcm> source;
        result = backend.prepare(file, 0, source);
        if (result.failed() || !checkRange(source, 50000, 1)) return juce::Result::fail("Pin fixture preparation");
        auto outputA = source->acquire(50000), outputB = source->acquire(50000);
        for (int frame : {66000, 83000, 101000, 126000})
            if (!checkRange(source, frame, 1)) return juce::Result::fail("Page pool could not recycle its unpinned page");
        float left = 0, right = 0;
        if (!outputA.read(50000, left, right) || left != fixtureSample(50000, 0))
            return juce::Result::fail("Recycling overwrote an output's pinned audio");
        outputA.reset();
        if (!checkRange(source, 141000, 1) || !outputB.read(50000, left, right) || left != fixtureSample(50000, 0))
            return juce::Result::fail("One output released another output's page");
    }
    {
        constexpr std::size_t budget = 8 * StreamingAudio::pageFrames * 2 * sizeof(float);
        StreamingAudio backend(budget);
        std::array<std::shared_ptr<StreamedPcm>, 4> sources;
        for (auto& source : sources)
        {
            result = backend.prepare(file, 0, source);
            if (result.failed()) return result;
        }
        for (int frame : {50000, 83000, 126000, 66000})
            for (const auto& source : sources)
                if (!checkRange(source, frame, 1)) return juce::Result::fail("Sharded page recycling changed samples");
        if (backend.pageBytes() > budget) return juce::Result::fail("Worker shards multiplied the shared budget");
    }
    {
        // Four worker shards, enough space for the entire comparison clip in
        // its shard. Production only keeps a moving read-ahead window.
        StreamingAudio backend(128 * StreamingAudio::pageFrames * 2 * sizeof(float));
        std::shared_ptr<StreamedPcm> source;
        result = backend.prepare(file, 0, source);
        if (result.failed() || !StreamingAudio::waitForRange(source, 0, source->frameCount()))
            return juce::Result::fail("Playback comparison prefetch");
        ClipData streamed; streamed.streamed = source; streamed.sampleRate = 48000;
        ClipData full; full.sampleRate = 48000; full.samples.resize(144000 * 2);
        for (int frame = 0; frame < 144000; ++frame) for (int channel = 0; channel < 2; ++channel)
            full.samples[static_cast<std::size_t>(frame) * 2 + channel] = fixtureSample(frame, channel);
        AudioCore streamedCore, fullCore;
        streamedCore.prepare(48000); fullCore.prepare(48000);
        for (const auto mode : {PlayMode::restart, PlayMode::overlap, PlayMode::holdLoop, PlayMode::toggleLoop})
        {
            streamedCore.resetAfterDeviceStop(); fullCore.resetAfterDeviceStop();
            VoiceSettings settings; settings.mode = mode; settings.trimStart = 0.45; settings.trimEnd = 0.8;
            settings.gain = 0.5f; settings.fadeIn = 0; settings.fadeOut = 0;
            settings.filterMix = 0.25f; settings.delayMix = 0.1f; settings.reverbMix = 0.1f;
            if (!streamedCore.trigger(1, &streamed, settings) || !fullCore.trigger(1, &full, settings))
                return juce::Result::fail("Streaming comparison queue");
            if (mode == PlayMode::overlap)
            { streamedCore.trigger(1, &streamed, settings); fullCore.trigger(1, &full, settings); }
            std::array<float, 256> a{}, b{}, c{}, d{};
            for (int block = 0; block < 210; ++block)
            {
                streamedCore.render(a.data(), b.data(), 256); fullCore.render(c.data(), d.data(), 256);
                for (std::size_t i = 0; i < a.size(); ++i)
                    if (std::abs(a[i] - c[i]) > 1.0e-6f || std::abs(b[i] - d[i]) > 1.0e-6f)
                        return juce::Result::fail("Paged playback changed overlap/loop/effect output");
            }
        }
        if (streamedCore.streamingUnderruns() != 0) return juce::Result::fail("Ready pages caused a playback underrun");
        streamedCore.resetAfterDeviceStop(); fullCore.resetAfterDeviceStop();
    }
    {
        auto waiting = std::make_shared<WaitingSource>();
        ClipData clip; clip.streamed = waiting; clip.sampleRate = 48000;
        AudioCore core; core.prepare(48000); core.trackVoice(1);
        VoiceSettings settings; settings.fadeOut = 0;
        core.trigger(1, &clip, settings);
        std::array<float, 128> left{}, right{}; core.render(left.data(), right.data(), 128);
        if (core.trackedPositionSeconds(1) != 0 || core.streamingUnderruns() == 0 || waiting->requests == 0)
            return juce::Result::fail("Unavailable pages advanced or lost the playhead");
        waiting->available = true; core.render(left.data(), right.data(), 1);
        if (core.trackedPositionSeconds(1) <= 0) return juce::Result::fail("Playback did not resume when its page became ready");
        core.resetAfterDeviceStop();
    }
    {
        const auto changing = root.getChildFile("recover.wav"), temporarilyAway = root.getChildFile("recover-away.wav");
        if (!file.copyFileTo(changing)) return juce::Result::fail("Recovery fixture copy");
        AudioEngine engine;
        const ClipData* clip = nullptr;
        result = engine.preparePlaybackClip(changing, clip, "unchanged");
        if (result.failed()) return result;
        const auto failedSource = clip->streamed;
        if (!changing.moveFileTo(temporarilyAway)) return juce::Result::fail("Recovery fixture temporarily unavailable");
        if (StreamingAudio::waitForRange(failedSource, 50000, 1) || !failedSource->failed())
            return juce::Result::fail("Unavailable stream did not report failure");
        if (!temporarilyAway.moveFileTo(changing)) return juce::Result::fail("Restore recovery fixture");
        if (engine.isPrepared(changing, "unchanged")) return juce::Result::fail("Failed stream still reported ready");
        result = engine.preparePlaybackClip(changing, clip, "unchanged");
        if (result.failed() || clip->streamed == failedSource || !checkRange(clip->streamed, 50000, 10))
            return juce::Result::fail("Restored file with unchanged revision could not be replayed");
    }
    if (file.getSize() != originalSize || file.getLastModificationTime().toMilliseconds() != originalModified)
        return juce::Result::fail("Streaming changed an original file");
    return juce::Result::ok();
}
}
