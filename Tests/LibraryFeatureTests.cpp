#include "../Source/MainComponent.h"

namespace dualdeck
{
juce::Result runPreloadTests(const juce::File& tempRoot)
{
    const auto root = tempRoot.getChildFile(juce::Uuid().toString());
    if (root.createDirectory().failed()) return juce::Result::fail("Preload fixture directory");
    const auto source = root.getChildFile("source.wav");
    std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
        .withSampleRate(8000).withNumChannels(1).withBitsPerSample(16));
    juce::AudioBuffer<float> samples(1, 2000); samples.clear();
    if (!writer || !writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()))
        return juce::Result::fail("Preload WAV fixture");
    writer.reset();
    auto board = std::make_unique<MainComponent>(true);
    board->stopTimer(); board->writable = false;
    board->library = Library{}; board->library.ensureProfile();
    auto& profile = board->library.current();
    for (int i = 0; i < 30; ++i)
    {
        Pad pad; pad.id = "preload-" + juce::String(i); pad.name = pad.id;
        pad.file = root.getChildFile(pad.id + ".wav");
        if (!source.copyFileTo(pad.file)) return juce::Result::fail("Copy preload fixture");
        pad.refreshFileInfo();
        if (i == 29) { pad.key = "0047:0"; pad.keyLabel = "Num 7"; }
        profile.pads.push_back(std::move(pad));
    }
    // Use the real keyboard-view scheduling path without opening devices or
    // allowing this isolated test board to write the user's library.
    board->smokeTest = false; board->keyboardMode = true; board->refresh();
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
    while (board->preloadLoader.getNumJobs() != 0 && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::Thread::sleep(1);
    if (board->preloadLoader.getNumJobs() != 0) return juce::Result::fail("Profile preload timed out");
    for (const auto& pad : profile.pads)
    {
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        if (!board->audio.isPrepared(pad.file, revision, pad.settings.trimStart))
            return juce::Result::fail("Keyboard-view first press still needs decoding: " + pad.name);
    }
    AudioEngine bounded(16000);
    const ClipData* ignored = nullptr;
    const auto first = profile.pads[0].file, second = profile.pads[1].file;
    if (bounded.loadClip(first, ignored).failed()
        || bounded.loadClip(second, ignored, {}, false, false).wasOk()
        || !bounded.isCached(first) || bounded.isCached(second) || bounded.cacheSize() > 16000)
        return juce::Result::fail("Background preparation evicted a prepared clip or exceeded its budget");
    if (bounded.loadClip(second, ignored).failed() || bounded.isCached(first) || !bounded.isCached(second))
        return juce::Result::fail("Real playback lost normal cache eviction");
    AudioEngine priorities(16000);
    priorities.preloadClips({{first, {}, true, true}, {second, {}, true, false}}, [] { return false; }, [](int) {});
    if (!priorities.isPrepared(first) || priorities.isPrepared(second))
        return juce::Result::fail("Visible preparation evicted a bound clip that fits the cache");
    if (priorities.preparePlaybackClip(second, ignored).failed())
        return juce::Result::fail("Preparation left a bound clip permanently pinned");
#if JUCE_WINDOWS
    ClipData windowsClip;
    if (decodeWithMediaFoundation(source, windowsClip, 0).wasOk() || !windowsClip.samples.empty())
        return juce::Result::fail("Windows decoder ran with a full preparation cache");
    if (decodeWithMediaFoundation(source, windowsClip, 32000).wasOk() || windowsClip.samples.size() * sizeof(float) > 32000)
        return juce::Result::fail("Windows background decoder exceeded its remaining PCM budget");
#endif
    return juce::Result::ok();
}

juce::Result runPreloadProbe(juce::StringArray& report)
{
    auto board = std::make_unique<MainComponent>(true);
    board->stopTimer(); board->writable = false; board->smokeTest = false;
    board->keyboardMode = true;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    board->refresh();
    while (board->preloadLoader.getNumJobs() != 0 && juce::Time::getMillisecondCounterHiRes() - start < 45000)
        juce::Thread::sleep(1);
    if (board->preloadLoader.getNumJobs() != 0) return juce::Result::fail("Warm-up exceeded 45 seconds");
    int ready = 0, available = 0, boundReady = 0, bound = 0;
    for (const auto& pad : board->library.current().pads)
    {
        if (pad.fileMissing) continue;
        ++available; if (pad.key.isNotEmpty()) ++bound;
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        if (board->audio.isPrepared(pad.file, revision, pad.settings.trimStart)) { ++ready; if (pad.key.isNotEmpty()) ++boundReady; }
    }
    report.add("Background warm-up: " + juce::String(juce::Time::getMillisecondCounterHiRes() - start, 2)
        + " ms; ready " + juce::String(ready) + "/" + juce::String(available)
        + "; bound ready " + juce::String(boundReady) + "/" + juce::String(bound)
        + "; PCM " + juce::String(board->audio.cacheSize() / (1024.0 * 1024.0), 2) + " MiB");
    AudioCore core; core.prepare(48000);
    int measured = 0;
    for (const auto& pad : board->library.current().pads)
    {
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        if (measured >= 6 || !board->audio.isPrepared(pad.file, revision, pad.settings.trimStart)) continue;
        const ClipData* clip = nullptr;
        VoiceSettings settings; settings.trimStart = pad.settings.trimStart; settings.requestedAtNs = AudioCore::nowNs();
        const auto result = board->audio.preparePlaybackClip(pad.file, clip, revision, settings.trimStart, true);
        if (result.failed()) return result;
        const bool queued = core.trigger(++measured, clip, settings);
        board->audio.releasePreparedReservation(pad.file, revision, settings.trimStart);
        float left[1]{}, right[1]{}; core.render(left, right, 1);
        report.add("Prepared first request " + pad.file.getFileName() + ": " + juce::String(core.lastOnsetLatencyMs(), 3) + " ms");
        core.resetAfterDeviceStop();
        if (!queued) return juce::Result::fail("Probe queue full");
    }
    report.add("Software-only measurement; excludes keyboard polling, real device buffers and Discord latency.");
    struct ProbeVoice { const Pad* pad; const ClipData* clip; juce::String revision; };
    std::vector<ProbeVoice> voices;
    for (const auto& pad : board->library.current().pads)
    {
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        if (!board->audio.isPrepared(pad.file, revision, pad.settings.trimStart)) continue;
        const ClipData* clip = nullptr;
        if (board->audio.preparePlaybackClip(pad.file, clip, revision, pad.settings.trimStart, true).wasOk())
            voices.push_back({&pad, clip, revision});
    }
    std::sort(voices.begin(), voices.end(), [](const auto& a, const auto& b)
    { return a.clip->duration() - a.pad->settings.trimStart > b.clip->duration() - b.pad->settings.trimStart; });
    AudioCore monitor; monitor.prepare(44100);
    const auto voiceCount = std::min<std::size_t>(32, voices.size());
    for (std::size_t i = 0; i < voiceCount; ++i)
    {
        VoiceSettings settings; settings.mode = PlayMode::overlap; settings.gain = 0.02f;
        settings.trimStart = voices[i].pad->settings.trimStart;
        core.trigger(i + 1, voices[i].clip, settings); monitor.trigger(i + 1, voices[i].clip, settings);
    }
    for (const auto& voice : voices)
        board->audio.releasePreparedReservation(voice.pad->file, voice.revision, voice.pad->settings.trimStart);
    const auto underrunsBefore = core.streamingUnderruns();
    const auto started = juce::Time::getMillisecondCounterHiRes();
    double maximumCallback = 0, totalCallback = 0;
    double firstMiss = -1, lastMiss = -1;
    std::uint64_t previousMisses = underrunsBefore;
    std::array<float, 128> left{}, right{}, monitorLeft{}, monitorRight{};
    constexpr int probeBlocks = 3750; // Ten seconds, including page-pool recycling.
    for (int block = 0; block < probeBlocks; ++block)
    {
        const auto before = juce::Time::getMillisecondCounterHiRes();
        core.render(left.data(), right.data(), 128);
        monitor.render(monitorLeft.data(), monitorRight.data(), 118);
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - before;
        maximumCallback = std::max(maximumCallback, elapsed); totalCallback += elapsed;
        const auto misses = core.streamingUnderruns() + monitor.streamingUnderruns();
        if (misses != previousMisses)
        {
            lastMiss = juce::Time::getMillisecondCounterHiRes() - started;
            if (firstMiss < 0) firstMiss = lastMiss;
            previousMisses = misses;
        }
        const auto deadline = started + (block + 1) * 128.0 / 48.0;
        while (juce::Time::getMillisecondCounterHiRes() < deadline) juce::Thread::sleep(1);
    }
    const auto underruns = core.streamingUnderruns() - underrunsBefore + monitor.streamingUnderruns();
    report.add("Demand paging / " + juce::String(static_cast<int>(voiceCount)) + " voices / two outputs / 10 seconds: "
        + juce::String(underruns) + " voice-buffer underruns; paired callback avg " + juce::String(totalCallback / probeBlocks, 3)
        + " ms / max " + juce::String(maximumCallback, 3) + " ms; PCM "
        + juce::String(board->audio.cacheSize() / (1024.0 * 1024.0), 2) + " MiB");
    report.add("First/last underrun at " + juce::String(firstMiss, 2) + " / " + juce::String(lastMiss, 2) + " ms; " + board->audio.streamingStatistics());
    core.resetAfterDeviceStop(); monitor.resetAfterDeviceStop();
    const auto streamError = board->audio.takeStreamingError();
    if (streamError.isNotEmpty()) return juce::Result::fail(streamError);
    if (underruns != 0) return juce::Result::fail("Demand paging did not keep up with the probe");
    return board->audio.cacheSize() <= AudioEngine::defaultCacheLimit
        ? juce::Result::ok() : juce::Result::fail("Warm-up exceeded the PCM cache budget");
}

juce::Result runFullKeyboardTests(const juce::File& tempRoot)
{
    std::set<juce::String> codes;
    for (const auto& row : keyboardLayout()) for (const auto& key : row)
        if (key.code.isNotEmpty() && !codes.insert(key.code).second)
            return juce::Result::fail("Duplicate physical key: " + key.code);
    for (const auto* code : {"000E:0", "002B:0", "0036:0", "005B:2", "005C:2", "005D:2",
                            "0037:2", "0046:0", "0045:4", "0052:2", "0053:2", "0047:2",
                            "004F:2", "0049:2", "0051:2", "004B:2", "004D:2", "0048:2", "0050:2",
                            "0045:0", "0035:2", "0037:0", "004A:0", "004E:0", "001C:2",
                            "0047:0", "0048:0", "0049:0", "004B:0", "004C:0", "004D:0",
                            "004F:0", "0050:0", "0051:0", "0052:0", "0053:0"})
        if (codes.count(code) == 0) return juce::Result::fail("Missing full-size key: " + juce::String(code));
    if (codes.size() != 105) return juce::Result::fail("Expected 105 distinct Italian ISO physical keys");
    auto board = std::make_unique<MainComponent>(true);
    board->stopTimer();
    for (const auto* enter : {"001C:0", "001C:2"})
        if (!board->localKeyIsNavigation(enter, board->tiles[0].get(), false)
            || !board->localKeyIsNavigation(enter, &board->keyboard, false)
            || board->localKeyIsNavigation(enter, board.get(), false))
            return juce::Result::fail("Enter could activate both a focused control and a bound pad: " + juce::String(enter));
    if (!board->localKeyIsNavigation("004B:2", &board->keyboard, false)
        || board->localKeyIsNavigation("004B:0", &board->keyboard, false))
        return juce::Result::fail("Numpad key mistaken for a graphical navigation command");
    board->binding = true;
    if (board->localKeyIsNavigation("001C:2", &board->keyboard, false))
        return juce::Result::fail("Numpad Enter cannot be bound while a graphical control is focused");
    board.reset();
    KeyboardView keyboard;
    keyboard.setSize(800, 420);
    keyboard.createComponentSnapshot(keyboard.getLocalBounds());
    if (keyboard.hitboxes.size() != 105) return juce::Result::fail("Not every full-size key can be clicked");
    juce::String assigned;
    keyboard.assign = [&](const juce::String& code) { assigned = code; };
    for (std::size_t i = 0; i < keyboard.hitboxes.size(); ++i)
    {
        const auto& hit = keyboard.hitboxes[i];
        if (!keyboard.getLocalBounds().toFloat().contains(hit.first) || hit.first.getWidth() < 30)
            return juce::Result::fail("Full-size key is clipped or too narrow: " + hit.second);
        for (std::size_t j = i + 1; j < keyboard.hitboxes.size(); ++j)
            if (hit.first.intersects(keyboard.hitboxes[j].first))
                return juce::Result::fail("Overlapping full-size key targets");
        const auto position = hit.first.getCentre();
        const auto now = juce::Time::getCurrentTime();
        keyboard.mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
            juce::ModifierKeys::rightButtonModifier, 1, 0, 0, 0, 0, &keyboard, &keyboard, now, position, now, 1, false));
        if (assigned != hit.second) return juce::Result::fail("Right click assigned the wrong physical key");
        keyboard.keyPressed(juce::KeyPress(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0));
        if (assigned != hit.second) return juce::Result::fail("Accessible assignment differs from mouse assignment");
    }
    Library library; library.ensureProfile();
    for (const auto& row : keyboardLayout()) for (const auto& key : row)
        if (key.code.isNotEmpty())
        {
            Pad pad; pad.id = key.code; pad.name = key.label;
            if (key.code != "0001:0" && key.code != "0058:0" && key.code != library.djStopKey)
            { pad.key = key.code; pad.keyLabel = key.label; }
            pad.file = tempRoot.getChildFile("keyboard-fixture.wav");
            library.current().pads.push_back(std::move(pad));
        }
    const auto file = tempRoot.getChildFile(juce::Uuid().toString()).getChildFile("keyboard.json");
    if (file.getParentDirectory().createDirectory().failed() || library.save(file).failed())
        return juce::Result::fail("Save full keyboard bindings");
    Library loaded;
    if (loaded.load(file).failed() || loaded.current().pads.size() != 105)
        return juce::Result::fail("Load full keyboard bindings");
    for (std::size_t i = 0; i < loaded.current().pads.size(); ++i)
        if (loaded.current().pads[i].key != library.current().pads[i].key
            || loaded.current().pads[i].keyLabel != library.current().pads[i].keyLabel)
            return juce::Result::fail("Full keyboard binding changed after reload");
    return juce::Result::ok();
}

juce::Result runClipDurationTests(const juce::File& tempRoot)
{
    const auto root = tempRoot.getChildFile(juce::Uuid().toString());
    if (root.createDirectory().failed()) return juce::Result::fail("Duration fixture directory");
    struct Case { int sourceFrames, expectedFrames, channels; };
    // 48 kHz fixtures: just below, exactly at, one sample over, and a partial
    // decoder block past 180 seconds. The budget fits only the retained prefix.
    for (const auto test : {Case{8628000, 8628000, 1}, Case{8640000, 8640000, 2},
                            Case{8640001, 8640000, 1}, Case{8652000, 8640000, 2}})
    {
        const auto file = root.getChildFile(juce::String(test.sourceFrames) + ".wav");
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        juce::WavAudioFormat format;
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
            .withSampleRate(48000).withNumChannels(test.channels).withBitsPerSample(16));
        if (!writer) return juce::Result::fail("Duration fixture writer");
        juce::AudioBuffer<float> block(test.channels, 4096);
        for (int channel = 0; channel < test.channels; ++channel)
            for (int frame = 0; frame < block.getNumSamples(); ++frame)
                block.setSample(channel, frame, channel == 0 ? 0.25f : -0.125f);
        for (int offset = 0; offset < test.sourceFrames; offset += block.getNumSamples())
        {
            for (int frame = std::max(0, 8640000 - offset); frame < block.getNumSamples(); ++frame)
                for (int channel = 0; channel < test.channels; ++channel)
                    block.setSample(channel, frame, 0.75f);
            if (!writer->writeFromAudioSampleBuffer(block, 0, std::min(block.getNumSamples(), test.sourceFrames - offset)))
                return juce::Result::fail("Duration fixture write");
        }
        writer.reset();
        juce::MemoryBlock original, after;
        if (!file.loadFileAsData(original)) return juce::Result::fail("Read original duration fixture");
        AudioEngine engine(static_cast<std::size_t>(test.expectedFrames) * 2 * sizeof(float));
        const ClipData* clip = nullptr;
        auto loaded = engine.loadClip(file, clip);
        if (loaded.failed()) return juce::Result::fail("JUCE duration " + juce::String(test.sourceFrames) + ": " + loaded.getErrorMessage());
        if (!clip || clip->frames() != test.expectedFrames || clip->sampleRate != 48000
            || clip->samples.front() != 0.25f || clip->samples[clip->samples.size() - 2] != 0.25f
            || clip->samples.back() != (test.channels == 1 ? 0.25f : -0.125f))
            return juce::Result::fail("JUCE did not retain the expected 180-second prefix / channels");
#if JUCE_WINDOWS
        ClipData windowsClip;
        loaded = decodeWithMediaFoundation(file, windowsClip);
        if (loaded.failed()) return juce::Result::fail("Windows duration: " + loaded.getErrorMessage());
        if (windowsClip.sampleRate != 48000 || windowsClip.frames() != test.expectedFrames
            || std::abs(windowsClip.samples[1000] - 0.25f) > 0.001f
            || std::abs(windowsClip.samples[windowsClip.samples.size() - 1000] - 0.25f) > 0.001f)
            return juce::Result::fail("Windows prefix for " + juce::String(test.sourceFrames)
                + ": got " + juce::String(windowsClip.frames()) + " frames at " + juce::String(windowsClip.sampleRate));
#endif
        if (!file.loadFileAsData(after) || after != original)
            return juce::Result::fail("Duration truncation modified the original file");
    }
    return juce::Result::ok();
}

juce::Result runLibraryFeatureTests(const juce::File& tempRoot)
{
    const auto root = tempRoot.getChildFile(juce::Uuid().toString());
    const auto source = root.getChildFile("external");
    const auto nested = source.getChildFile("nested");
    const auto project = root.getChildFile("project");
    if (nested.createDirectory().failed() || project.createDirectory().failed()) return juce::Result::fail("Fixture folders");
    const auto first = source.getChildFile("sound2.wav");
    const auto second = source.getChildFile("sound10.mp3");
    const auto sameName = nested.getChildFile("sound2.wav");
    if (!first.replaceWithText("first") || !second.replaceWithText("second longer") || !sameName.replaceWithText("nested"))
        return juce::Result::fail("Fixture files");
    source.getChildFile("ignore.txt").replaceWithText("not audio");
    Library library; library.ensureProfile();
    const auto id = library.current().id;
    if (library.globalPitchEnabled || library.globalPitch != 0)
        return juce::Result::fail("Global audio defaults");
    const auto originalName = library.current().name;
    if (library.renameProfile(id, "  ").wasOk() || library.current().name != originalName
        || library.renameProfile("missing", "Name").wasOk()
        || library.renameProfile(id, juce::String::repeatedString("x", 81)).wasOk())
        return juce::Result::fail("Invalid profile names accepted");
    library.profiles.push_back({"other", "Other", {}});
    if (library.renameProfile(id, " other ").wasOk()) return juce::Result::fail("Duplicate profile name accepted");
    library.removeProfile("other");
    if (library.renameProfile(id, "  Suoni personali  ").failed() || library.current().id != id
        || library.current().name != "Suoni personali" || library.activeProfile != 0)
        return juce::Result::fail("Profile rename changed identity or selection");
    library.globalPitchEnabled = true; library.globalPitch = -3.5;
    library.outputVolume = 0.23f; library.monitorVolume = 0.67f;
    const auto scan = scanAudioFolders({source, nested, source}, {});
    if (scan.cancelled || library.mergeFolderScan(id, scan) != 3 || library.totalPads() != 3 || library.current().folders.size() != 2)
        return juce::Result::fail("Recursive scan / overlapping folders / duplicate root");
    if (project.getChildFile("sounds").exists() || first.loadFileAsString() != "first")
        return juce::Result::fail("Scan copied or changed original audio");
    auto& pads = library.current().pads;
    const auto firstIt = std::find_if(pads.begin(), pads.end(), [&](const Pad& p) { return p.file == first; });
    if (firstIt == pads.end()) return juce::Result::fail("External reference missing");
    const auto index = static_cast<std::size_t>(firstIt - pads.begin());
    pads[index].name = "Custom label"; pads[index].key = "001E:0"; pads[index].keyLabel = "A";
    pads[index].settings.gain = 0.37f; pads[index].settings.overrideMasterVolume = true; pads[index].favorite = true;
    pads[index].useGlobalMode = false;
    auto& configured = pads[index].settings;
    configured.mode = PlayMode::holdLoop;
    configured.speed = 1.25; configured.trimStart = 0.12; configured.trimEnd = 0.8;
    configured.fadeIn = 0.03; configured.fadeOut = 0.17;
    configured.filterCutoffHz = 4321; configured.filterMix = 0.32f;
    configured.highPassCutoffHz = 210; configured.highPassMix = 0.24f;
    configured.delayTimeMs = 345; configured.delayFeedback = 0.41f; configured.delayMix = 0.28f;
    configured.reverbDecay = 1.4f; configured.reverbMix = 0.43f;
    configured.compressorEnabled = true; configured.compressorThresholdDb = -18; configured.compressorRatio = 3;
    configured.tremoloEnabled = true; configured.tremoloSpeedHz = 2.5f; configured.tremoloDepth = 0.61f;
    configured.distortionEnabled = true; configured.distortionDrive = 23;
    configured.pingPongTimeMs = 310; configured.pingPongFeedback = 0.26f; configured.pingPongMix = 0.36f;
    configured.chorusSpeedHz = 2.1f; configured.chorusDepth = 0.49f; configured.chorusMix = 0.34f;
    const auto padId = pads[index].id;
    const auto survivor = std::find_if(pads.begin(), pads.end(), [&](const Pad& pad) { return pad.file == second; });
    if (survivor == pads.end()) return juce::Result::fail("Second linked sound missing");
    survivor->key = "0030:0"; survivor->settings.gain = 0.42f;
    const auto survivorId = survivor->id;
    std::vector<juce::File> known;
    for (const auto& pad : pads) known.push_back(pad.file);
    if (library.mergeFolderScan(id, scanAudioFolders({source, nested}, known)) != 0
        || pads[index].id != padId || pads[index].key != "001E:0" || pads[index].name != "Custom label"
        || pads[index].settings.gain != 0.37f || !pads[index].settings.overrideMasterVolume)
        return juce::Result::fail("Rescan duplicated or reset pad settings");
    const auto added = source.getChildFile("new.ogg"); added.replaceWithText("new file");
    if (library.mergeFolderScan(id, scanAudioFolders({source}, known)) != 1 || pads[index].id != padId)
        return juce::Result::fail("New file changed stable indices");
    known.push_back(added);
    const auto temporarilyMissing = source.getChildFile("sound2.wav.fixture-backup");
    const auto recoveryFile = project.getChildFile("recovery/library.json");
    const auto savedPadState = [&](const Library& state) -> juce::String
    {
        if (state.save(recoveryFile).failed()) return {};
        const auto json = juce::JSON::parse(recoveryFile);
        const auto* profiles = json["Profiles"].getArray();
        if (profiles) for (const auto& profile : *profiles)
            if (const auto* entries = profile["Pads"].getArray())
                for (const auto& entry : *entries)
                    if (entry["Id"].toString() == padId) return juce::JSON::toString(entry);
        return {};
    };
    const auto expectedPadState = savedPadState(library);
    if (expectedPadState.isEmpty()) return juce::Result::fail("Save configured pad recovery fixture");
    Library synced = library;
    if (!first.moveFileTo(temporarilyMissing)) return juce::Result::fail("Missing-file fixture");
    auto deletionScan = scanAudioFolders({source, nested}, known);
    auto partialScan = deletionScan; partialScan.partial = true;
    int removed = 0;
    if (synced.mergeFolderScan(id, partialScan, &removed) != 0 || removed != 0 || synced.totalPads() != 4)
        return juce::Result::fail("Partial scan removed a pad");
    if (synced.mergeFolderScan(id, deletionScan, &removed) != 0 || removed != 0 || synced.totalPads() != 4
        || !synced.current().pads[index].fileMissing || savedPadState(synced) != expectedPadState
        || std::none_of(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad)
        { return pad.id == survivorId && pad.key == "0030:0" && pad.settings.gain == 0.42f; }))
        return juce::Result::fail("Missing sound lost its pad identity, binding or configured settings");
    Library restarted;
    if (restarted.load(recoveryFile).failed() || restarted.totalPads() != 4
        || !restarted.current().pads[index].fileMissing || savedPadState(restarted) != expectedPadState)
        return juce::Result::fail("Restart while file is missing lost pad settings");
    synced = std::move(restarted);
    if (synced.mergeFolderScan(id, deletionScan, &removed) != 0 || removed != 0
        || savedPadState(synced) != expectedPadState)
        return juce::Result::fail("Repeated missing-file scan changed pad settings");
    const auto whileMissing = source.getChildFile("added-while-missing.wav");
    if (!whileMissing.replaceWithText("another audio")) return juce::Result::fail("Concurrent import fixture");
    if (synced.mergeFolderScan(id, scanAudioFolders({source, nested}, known), &removed) != 1
        || removed != 0 || synced.totalPads() != 5 || savedPadState(synced) != expectedPadState)
        return juce::Result::fail("Import while another file is missing replaced its settings");
    if (!temporarilyMissing.moveFileTo(first)) return juce::Result::fail("Restore fixture");
    if (synced.mergeFolderScan(id, scanAudioFolders({source, nested}, known), &removed) != 0 || removed != 0
        || synced.totalPads() != 5 || synced.current().pads[index].fileMissing
        || savedPadState(synced) != expectedPadState
        || std::count_if(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad) { return pad.file == first; }) != 1)
        return juce::Result::fail("Returning sound did not recover its original pad and all saved settings");
    Library restoredAfterRestart;
    if (restoredAfterRestart.load(recoveryFile).failed() || restoredAfterRestart.current().pads[index].fileMissing
        || savedPadState(restoredAfterRestart) != expectedPadState)
        return juce::Result::fail("Restored pad settings changed after another restart");
    const auto offlineRoot = root.getChildFile("offline");
    Pad offlinePad; offlinePad.id = "offline-pad"; offlinePad.file = offlineRoot.getChildFile("lost.wav");
    synced.current().pads.push_back(offlinePad);
    const auto offlineScan = scanAudioFolders({offlineRoot}, {offlinePad.file});
    if (offlineScan.warnings.isEmpty() || synced.mergeFolderScan(id, offlineScan, &removed) != 0 || removed != 0
        || std::none_of(synced.current().pads.begin(), synced.current().pads.end(), [&](const Pad& pad) { return pad.id == "offline-pad"; }))
        return juce::Result::fail("Unavailable folder removed a pad");
    const auto cancelled = scanAudioFolders({source}, known, [] { return true; });
    if (!cancelled.cancelled || library.mergeFolderScan(id, cancelled) != 0 || pads.size() != 4)
        return juce::Result::fail("Cancelled scan applied changes");
    const auto unavailable = scanAudioFolders({root.getChildFile("offline")}, known);
    if (unavailable.warnings.isEmpty()) return juce::Result::fail("Unavailable folder not reported");
    library.current().sortField = SortField::modified; library.current().ascending = false;
    const auto saved = project.getChildFile("data/library.json");
    auto result = library.save(saved); if (result.failed()) return result;
    Library loaded; result = loaded.load(saved); if (result.failed()) return result;
    if (!loaded.globalPitchEnabled || loaded.globalPitch != -3.5
        || std::abs(loaded.outputVolume - 0.23f) > 0.00001f || std::abs(loaded.monitorVolume - 0.67f) > 0.00001f
        || loaded.current().name != "Suoni personali" || loaded.current().id != id
        || loaded.current().pads[index].key != "001E:0" || loaded.current().pads[index].settings.gain != 0.37f
        || !loaded.current().pads[index].settings.overrideMasterVolume)
        return juce::Result::fail("Per-pad volume / renamed profile persistence");
    auto settingsJson = juce::JSON::parse(saved);
    settingsJson.getDynamicObject()->setProperty("GlobalPitch", 100);
    const auto bounded = project.getChildFile("data/bounded.json"); bounded.replaceWithText(juce::JSON::toString(settingsJson));
    Library clamped; result = clamped.load(bounded);
    if (result.failed() || clamped.globalPitch != 12) return juce::Result::fail("Global pitch clamp");
    settingsJson.getDynamicObject()->removeProperty("IndividualVolumes");
    settingsJson.getDynamicObject()->removeProperty("GlobalPitchEnabled");
    settingsJson.getDynamicObject()->removeProperty("GlobalPitch");
    settingsJson.getDynamicObject()->setProperty("SchemaVersion", 4);
    bounded.replaceWithText(juce::JSON::toString(settingsJson)); result = clamped.load(bounded);
    if (result.failed() || clamped.current().pads[index].settings.overrideMasterVolume
        || clamped.globalPitchEnabled || clamped.globalPitch != 0)
        return juce::Result::fail("Existing library safe audio defaults");
    if (loaded.current().folders != library.current().folders || loaded.current().sortField != SortField::modified
        || loaded.current().ascending || loaded.current().pads[index].file != first || loaded.current().pads[index].id != padId
        || loaded.current().pads[index].name != "Custom label" || !loaded.current().pads[index].favorite)
        return juce::Result::fail("Folder/settings/order persistence");
    // Missing external references must not be redirected to a same-named local file.
    first.moveFileTo(temporarilyMissing);
    const auto localCollision = project.getChildFile("sounds/sound2.wav");
    localCollision.getParentDirectory().createDirectory(); localCollision.replaceWithText("different audio");
    Library collision; result = collision.load(saved);
    if (result.failed() || collision.current().pads[index].file != first || !collision.current().pads[index].fileMissing)
        return juce::Result::fail("External file redirected to local name collision");
    temporarilyMissing.moveFileTo(first);
    // A modern relative local path must not be redirected to another same-named
    // file while the original is missing, including across a save/reload.
    const auto localFile = project.getChildFile("sounds/nested/sound2.wav");
    const auto localAway = localFile.getSiblingFile("sound2.wav.fixture-backup");
    if (localFile.getParentDirectory().createDirectory().failed() || !localFile.replaceWithText("local nested audio"))
        return juce::Result::fail("Local missing-file fixture");
    Library localLibrary; localLibrary.ensureProfile();
    Pad localPad = pads[index]; localPad.file = localFile; localPad.refreshFileInfo();
    localLibrary.current().pads.push_back(localPad);
    const auto localProfile = localLibrary.current().id;
    const auto ownSounds = project.getChildFile("sounds");
    if (localLibrary.mergeFolderScan(localProfile, scanAudioFolders({ownSounds}, {localFile})) != 1)
        return juce::Result::fail("Local same-name collision fixture");
    const auto expectedLocalState = savedPadState(localLibrary);
    if (expectedLocalState.isEmpty() || !localFile.moveFileTo(localAway)
        || localLibrary.mergeFolderScan(localProfile, scanAudioFolders({ownSounds}, {localFile, localCollision})) != 0
        || !localLibrary.current().pads[0].fileMissing || savedPadState(localLibrary) != expectedLocalState)
        return juce::Result::fail("Local missing pad lost settings before restart");
    Library localRestart;
    if (localRestart.load(recoveryFile).failed() || localRestart.current().pads[0].file != localFile
        || !localRestart.current().pads[0].fileMissing || savedPadState(localRestart) != expectedLocalState)
        return juce::Result::fail("Missing relative local file was redirected to a same-named file after restart");
    if (!localAway.moveFileTo(localFile)
        || localRestart.mergeFolderScan(localProfile, scanAudioFolders({ownSounds}, {localFile, localCollision})) != 0
        || localRestart.current().pads[0].fileMissing || savedPadState(localRestart) != expectedLocalState)
        return juce::Result::fail("Returning local file did not recover its configured pad");
    Profile sortProfile;
    const std::array<juce::String, 4> names {"Sound 10", "sound 2", "Alpha", "Alpha"};
    const std::array<juce::String, 4> extensions {"wav", "mp3", "ogg", "aif"};
    const std::array<juce::int64, 4> sizes {300, 100, 200, 200};
    for (int i = 0; i < 4; ++i)
    {
        Pad pad; pad.id = juce::String(i); pad.name = names[i];
        pad.file = source.getChildFile(juce::String(i) + "." + extensions[i]);
        pad.fileSizeBytes = sizes[i]; pad.modifiedMs = sizes[i]; pad.favorite = i == 1;
        sortProfile.pads.push_back(pad);
    }
    const auto expect = [&](SortField field, bool ascending, std::vector<int> order)
    {
        sortProfile.sortField = field; sortProfile.ascending = ascending;
        if (orderedPads(sortProfile) != order) return false;
        const auto numbers = padDisplayNumbers(sortProfile);
        for (std::size_t rank = 0; rank < order.size(); ++rank)
            if (numbers[static_cast<std::size_t>(order[rank])] != static_cast<int>(rank + 1)
                || orderedPads(sortProfile, "#" + juce::String(rank + 1)) != std::vector<int>({order[rank]}))
                return false;
        return true;
    };
    if (!expect(SortField::index, true, {0,1,2,3}) || !expect(SortField::index, false, {3,2,1,0})
        || !expect(SortField::name, true, {2,3,1,0}) || !expect(SortField::name, false, {0,1,2,3})
        || !expect(SortField::modified, true, {1,2,3,0}) || !expect(SortField::modified, false, {0,2,3,1})
        || !expect(SortField::size, true, {1,2,3,0}) || !expect(SortField::size, false, {0,2,3,1})
        || !expect(SortField::type, true, {3,1,2,0}) || !expect(SortField::type, false, {0,2,1,3}))
        return juce::Result::fail("Sort criteria / directions / stable ties");
    if (orderedPads(sortProfile, "sound") != std::vector<int>({0,1})
        || orderedPads(sortProfile, {}, true) != std::vector<int>({1})
        || orderedPads(sortProfile, "#3") != std::vector<int>({1})
        || orderedPads(sortProfile, "#3", true) != std::vector<int>({1})
        || !orderedPads(sortProfile, "#1", true).empty()
        || !orderedPads(sortProfile, "#0").empty() || !orderedPads(sortProfile, "#999").empty()
        || padDisplayNumbers(library.current()) != padDisplayNumbers(loaded.current())
        || !padDisplayNumbers(Profile {}).empty()
        || sortProfile.pads[0].id != "0")
        return juce::Result::fail("Filter or sort changed pad identities");
    Profile grouped;
    const auto folderA = root.getChildFile("group2/Clips"), folderB = root.getChildFile("group10/Clips");
    for (int i = 0; i < 27; ++i)
    {
        Pad pad; pad.id = juce::String(i); pad.name = "Clip " + juce::String(i);
        pad.file = (i == 1 ? folderB : folderA).getChildFile(pad.name + ".wav");
        pad.favorite = i == 1 || i == 26;
        grouped.pads.push_back(pad);
    }
    const auto groupedPages = folderPadPages(grouped, orderedPads(grouped));
    if (groupedPages.size() != 3 || groupedPages[0].folder != folderA || groupedPages[1].folder != folderA
        || groupedPages[2].folder != folderB || groupedPages[0].indices.size() != 24
        || groupedPages[1].indices != std::vector<int>({25,26}) || groupedPages[2].indices != std::vector<int>({1}))
        return juce::Result::fail("Folder pages mixed directories, lost missing files or changed stable indices");
    grouped.ascending = false;
    const auto descendingPages = folderPadPages(grouped, orderedPads(grouped), folderA);
    const auto favoritePages = folderPadPages(grouped, orderedPads(grouped, {}, true));
    const auto searchedPages = folderPadPages(grouped, orderedPads(grouped, "#26"));
    const auto groupedNumbers = padDisplayNumbers(grouped);
    if (descendingPages.size() != 2 || descendingPages[0].indices.front() != 26
        || descendingPages[1].indices != std::vector<int>({2,0})
        || favoritePages.size() != 2 || favoritePages[0].indices != std::vector<int>({26})
        || favoritePages[1].indices != std::vector<int>({1})
        || searchedPages.size() != 1 || searchedPages[0].folder != folderB
        || !folderPadPages(grouped, orderedPads(grouped, "#26"), folderA).empty()
        || groupedNumbers[26] != 1 || groupedNumbers[25] != 2 || groupedNumbers[1] != 26
        || groupedNumbers[2] != 25 || groupedNumbers[0] != 27
        || !folderPadPages(grouped, {}).empty())
        return juce::Result::fail("Folder selection, search, favorites, sorting or empty pages");
    loaded.profiles.push_back({"second", "Second", {}});
    loaded.profiles.push_back({"third", "Third", {}});
    loaded.activeProfile = 2;
    if (!loaded.removeProfile("second") || loaded.current().id != "third" || loaded.activeProfile != 1)
        return juce::Result::fail("Deleting preceding inactive profile changed selection");
    if (!loaded.removeProfile("third") || loaded.current().id != id || !first.existsAsFile())
        return juce::Result::fail("Deleting active profile or audio preservation");
    if (!loaded.removeProfile(id) || loaded.profiles.size() != 1 || !loaded.current().pads.empty()
        || !loaded.current().folders.empty() || !first.existsAsFile() || loaded.removeProfile("absent")
        || loaded.mergeFolderScan(id, scan) != 0)
        return juce::Result::fail("Last profile replacement / stale scan after deletion");
    return juce::Result::ok();
}

juce::Result runPadOrderingUiTests()
{
    auto board = std::make_unique<MainComponent>(true);
    board->stopTimer();
    board->library = Library {};
    board->library.ensureProfile();
    auto& profile = board->library.current();
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("DualDeck-order-fixture");
    for (int i = 0; i < 30; ++i)
    {
        Pad pad; pad.id = "order-" + juce::String(i); pad.name = "Sound " + juce::String(30 - i);
        pad.file = folder.getChildFile(pad.name + ".wav"); pad.favorite = i == 0;
        if (i == 0) { pad.key = "003B:0"; pad.keyLabel = "F1"; }
        profile.pads.push_back(pad);
    }
    board->refresh(); board->selectPad(0);
    board->keyboard.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)); // Focus F1.
    board->sortBy.setSelectedId(2, juce::sendNotificationSync);
    if (board->tiles[0]->index != 29 || board->tiles[0]->displayNumber != 1
        || board->tiles[0]->getTitle() != "Pad 1: Sound 1"
        || board->padHeading.getText() != "SUONO #30"
        || !board->keyboard.getDescription().startsWith("Associato al pad 30: Sound 30."))
        return juce::Result::fail("Name sort did not renumber tiles, details or keyboard accessibility");
    board->page = 1; board->rebuildTiles();
    if (board->tiles[0]->index != 5 || board->tiles[0]->displayNumber != 25)
        return juce::Result::fail("Pad numbers restarted on the second page");
    board->sortDirection.onClick();
    if (board->page != 0 || board->tiles[0]->index != 0 || board->tiles[0]->displayNumber != 1
        || board->padHeading.getText() != "SUONO #1" || board->keyboard.padNumber(0) != 1
        || !board->keyboard.getDescription().startsWith("Associato al pad 1: Sound 30."))
        return juce::Result::fail("Descending sort did not update the selected pad number");
    board->search.setText("#25", false); board->rebuildTiles();
    if (board->filtered != std::vector<int>({24}) || board->tiles[0]->displayNumber != 25)
        return juce::Result::fail("Index search disagrees with displayed pad number");
    board->search.clear(); board->favoritesOnly.setToggleState(true, juce::dontSendNotification);
    board->sortDirection.onClick();
    if (board->filtered != std::vector<int>({0}) || board->tiles[0]->displayNumber != 30)
        return juce::Result::fail("Favorite filtering changed the full-profile number");
    board->favoritesOnly.setToggleState(false, juce::dontSendNotification);
    profile.pads[0].name = "A first"; board->rebuildTiles();
    if (board->tiles[0]->index != 0 || board->tiles[0]->displayNumber != 1
        || board->padHeading.getText() != "SUONO #1"
        || profile.pads[0].id != "order-0" || profile.pads[0].key != "003B:0")
        return juce::Result::fail("Renaming did not update numbering or changed the pad binding");
    profile.pads[29].useGlobalMode = false; profile.pads[29].settings.mode = PlayMode::holdLoop;
    board->tiles[1]->press(board->tiles[1]->index, true);
    if (!board->pressedPads.isHeld(29) || board->pressedPads.isHeld(1))
        return juce::Result::fail("Display number was used as a playback index");
    board->tiles[1]->press(board->tiles[1]->index, false);
    if (board->pressedPads.isHeld(29)) return juce::Result::fail("Sorted pad release used the wrong index");
    return juce::Result::ok();
}
}
