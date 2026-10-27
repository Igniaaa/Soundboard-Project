#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Library.h"
#include <map>
#include <mutex>
#include <optional>
#include <array>

namespace dualdeck
{
class MicrophoneRing
{
public:
    static constexpr std::uint64_t capacity = 65536;
    MicrophoneRing();
    void reset(double sourceRate);
    void push(const float* left, const float* right, int count) noexcept;
    void read(float* left, float* right, int count, double outputRate) noexcept;
private:
    std::unique_ptr<std::atomic<float>[]> samplesLeft, samplesRight;
    std::atomic<std::uint64_t> written {0};
    double inputRate = 48000.0, readPosition = -1.0;
};
class OutputEndpoint final : private juce::AudioIODeviceCallback
{
public:
    OutputEndpoint();
    ~OutputEndpoint() override;
    juce::StringArray deviceNames();
    juce::StringArray inputDeviceNames();
    juce::Result open(const juce::String& name, const juce::String& inputName = {});
    void setMicrophone(MicrophoneRing* ring, bool isPrimary, std::atomic<float>* gain,
                       std::atomic<bool>* monitorEnabled);
    void close();
    juce::String currentName() const;
    juce::String details() const;
    juce::String takeError();
    bool isOpen() const { return device && device->isOpen(); }
    AudioCore core;
private:
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String&) override;
    std::unique_ptr<juce::AudioIODeviceType> type;
    std::unique_ptr<juce::AudioIODevice> device;
    juce::CriticalSection errorLock;
    juce::String error;
    MicrophoneRing* microphone = nullptr;
    std::atomic<float>* microphoneGain = nullptr;
    std::atomic<bool>* microphoneMonitor = nullptr;
    bool primary = false;
    std::array<float, 8192> microphoneLeft {}, microphoneRight {};
};
class AudioEngine
{
public:
    static constexpr std::size_t defaultCacheLimit = 384u * 1024u * 1024u;
    enum class PlayResult { queued, notCached, invalidTrim, noOutput, queueFull };
    explicit AudioEngine(std::size_t cacheLimitBytes = defaultCacheLimit);
    OutputEndpoint main, monitor;
    juce::Result configure(const juce::String& outputName, const juce::String& monitorName, bool monitorEnabled,
                           const juce::String& inputName = {}, bool microphoneEnabled = false);
    void setMicrophoneGain(float gain) { microphoneGain.store(std::clamp(gain, 0.0f, 1.0f)); }
    void setMicrophoneMonitor(bool enabled) { microphoneMonitor.store(enabled); }
    juce::Result loadClip(const juce::File&, const ClipData*&, const juce::String& revision = {}, bool reserveForPlayback = false); // Background worker only.
    bool isCached(const juce::File&, const juce::String& revision = {});
    PlayResult playCached(std::uint64_t id, const juce::File&, const juce::String& revision, const VoiceSettings&);
    PlayResult playReserved(std::uint64_t id, const juce::File&, const juce::String& revision, const VoiceSettings&);
    void releaseReservation(const juce::File&, const juce::String& revision);
    void release(std::uint64_t id);
    void setScrubTarget(double seconds);
    void stop();
    void pause(bool);
    bool hasMonitor() const { return monitor.isOpen(); }
    std::size_t cacheSize() const { return cacheBytes.load(); }
    ~AudioEngine();
private:
    struct CacheEntry { std::unique_ptr<ClipData> clip; std::uint64_t lastAccess = 0; };
    juce::String keyFor(const juce::File& file, const juce::String& revision) const { return file.getFullPathName() + "|" + revision; }
    bool makeRoom(std::size_t bytes);
    PlayResult queueClip(std::uint64_t id, const ClipData*, const VoiceSettings&, bool consumeReservation);
    std::map<juce::String, CacheEntry> clips;
    std::mutex cacheLock;
    std::atomic<std::size_t> cacheBytes {0};
    std::size_t cacheLimit;
    std::uint64_t accessCounter = 0;
    MicrophoneRing microphone;
    std::atomic<float> microphoneGain {0.7f};
    std::atomic<bool> microphoneMonitor {false};
};
juce::Result decodeWithMediaFoundation(const juce::File&, ClipData&);
}
