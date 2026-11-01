#include "AudioEngine.h"

namespace dualdeck
{
MicrophoneRing::MicrophoneRing()
    : samplesLeft(std::make_unique<std::atomic<float>[]>(capacity)),
      samplesRight(std::make_unique<std::atomic<float>[]>(capacity))
{
    for (std::uint64_t i = 0; i < capacity; ++i)
    { samplesLeft[i].store(0.0f); samplesRight[i].store(0.0f); }
}
void MicrophoneRing::reset(double sourceRate)
{
    inputRate = sourceRate > 0 ? sourceRate : 48000.0;
    readPosition = -1.0;
    written.store(0, std::memory_order_release);
}
void MicrophoneRing::push(const float* left, const float* right, int count) noexcept
{
    if (!left || count <= 0) return;
    const auto start = written.load(std::memory_order_relaxed);
    for (int i = 0; i < count; ++i)
    {
        const auto slot = (start + static_cast<std::uint64_t>(i)) % capacity;
        samplesLeft[slot].store(std::isfinite(left[i]) ? left[i] : 0.0f, std::memory_order_relaxed);
        const float r = right ? right[i] : left[i];
        samplesRight[slot].store(std::isfinite(r) ? r : 0.0f, std::memory_order_relaxed);
    }
    written.store(start + static_cast<std::uint64_t>(count), std::memory_order_release);
}
void MicrophoneRing::read(float* left, float* right, int count, double outputRate) noexcept
{
    const auto rate = outputRate > 0 ? outputRate : 48000.0;
    const auto baseStep = inputRate / rate;
    for (int i = 0; i < count; ++i)
    {
        const auto available = written.load(std::memory_order_acquire);
        const auto delay = inputRate * 0.015;
        if (readPosition < 0 || readPosition + capacity - 2 < static_cast<double>(available))
            readPosition = std::max(0.0, static_cast<double>(available) - delay);
        if (available > static_cast<std::uint64_t>(delay)
            && (static_cast<double>(available) - readPosition < inputRate * 0.003
                || static_cast<double>(available) - readPosition > inputRate * 0.1))
            readPosition = static_cast<double>(available) - delay;
        float l = 0, r = 0;
        if (readPosition + 1 < static_cast<double>(available))
        {
            const auto first = static_cast<std::uint64_t>(readPosition);
            const auto second = first + 1;
            const float fraction = static_cast<float>(readPosition - static_cast<double>(first));
            const float a = samplesLeft[first % capacity].load(std::memory_order_relaxed);
            const float b = samplesLeft[second % capacity].load(std::memory_order_relaxed);
            const float c = samplesRight[first % capacity].load(std::memory_order_relaxed);
            const float d = samplesRight[second % capacity].load(std::memory_order_relaxed);
            l = a + fraction * (b - a); r = c + fraction * (d - c);
            const auto buffered = static_cast<double>(available) - readPosition;
            const auto drift = std::clamp((buffered - delay) / (inputRate * 2.0), -0.005, 0.005);
            readPosition += baseStep * (1.0 + drift);
        }
        left[i] = l; right[i] = r;
    }
}
OutputEndpoint::OutputEndpoint()
{
#if JUCE_WINDOWS
    type.reset(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::shared));
#elif JUCE_MAC
    type.reset(juce::AudioIODeviceType::createAudioIODeviceType_CoreAudio());
#else
    type.reset(juce::AudioIODeviceType::createAudioIODeviceType_ALSA());
#endif
}
OutputEndpoint::~OutputEndpoint() { close(); }
juce::StringArray OutputEndpoint::deviceNames()
{
    if (!type) return {};
    type->scanForDevices();
    return type->getDeviceNames(false);
}
juce::StringArray OutputEndpoint::inputDeviceNames()
{
    if (!type) return {};
    type->scanForDevices();
    return type->getDeviceNames(true);
}
void OutputEndpoint::setMicrophone(MicrophoneRing* ring, bool isPrimary, std::atomic<float>* gain,
                                    std::atomic<bool>* monitorEnabled)
{
    microphone = ring; primary = isPrimary; microphoneGain = gain; microphoneMonitor = monitorEnabled;
}
juce::Result OutputEndpoint::open(const juce::String& requested, const juce::String& inputName)
{
    close();
    auto names = deviceNames();
    if (names.isEmpty()) return juce::Result::fail("Nessuna uscita audio disponibile.");
    const auto index = type->getDefaultDeviceIndex(false);
    const auto selected = requested.isEmpty() ? names[juce::jlimit(0, names.size() - 1, index)] : requested;
    if (!names.contains(selected)) return juce::Result::fail("Uscita non disponibile: " + selected);
    if (inputName.isNotEmpty() && !inputDeviceNames().contains(inputName))
        return juce::Result::fail("Microfono non disponibile: " + inputName);
    device.reset(type->createDevice(selected, inputName));
    if (!device) return juce::Result::fail("Impossibile creare l'uscita " + selected);
    juce::BigInteger channels;
    channels.setRange(0, juce::jmin(2, device->getOutputChannelNames().size()), true);
    juce::BigInteger inputChannels;
    if (inputName.isNotEmpty())
    {
        const auto count = juce::jmin(2, device->getInputChannelNames().size());
        if (count == 0) { close(); return juce::Result::fail("Il dispositivo scelto non ha canali microfono."); }
        inputChannels.setRange(0, count, true);
    }
    const auto rates = device->getAvailableSampleRates();
    const double rate = rates.contains(48000.0) ? 48000.0 : (rates.isEmpty() ? 44100.0 : rates.getFirst());
    auto message = device->open(inputChannels, channels, rate, device->getDefaultBufferSize());
    if (message.isNotEmpty()) { device.reset(); return juce::Result::fail(message); }
    if (device->getActiveOutputChannels().countNumberOfSetBits() < 2)
    { close(); return juce::Result::fail("Questa anteprima richiede un'uscita stereo."); }
    device->start(this);
    return juce::Result::ok();
}
void OutputEndpoint::close()
{
    if (device) { device->stop(); device->close(); device.reset(); }
    core.resetAfterDeviceStop();
}
juce::String OutputEndpoint::currentName() const { return device ? device->getName() : juce::String(); }
juce::String OutputEndpoint::details() const
{
    if (!device) return "Disattivata";
    return juce::String(device->getCurrentSampleRate() / 1000.0, 1) + " kHz  /  "
        + juce::String(device->getCurrentBufferSizeSamples()) + " campioni  /  latenza driver "
        + juce::String(1000.0 * device->getOutputLatencyInSamples() / device->getCurrentSampleRate(), 1) + " ms";
}
void OutputEndpoint::audioDeviceIOCallbackWithContext(const float* const* inputs, int inputChannels,
    float* const* outputs, int channels, int frames, const juce::AudioIODeviceCallbackContext&)
{
    for (int offset = 0; offset < frames; offset += static_cast<int>(microphoneLeft.size()))
    {
        const int count = juce::jmin(frames - offset, static_cast<int>(microphoneLeft.size()));
        auto* left = channels > 0 && outputs[0] ? outputs[0] + offset : nullptr;
        auto* right = channels > 1 && outputs[1] ? outputs[1] + offset : nullptr;
        if (primary && microphone && inputChannels > 0 && inputs && inputs[0])
        {
            const auto* inputLeft = inputs[0] + offset;
            const auto* inputRight = inputChannels > 1 && inputs[1] ? inputs[1] + offset : inputLeft;
            microphone->push(inputLeft, inputRight, count);
            core.render(left, right, count, inputLeft, inputRight,
                        microphoneGain ? microphoneGain->load(std::memory_order_relaxed) : 0.0f);
        }
        else if (!primary && microphone && microphoneMonitor && microphoneMonitor->load(std::memory_order_relaxed))
        {
            microphone->read(microphoneLeft.data(), microphoneRight.data(), count,
                             device ? device->getCurrentSampleRate() : 48000.0);
            core.render(left, right, count, microphoneLeft.data(), microphoneRight.data(),
                        microphoneGain ? microphoneGain->load(std::memory_order_relaxed) : 0.0f);
        }
        else core.render(left, right, count);
    }
    for (int channel = 2; channel < channels; ++channel) if (outputs[channel]) juce::FloatVectorOperations::clear(outputs[channel], frames);
}
void OutputEndpoint::audioDeviceAboutToStart(juce::AudioIODevice* source)
{
    core.prepare(source->getCurrentSampleRate());
    if (primary && microphone) microphone->reset(source->getCurrentSampleRate());
}
void OutputEndpoint::audioDeviceStopped() { core.stop(); }
void OutputEndpoint::audioDeviceError(const juce::String& message)
{
    const juce::ScopedLock lock(errorLock); error = message;
}
juce::String OutputEndpoint::takeError()
{
    const juce::ScopedLock lock(errorLock); auto result = error; error.clear(); return result;
}
AudioEngine::AudioEngine(std::size_t cacheLimitBytes) : cacheLimit(cacheLimitBytes)
{
    main.setMicrophone(&microphone, true, &microphoneGain, &microphoneMonitor);
    monitor.setMicrophone(&microphone, false, &microphoneGain, &microphoneMonitor);
}
AudioEngine::~AudioEngine() { monitor.close(); main.close(); }
juce::Result AudioEngine::configure(const juce::String& outputName, const juce::String& monitorName,
                                    bool monitorEnabled, const juce::String& inputName, bool microphoneEnabled)
{
    if (monitorEnabled && monitorName.isEmpty()) return juce::Result::fail("Scegli il dispositivo cuffie.");
    if (microphoneEnabled && inputName.isEmpty()) return juce::Result::fail("Scegli il microfono fisico.");
    stop(); monitor.close();
    auto result = main.open(outputName, microphoneEnabled ? inputName : juce::String());
    if (result.failed()) return result;
    if (monitorEnabled && monitorName != main.currentName())
    {
        result = monitor.open(monitorName);
        if (result.failed())
        {
            main.close();
            return juce::Result::fail("Uscite non applicate. Cuffie: " + result.getErrorMessage());
        }
    }
    return juce::Result::ok();
}
bool AudioEngine::makeRoom(std::size_t bytes)
{
    if (bytes > cacheLimit) return false;
    while (cacheBytes.load() > cacheLimit - bytes)
    {
        auto victim = clips.end();
        for (auto candidate = clips.begin(); candidate != clips.end(); ++candidate)
            if (candidate->second.clip->activeReferences.load(std::memory_order_acquire) == 0
                && (victim == clips.end() || candidate->second.lastAccess < victim->second.lastAccess))
                victim = candidate;
        if (victim == clips.end()) return false;
        cacheBytes.fetch_sub(victim->second.clip->samples.size() * sizeof(float));
        clips.erase(victim);
    }
    return true;
}
juce::Result AudioEngine::loadClip(const juce::File& file, const ClipData*& result, const juce::String& revision,
                                  bool reserveForPlayback, bool allowEviction)
{
    const auto key = keyFor(file, revision);
    {
        const std::lock_guard<std::mutex> lock(cacheLock);
        if (const auto found = clips.find(key); found != clips.end())
        {
            if (allowEviction || reserveForPlayback) found->second.lastAccess = ++accessCounter;
            result = found->second.clip.get();
            if (reserveForPlayback) result->activeReferences.fetch_add(1, std::memory_order_acq_rel);
            return juce::Result::ok();
        }
    }
    const auto decodeBudget = allowEviction ? cacheLimit : cacheLimit - cacheBytes.load();
    if (decodeBudget < 2 * sizeof(float)) return juce::Result::fail("Preparazione rinviata: memoria audio occupata.");
    if (!file.existsAsFile()) return juce::Result::fail("File non trovato: " + file.getFileName());
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    auto clip = std::make_unique<ClipData>();
    if (!reader)
    {
        const auto decoded = decodeWithMediaFoundation(file, *clip, decodeBudget);
        if (decoded.failed()) return decoded;
    }
    else
    {
        if (!std::isfinite(reader->sampleRate) || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
            return juce::Result::fail("Il file audio e vuoto o ha una frequenza non valida.");
        if (reader->numChannels < 1 || reader->numChannels > 2) return juce::Result::fail("Sono supportati file mono o stereo.");
        // Decode only the playable prefix; the source file stays untouched.
        const auto framesToRead = reader->lengthInSamples / reader->sampleRate > 180.0
            ? static_cast<juce::int64>(reader->sampleRate * 180.0) : reader->lengthInSamples;
        const auto maxFrames = cacheLimit / (2 * sizeof(float));
        if (static_cast<std::uint64_t>(framesToRead) > maxFrames)
            return juce::Result::fail("Il clip supera la cache audio da 384 MiB.");
        if (!allowEviction && static_cast<std::size_t>(framesToRead) * 2 * sizeof(float) > cacheLimit - cacheBytes.load())
            return juce::Result::fail("Preparazione rinviata: memoria audio occupata.");
        clip->sampleRate = reader->sampleRate;
        clip->samples.resize(static_cast<std::size_t>(framesToRead) * 2);
        juce::AudioBuffer<float> chunk(2, 8192);
        for (juce::int64 offset = 0; offset < framesToRead; offset += chunk.getNumSamples())
        {
            const int frames = static_cast<int>(juce::jmin<juce::int64>(chunk.getNumSamples(), framesToRead - offset));
            if (!reader->read(&chunk, 0, frames, offset, true, true)) return juce::Result::fail("Errore di lettura audio.");
            for (int frame = 0; frame < frames; ++frame)
                for (int channel = 0; channel < 2; ++channel)
                {
                    const auto value = chunk.getSample(reader->numChannels == 1 ? 0 : channel, frame);
                    clip->samples[static_cast<std::size_t>(offset + frame) * 2 + channel] = std::isfinite(value) ? value : 0.0f;
                }
        }
    }
    if (!std::isfinite(clip->sampleRate) || clip->sampleRate <= 0 || clip->frames() < 1 || clip->duration() > 180.0)
        return juce::Result::fail("Il clip deve durare tra 0 e 180 secondi.");
    const auto bytes = clip->samples.size() * sizeof(float);
    const std::lock_guard<std::mutex> lock(cacheLock);
    if (const auto found = clips.find(key); found != clips.end())
    {
        if (allowEviction || reserveForPlayback) found->second.lastAccess = ++accessCounter;
        result = found->second.clip.get();
        if (reserveForPlayback) result->activeReferences.fetch_add(1, std::memory_order_acq_rel);
        return juce::Result::ok();
    }
    if (allowEviction ? !makeRoom(bytes) : bytes > cacheLimit - cacheBytes.load())
        return juce::Result::fail("Cache audio piena: i clip in riproduzione occupano la memoria disponibile. Ferma i suoni e riprova.");
    if (reserveForPlayback) clip->activeReferences.fetch_add(1, std::memory_order_relaxed);
    result = clip.get();
    clips.emplace(key, CacheEntry {std::move(clip), ++accessCounter});
    cacheBytes.fetch_add(bytes);
    return juce::Result::ok();
}
juce::Result AudioEngine::preparePlaybackClip(const juce::File& file, const ClipData*& result, const juce::String& revision,
                                             double startSeconds, bool reserveForPlayback, bool allowEviction)
{
    const auto key = preparedKey(file, revision, startSeconds);
    {
        const std::lock_guard<std::mutex> lock(cacheLock);
        if (const auto found = prepared.find(key); found != prepared.end())
        {
            if (!found->second.clip->streamed->failed())
            {
                if (allowEviction || reserveForPlayback) found->second.lastAccess = ++accessCounter;
                result = found->second.clip.get();
                if (reserveForPlayback) result->activeReferences.fetch_add(1, std::memory_order_acq_rel);
                return juce::Result::ok();
            }
            if (found->second.clip->activeReferences.load(std::memory_order_acquire) != 0)
                return juce::Result::fail("Il suono precedente è ancora in arresto. Riprova.");
            prefixBytes.fetch_sub(found->second.clip->streamed->prefixBytes()); prepared.erase(found);
        }
    }
    auto clip = std::make_unique<ClipData>();
    const auto loaded = streaming.prepare(file, startSeconds, clip->streamed);
    if (loaded.failed()) return loaded;
    clip->sampleRate = clip->streamed->rate();
    const auto bytes = clip->streamed->prefixBytes();
    const auto limit = std::min(cacheLimit, static_cast<std::size_t>(96u * 1024u * 1024u));
    const std::lock_guard<std::mutex> lock(cacheLock);
    if (const auto found = prepared.find(key); found != prepared.end())
    {
        if (found->second.clip->streamed->failed()) return juce::Result::fail("Lettura audio interrotta. Riprova.");
        result = found->second.clip.get();
        if (reserveForPlayback) result->activeReferences.fetch_add(1, std::memory_order_acq_rel);
        return juce::Result::ok();
    }
    if (bytes > limit) return juce::Result::fail("Il prefisso audio supera la memoria disponibile.");
    while (prefixBytes.load() > limit - bytes)
    {
        if (!allowEviction) return juce::Result::fail("Preparazione rinviata: memoria iniziale occupata.");
        auto victim = prepared.end();
        for (auto it = prepared.begin(); it != prepared.end(); ++it)
            if (it->second.clip->activeReferences.load(std::memory_order_acquire) == 0
                && (victim == prepared.end() || it->second.lastAccess < victim->second.lastAccess)) victim = it;
        if (victim == prepared.end()) return juce::Result::fail("Memoria iniziale occupata dai suoni attivi.");
        prefixBytes.fetch_sub(victim->second.clip->streamed->prefixBytes()); prepared.erase(victim);
    }
    if (reserveForPlayback) clip->activeReferences.fetch_add(1, std::memory_order_relaxed);
    result = clip.get();
    prepared.emplace(key, CacheEntry{std::move(clip), ++accessCounter}); prefixBytes.fetch_add(bytes);
    return juce::Result::ok();
}
bool AudioEngine::isPrepared(const juce::File& file, const juce::String& revision, double startSeconds)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = prepared.find(preparedKey(file, revision, startSeconds));
    return found != prepared.end() && !found->second.clip->streamed->failed();
}
AudioEngine::PlayResult AudioEngine::playPrepared(std::uint64_t id, const juce::File& file, const juce::String& revision,
                                                const VoiceSettings& settings, bool consumeReservation)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = prepared.find(preparedKey(file, revision, settings.trimStart));
    if (found == prepared.end()) return PlayResult::notCached;
    if (found->second.clip->streamed->failed())
    {
        if (consumeReservation) found->second.clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
        return PlayResult::notCached;
    }
    found->second.lastAccess = ++accessCounter;
    return queueClip(id, found->second.clip.get(), settings, consumeReservation);
}
void AudioEngine::releasePreparedReservation(const juce::File& file, const juce::String& revision, double startSeconds)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = prepared.find(preparedKey(file, revision, startSeconds));
    if (found == prepared.end()) return;
    auto& references = found->second.clip->activeReferences;
    auto count = references.load(std::memory_order_acquire);
    while (count > 0 && !references.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel)) {}
}
void AudioEngine::preloadClips(const std::vector<PreloadRequest>& requests, const std::function<bool()>& cancelled,
                               const std::function<void(int)>& remaining)
{
    struct Pins
    {
        AudioEngine& engine;
        std::vector<PreloadRequest> files;
        ~Pins() { for (const auto& file : files) engine.releasePreparedReservation(file.file, file.revision, file.startSeconds); }
    } pinned{*this, {}};
    int pending = static_cast<int>(requests.size());
    for (const auto& request : requests)
    {
        if (cancelled()) return;
        // Leave tiny fragments unused rather than scanning/decoding the rest
        // of a large library when the background cache is effectively full.
        const ClipData* clip = nullptr;
        const auto result = preparePlaybackClip(request.file, clip, request.revision, request.startSeconds, request.bound, request.priority);
        if (result.wasOk() && request.bound) pinned.files.push_back(request);
        remaining(--pending);
    }
    remaining(0);
}
bool AudioEngine::isCached(const juce::File& file, const juce::String& revision)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = clips.find(keyFor(file, revision));
    if (found == clips.end()) return false;
    found->second.lastAccess = ++accessCounter;
    return true;
}
AudioEngine::PlayResult AudioEngine::queueClip(std::uint64_t id, const ClipData* clip, const VoiceSettings& settings, bool consumeReservation)
{
    if (clip == nullptr) return PlayResult::notCached;
    if (settings.trimEnd > 0 && settings.trimStart >= settings.trimEnd)
    {
        if (consumeReservation) clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
        return PlayResult::invalidTrim;
    }
    if (settings.trimStart >= clip->duration())
    {
        if (consumeReservation) clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
        return PlayResult::invalidTrim;
    }
    if (!main.isOpen())
    {
        if (consumeReservation) clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
        return PlayResult::noOutput;
    }
    const bool mainQueued = main.core.trigger(id, clip, settings);
    const bool monitorQueued = !monitor.isOpen() || monitor.core.trigger(id, clip, settings);
    if (consumeReservation) clip->activeReferences.fetch_sub(1, std::memory_order_acq_rel);
    if (!mainQueued || !monitorQueued)
    {
        stop();
        return PlayResult::queueFull;
    }
    return PlayResult::queued;
}
AudioEngine::PlayResult AudioEngine::playCached(std::uint64_t id, const juce::File& file, const juce::String& revision, const VoiceSettings& settings)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = clips.find(keyFor(file, revision));
    if (found == clips.end()) return PlayResult::notCached;
    found->second.lastAccess = ++accessCounter;
    return queueClip(id, found->second.clip.get(), settings, false);
}
AudioEngine::PlayResult AudioEngine::playReserved(std::uint64_t id, const juce::File& file, const juce::String& revision, const VoiceSettings& settings)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = clips.find(keyFor(file, revision));
    if (found == clips.end()) return PlayResult::notCached;
    found->second.lastAccess = ++accessCounter;
    return queueClip(id, found->second.clip.get(), settings, true);
}
void AudioEngine::releaseReservation(const juce::File& file, const juce::String& revision)
{
    const std::lock_guard<std::mutex> lock(cacheLock);
    const auto found = clips.find(keyFor(file, revision));
    if (found != clips.end())
    {
        auto references = found->second.clip->activeReferences.load(std::memory_order_acquire);
        while (references > 0 && !found->second.clip->activeReferences.compare_exchange_weak(references, references - 1,
            std::memory_order_acq_rel, std::memory_order_acquire)) {}
    }
}
void AudioEngine::release(std::uint64_t id) { main.core.release(id); monitor.core.release(id); }
void AudioEngine::setScrubTarget(double seconds)
{
    main.core.setScrubTarget(seconds);
    monitor.core.setScrubTarget(seconds);
}
void AudioEngine::stop() { main.core.stop(); monitor.core.stop(); }
void AudioEngine::pause(bool value) { main.core.pause(value); monitor.core.pause(value); }
}
