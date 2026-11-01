#include "AudioEngine.h"
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
juce::Result decodeWithMediaFoundation(const juce::File& file, ClipData& clip, std::size_t maxPcmBytes)
{
#if JUCE_WINDOWS
    if (maxPcmBytes < 2 * sizeof(float)) return juce::Result::fail("Preparazione rinviata: memoria audio occupata.");
    using Microsoft::WRL::ComPtr;
    constexpr DWORD firstAudio = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    struct Runtime
    {
        HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HRESULT media = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
        ~Runtime() { if (SUCCEEDED(media)) MFShutdown(); if (SUCCEEDED(com)) CoUninitialize(); }
    } runtime;
    auto fail = [&file](HRESULT code)
    { return juce::Result::fail("Codec non disponibile o file non valido: " + file.getFileName() + " (0x" + juce::String::toHexString(static_cast<int>(code)) + ")"); };
    if (FAILED(runtime.media)) return fail(runtime.media);
    ComPtr<IMFAttributes> attributes;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) return fail(hr);
    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromURL(file.getFullPathName().toWideCharPointer(), attributes.Get(), &reader);
    if (FAILED(hr)) return fail(hr);
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(firstAudio, TRUE);
    ComPtr<IMFMediaType> desired;
    hr = MFCreateMediaType(&desired);
    if (FAILED(hr)) return fail(hr);
    desired->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    desired->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    desired->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    desired->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    desired->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
    desired->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
    desired->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000);
    hr = reader->SetCurrentMediaType(firstAudio, nullptr, desired.Get());
    if (FAILED(hr)) return fail(hr);
    ComPtr<IMFMediaType> actual;
    hr = reader->GetCurrentMediaType(firstAudio, &actual);
    if (FAILED(hr)) return fail(hr);
    UINT32 rate = 0, channels = 0;
    actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (channels != 2 || rate != 48000) return juce::Result::fail("Il codec non fornisce audio stereo a 48 kHz.");
    clip.sampleRate = rate;
    const auto maxSamples = static_cast<std::size_t>(180) * rate * 2;
    const auto memorySamples = maxPcmBytes / (2 * sizeof(float)) * 2;
    for (;;)
    {
        DWORD flags = 0; ComPtr<IMFSample> sample;
        hr = reader->ReadSample(firstAudio, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) return fail(FAILED(hr) ? hr : E_FAIL);
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) return juce::Result::fail("Il formato audio cambia durante il file.");
        if (sample)
        {
            ComPtr<IMFMediaBuffer> buffer; hr = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(hr)) return fail(hr);
            BYTE* bytes = nullptr; DWORD size = 0; hr = buffer->Lock(&bytes, nullptr, &size);
            if (FAILED(hr)) return fail(hr);
            if (size % 8 != 0)
            { buffer->Unlock(); return juce::Result::fail("Il codec ha fornito un blocco audio non valido."); }
            const auto count = juce::jmin(static_cast<std::size_t>(size / sizeof(float)), maxSamples - clip.samples.size());
            if (count > memorySamples - clip.samples.size())
            { buffer->Unlock(); return juce::Result::fail("Preparazione rinviata: memoria audio occupata."); }
            const auto* values = reinterpret_cast<const float*>(bytes);
            clip.samples.insert(clip.samples.end(), values, values + count); buffer->Unlock();
        }
        // Stop decoding at the boundary, including inside the final block.
        if (clip.samples.size() == maxSamples) break;
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
    }
    if (clip.samples.empty()) return juce::Result::fail("Il file audio e vuoto.");
    for (auto& sample : clip.samples) if (!std::isfinite(sample)) sample = 0;
    return juce::Result::ok();
#else
    juce::ignoreUnused(file, clip, maxPcmBytes);
    return juce::Result::fail("Formato audio non supportato su questa piattaforma.");
#endif
}
}
