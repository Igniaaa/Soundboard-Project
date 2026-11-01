#include "WindowsStreamReader.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

#if JUCE_WINDOWS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#endif

namespace dualdeck
{
#if JUCE_WINDOWS
namespace
{
using Microsoft::WRL::ComPtr;
constexpr DWORD firstAudio = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr juce::int64 rate = 48000;
constexpr juce::int64 ticksPerSecond = 10000000;
constexpr juce::int64 maxFrames = 180 * rate;
// Reject unreasonable/malformed decoder blocks before allocating a copy.
constexpr DWORD maximumBlockBytes = static_cast<DWORD>(4 * rate * 2 * sizeof(float));

struct MediaRuntime
{
    const std::thread::id thread = std::this_thread::get_id();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const HRESULT media = SUCCEEDED(com) ? MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) : com;
    ~MediaRuntime()
    {
        jassert(thread == std::this_thread::get_id());
        if (SUCCEEDED(media)) MFShutdown();
        if (SUCCEEDED(com)) CoUninitialize();
    }
};

struct ScopedVariant
{
    PROPVARIANT value{};
    ~ScopedVariant() { PropVariantClear(&value); }
};

class WindowsStreamReader final : public juce::AudioFormatReader
{
public:
    WindowsStreamReader() : juce::AudioFormatReader(nullptr, "Windows Media Foundation stream") {}

    bool initialise(const juce::File& file, juce::String& error)
    {
        auto fail = [&error](const juce::String& message, HRESULT code = S_OK)
        {
            error = message;
            if (FAILED(code)) error += " (0x" + juce::String::toHexString(static_cast<int>(code)) + ")";
            return false;
        };
        if (FAILED(runtime.com)) return fail("Impossibile inizializzare il lettore audio sul thread di preparazione.", runtime.com);
        if (FAILED(runtime.media)) return fail("Media Foundation non disponibile.", runtime.media);
        HRESULT hr = MFCreateSourceReaderFromURL(file.getFullPathName().toWideCharPointer(), nullptr, &source);
        if (FAILED(hr)) return fail("Codec non disponibile o file audio non valido: " + file.getFileName(), hr);
        hr = source->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
        if (SUCCEEDED(hr)) hr = source->SetStreamSelection(firstAudio, TRUE);
        if (FAILED(hr)) return fail("Il file non contiene un flusso audio leggibile.", hr);

        // Obtain and cap duration before asking the decoder for any PCM.
        ScopedVariant duration;
        hr = source->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration.value);
        if (FAILED(hr)) return fail("Durata audio non disponibile.", hr);
        juce::uint64 durationTicks = 0;
        if (duration.value.vt == VT_UI8) durationTicks = duration.value.uhVal.QuadPart;
        else if (duration.value.vt == VT_I8 && duration.value.hVal.QuadPart > 0)
            durationTicks = static_cast<juce::uint64>(duration.value.hVal.QuadPart);
        else return fail("Metadati della durata audio non validi.");
        const auto cappedTicks = juce::jmin(durationTicks, static_cast<juce::uint64>(180 * ticksPerSecond));
        lengthInSamples = static_cast<juce::int64>((cappedTicks * rate + ticksPerSecond / 2) / ticksPerSecond);
        if (lengthInSamples <= 0 || lengthInSamples > maxFrames) return fail("Il file audio e vuoto o la durata non e valida.");

        ScopedVariant characteristics;
        hr = source->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
                                             MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, &characteristics.value);
        if (FAILED(hr) || characteristics.value.vt != VT_UI4
            || (characteristics.value.ulVal & MFMEDIASOURCE_CAN_SEEK) == 0)
            return fail("Il formato audio non consente l'accesso alle posizioni richieste.", hr);

        ComPtr<IMFMediaType> desired;
        hr = MFCreateMediaType(&desired);
        if (SUCCEEDED(hr)) hr = desired->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (SUCCEEDED(hr)) hr = desired->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        if (SUCCEEDED(hr)) hr = desired->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
        if (SUCCEEDED(hr)) hr = desired->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, static_cast<UINT32>(rate));
        if (SUCCEEDED(hr)) hr = desired->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
        if (SUCCEEDED(hr)) hr = desired->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
        if (SUCCEEDED(hr)) hr = desired->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, static_cast<UINT32>(rate * 8));
        if (SUCCEEDED(hr)) hr = source->SetCurrentMediaType(firstAudio, nullptr, desired.Get());
        if (FAILED(hr)) return fail("Il codec non fornisce audio stereo a 48 kHz.", hr);
        if (!validOutputFormat()) return fail("Il codec ha fornito un formato audio non valido.");
        sampleRate = static_cast<double>(rate);
        numChannels = 2;
        bitsPerSample = 32;
        usesFloatingPointData = true;

        // Prime one bounded block so missing codecs and malformed initial PCM
        // fail at creation instead of yielding a silently unusable reader.
        if (!loadBlock() || block.empty()) return fail("Il codec non riesce a leggere il primo blocco audio.", lastError);
        error.clear();
        return true;
    }

    bool readSamples(int* const* destinations, int destinationChannels, int destinationOffset,
                     juce::int64 startFrame, int frames) override
    {
        if (destinations == nullptr || destinationChannels < 0 || destinationChannels > 2
            || destinationOffset < 0 || startFrame < 0 || frames < 0)
            return false;
        for (int channel = 0; channel < destinationChannels; ++channel)
            if (destinations[channel] != nullptr)
                std::memset(destinations[channel] + destinationOffset, 0, static_cast<size_t>(frames) * sizeof(float));
        if (runtime.thread != std::this_thread::get_id()) { jassertfalse; return false; }
        if (failed) return false;
        if (frames == 0 || startFrame >= lengthInSamples) return true;

        const int wanted = static_cast<int>(juce::jmin(static_cast<juce::int64>(frames), lengthInSamples - startFrame));
        const auto cachedEnd = blockStart + static_cast<juce::int64>(block.size() / 2);
        const bool inBlock = startFrame >= blockStart && startFrame < cachedEnd;
        if (!inBlock && startFrame != nextReadFrame && !seek(startFrame)) return false;
        juce::int64 position = startFrame;
        int written = 0;
        while (written < wanted)
        {
            const auto blockEnd = blockStart + static_cast<juce::int64>(block.size() / 2);
            if (block.empty() || position >= blockEnd)
            {
                if (endOfStream) break;
                if (!loadBlock()) return false;
                continue;
            }
            if (position < blockStart)
            {
                // Presentation gaps (including delayed stream starts) are silence.
                const auto count = static_cast<int>(juce::jmin(static_cast<juce::int64>(wanted - written), blockStart - position));
                position += count;
                written += count;
                continue;
            }
            const int count = static_cast<int>(juce::jmin(static_cast<juce::int64>(wanted - written), blockEnd - position));
            const auto sourceOffset = static_cast<size_t>(position - blockStart) * 2;
            for (int channel = 0; channel < destinationChannels; ++channel)
                if (destinations[channel] != nullptr)
                {
                    auto* out = reinterpret_cast<float*>(destinations[channel]) + destinationOffset + written;
                    for (int frame = 0; frame < count; ++frame)
                        out[frame] = block[sourceOffset + static_cast<size_t>(frame) * 2 + static_cast<size_t>(channel)];
                }
            position += count;
            written += count;
        }
        nextReadFrame = startFrame + wanted;
        return true;
    }

private:
    bool validOutputFormat() const
    {
        ComPtr<IMFMediaType> type;
        if (FAILED(source->GetCurrentMediaType(firstAudio, &type))) return false;
        GUID major{}, subtype{};
        UINT32 channels = 0, sampleRateValue = 0, bits = 0, alignment = 0;
        return SUCCEEDED(type->GetGUID(MF_MT_MAJOR_TYPE, &major)) && IsEqualGUID(major, MFMediaType_Audio)
            && SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) && IsEqualGUID(subtype, MFAudioFormat_Float)
            && SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels)) && channels == 2
            && SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRateValue)) && sampleRateValue == rate
            && SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits)) && bits == 32
            && SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &alignment)) && alignment == 8;
    }

    bool reject(HRESULT error = E_FAIL)
    {
        lastError = error;
        failed = true;
        block.clear();
        return false;
    }

    bool seek(juce::int64 requestedFrame)
    {
        // Seeking to an integer second before the target avoids the ambiguity
        // of 100ns timestamps between adjacent 48kHz samples and gives compressed
        // decoders/resamplers preroll. Timestamped output is trimmed below.
        const auto seconds = juce::jmax(juce::int64{}, requestedFrame / rate - 1);
        ScopedVariant position;
        position.value.vt = VT_I8;
        position.value.hVal.QuadPart = seconds * ticksPerSecond;
        const HRESULT hr = source->SetCurrentPosition(GUID_NULL, position.value);
        if (FAILED(hr)) return reject(hr);
        block.clear();
        endOfStream = false;
        nextReadFrame = requestedFrame;
        return true;
    }

    bool loadBlock()
    {
        block.clear();
        // Bound empty/event-only output so malformed streams cannot spin forever.
        for (int attempts = 0; attempts < 256 && !endOfStream; ++attempts)
        {
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            ComPtr<IMFSample> sample;
            const HRESULT hr = source->ReadSample(firstAudio, 0, nullptr, &flags, &timestamp, &sample);
            if (FAILED(hr)) return reject(hr);
            if ((flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED
                          | MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED | MF_SOURCE_READERF_NEWSTREAM)) != 0)
                return reject(MF_E_INVALIDMEDIATYPE);
            if (!validOutputFormat()) return reject(MF_E_INVALIDMEDIATYPE);
            endOfStream = (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;
            if (!sample) continue;
            const long double framePosition = static_cast<long double>(timestamp) * rate / ticksPerSecond;
            if (framePosition < -static_cast<long double>(maxFrames)
                || framePosition > static_cast<long double>(maxFrames + 4 * rate))
                return reject();
            blockStart = static_cast<juce::int64>(std::llround(framePosition));
            ComPtr<IMFMediaBuffer> buffer;
            HRESULT bufferResult = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(bufferResult)) return reject(bufferResult);
            BYTE* bytes = nullptr;
            DWORD size = 0;
            bufferResult = buffer->Lock(&bytes, nullptr, &size);
            if (FAILED(bufferResult)) return reject(bufferResult);
            struct Unlock { IMFMediaBuffer* buffer; ~Unlock() { buffer->Unlock(); } } unlock{buffer.Get()};
            if (size % 8 != 0 || size > maximumBlockBytes || (size != 0 && bytes == nullptr)) return reject();
            if (size == 0) continue;
            block.resize(size / sizeof(float));
            std::memcpy(block.data(), bytes, size);
            for (auto& value : block) if (!std::isfinite(value)) value = 0.0f;
            return true;
        }
        if (endOfStream) return true;
        return reject();
    }

    // First member: COM/MF outlives every interface and decoded block.
    MediaRuntime runtime;
    ComPtr<IMFSourceReader> source;
    std::vector<float> block;
    juce::int64 blockStart = 0;
    juce::int64 nextReadFrame = 0;
    HRESULT lastError = S_OK;
    bool endOfStream = false;
    bool failed = false;
};
}
#endif

std::unique_ptr<juce::AudioFormatReader> createWindowsStreamReader(const juce::File& file, juce::String& error)
{
#if JUCE_WINDOWS
    if (!file.existsAsFile())
    {
        error = "File audio non trovato: " + file.getFileName();
        return {};
    }
    auto reader = std::make_unique<WindowsStreamReader>();
    if (!reader->initialise(file, error)) return {};
    return reader;
#else
    juce::ignoreUnused(file);
    error = "Lo streaming Media Foundation e disponibile solo su Windows.";
    return {};
#endif
}
}
