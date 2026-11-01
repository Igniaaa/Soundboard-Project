#include "../Source/WindowsStreamReader.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace dualdeck
{
namespace
{
constexpr int fixtureRate = 48000;

// Distinct stereo ramps and impulses expose channel swaps, stale blocks,
// timestamp rounding, and seeks landing one sample to either side.
int fixtureValue(juce::int64 frame, int channel)
{
    if (frame == 7 || frame == 47999 || frame == 48001 || frame == 95999)
        return channel == 0 ? 28000 : -25000;
    return static_cast<int>((frame * (channel == 0 ? 97 : 173) + channel * 7919) % 50001) - 25000;
}

juce::Result writeFixture(const juce::File& file, int seconds, int channels = 2,
                          bool floatingPoint = false)
{
    auto stream = file.createOutputStream();
    if (!stream) return juce::Result::fail("Cannot create MF streaming fixture");
    const int frames = seconds * fixtureRate;
    const int bytesPerSample = floatingPoint ? 4 : 2;
    const int dataBytes = frames * channels * bytesPerSample;
    stream->write("RIFF", 4); stream->writeInt(36 + dataBytes); stream->write("WAVEfmt ", 8);
    stream->writeInt(16); stream->writeShort(static_cast<short>(floatingPoint ? 3 : 1));
    stream->writeShort(static_cast<short>(channels)); stream->writeInt(fixtureRate);
    stream->writeInt(fixtureRate * channels * bytesPerSample);
    stream->writeShort(static_cast<short>(channels * bytesPerSample));
    stream->writeShort(static_cast<short>(bytesPerSample * 8));
    stream->write("data", 4); stream->writeInt(dataBytes);
    // Keep fixture generation bounded even for the 181-second cap regression.
    std::array<char, 4096 * 2 * 4> chunk{};
    for (int first = 0; first < frames; first += 4096)
    {
        const int count = juce::jmin(4096, frames - first);
        for (int i = 0; i < count; ++i)
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto offset = static_cast<size_t>((i * channels + channel) * bytesPerSample);
                if (floatingPoint)
                {
                    float value = static_cast<float>(fixtureValue(first + i, channel)) / 32768.0f;
                    if (first + i == 37) value = std::numeric_limits<float>::quiet_NaN();
                    if (first + i == 38) value = std::numeric_limits<float>::infinity();
                    juce::uint32 bits = 0;
                    std::memcpy(&bits, &value, sizeof(bits));
                    for (int byte = 0; byte < 4; ++byte)
                        chunk[offset + static_cast<size_t>(byte)] = static_cast<char>(bits >> (byte * 8));
                }
                else
                {
                    const auto value = static_cast<juce::uint16>(fixtureValue(first + i, channel));
                    chunk[offset] = static_cast<char>(value);
                    chunk[offset + 1] = static_cast<char>(value >> 8);
                }
            }
        if (!stream->write(chunk.data(), static_cast<size_t>(count * channels * bytesPerSample)))
            return juce::Result::fail("Cannot write MF streaming fixture");
    }
    stream->flush();
    return stream->getStatus();
}

juce::Result checkRead(juce::AudioFormatReader& reader, juce::int64 start, int count,
                       int sourceChannels = 2, bool sanitised = false)
{
    juce::AudioBuffer<float> output(2, count + 9);
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill(output.getWritePointer(channel), 123.0f, count + 9);
    if (!reader.read(&output, 3, count, start, true, true))
        return juce::Result::fail("MF stream read failed at frame " + juce::String(start));
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < count + 9; ++i)
        {
            const auto frame = start + i - 3;
            const bool outsideDestination = i < 3 || i >= count + 3;
            const bool silent = frame < 0 || frame >= reader.lengthInSamples
                || (sanitised && (frame == 37 || frame == 38));
            const float expected = outsideDestination ? 123.0f : silent ? 0.0f
                : static_cast<float>(fixtureValue(frame, sourceChannels == 1 ? 0 : channel)) / 32768.0f;
            if (std::abs(output.getSample(channel, i) - expected) > 0.000002f
                || !std::isfinite(output.getSample(channel, i)))
                return juce::Result::fail("MF frame/channel alignment or zero-fill mismatch at "
                    + juce::String(frame) + " channel " + juce::String(channel));
        }
    }
    return juce::Result::ok();
}
}

juce::Result runWindowsStreamReaderTests(const juce::File& tempRoot)
{
#if JUCE_WINDOWS
    const auto root = tempRoot.getChildFile("mf-stream-" + juce::Uuid().toString());
    if (auto result = root.createDirectory(); result.failed()) return result;
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup{root};
    const auto stereo = root.getChildFile("stereo.wav");
    if (auto result = writeFixture(stereo, 2); result.failed()) return result;
    const auto originalSize = stereo.getSize();
    const auto originalTime = stereo.getLastModificationTime();
    juce::String error;
    auto reader = createWindowsStreamReader(stereo, error);
    if (!reader) return juce::Result::fail("Direct MF WAV reader unavailable: " + error);
    if (reader->sampleRate != fixtureRate || reader->numChannels != 2
        || !reader->usesFloatingPointData || reader->lengthInSamples != 96000)
        return juce::Result::fail("MF stream metadata/output format incorrect");
    // Full sequential traversal with irregular boundaries catches dropped
    // remainders when an MF block straddles two requested pages.
    for (int start = 0; start < 96000; start += 997)
        if (auto result = checkRead(*reader, start, juce::jmin(997, 96000 - start)); result.failed())
            return result;
    for (const auto start : {48001, 7, 82003, 47999, 32001, 1, 95990, 96000, 98000, -7, 0})
        if (auto result = checkRead(*reader, start, 257); result.failed()) return result;
    // Exercise the low-level nullable-channel contract directly as well.
    std::array<float, 17> right{};
    int* destinations[]{nullptr, reinterpret_cast<int*>(right.data())};
    if (!reader->readSamples(destinations, 2, 0, 48001, static_cast<int>(right.size())))
        return juce::Result::fail("MF nullable-channel read failed");
    for (size_t i = 0; i < right.size(); ++i)
        if (std::abs(right[i] - fixtureValue(48001 + static_cast<juce::int64>(i), 1) / 32768.0f) > 0.000002f)
            return juce::Result::fail("MF nullable-channel read misaligned");
    reader.reset();
    if (stereo.getSize() != originalSize || stereo.getLastModificationTime() != originalTime)
        return juce::Result::fail("MF reader modified the original fixture");

    const auto mono = root.getChildFile("mono.wav");
    if (auto result = writeFixture(mono, 2, 1); result.failed()) return result;
    reader = createWindowsStreamReader(mono, error);
    if (!reader) return juce::Result::fail("MF mono conversion unavailable: " + error);
    if (auto result = checkRead(*reader, 48001, 713, 1); result.failed()) return result;
    reader.reset();

    const auto floats = root.getChildFile("nonfinite.wav");
    if (auto result = writeFixture(floats, 2, 2, true); result.failed()) return result;
    reader = createWindowsStreamReader(floats, error);
    if (!reader) return juce::Result::fail("MF float WAV unavailable: " + error);
    if (auto result = checkRead(*reader, 0, 100, 2, true); result.failed()) return result;
    reader.reset();

    const auto longFile = root.getChildFile("duration-cap.wav");
    if (auto result = writeFixture(longFile, 181); result.failed()) return result;
    reader = createWindowsStreamReader(longFile, error);
    if (!reader) return juce::Result::fail("MF duration cap fixture unavailable: " + error);
    if (reader->lengthInSamples != 180 * fixtureRate)
        return juce::Result::fail("MF reader did not cap duration at 180 seconds");
    if (auto result = checkRead(*reader, 180 * fixtureRate - 13, 100); result.failed()) return result;
    if (auto result = checkRead(*reader, 180 * fixtureRate, 100); result.failed()) return result;
    if (auto result = checkRead(*reader, 17, 100); result.failed()) return result;
    reader.reset();

    const auto invalid = root.getChildFile("invalid.wav");
    invalid.replaceWithText("not a wave file");
    error.clear();
    if (createWindowsStreamReader(invalid, error) || error.isEmpty())
        return juce::Result::fail("MF invalid source did not fail clearly");
    return juce::Result::ok();
#else
    juce::String error;
    if (createWindowsStreamReader(tempRoot.getChildFile("unsupported.wav"), error) || error.isEmpty())
        return juce::Result::fail("Non-Windows MF factory must report unsupported platform");
    return juce::Result::ok();
#endif
}
}
