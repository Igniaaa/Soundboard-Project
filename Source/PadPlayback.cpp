#include "MainComponent.h"

namespace dualdeck
{
namespace
{
class PlaybackJob final : public juce::ThreadPoolJob
{
public:
    explicit PlaybackJob(std::function<void()> function) : ThreadPoolJob("Decode clip"), work(std::move(function)) {}
    JobStatus runJob() override { work(); return jobHasFinished; }
private:
    std::function<void()> work;
};
}
void MainComponent::preloadProfile(const std::vector<int>& visiblePadIndices)
{
    if (smokeTest) return;
    const auto& pads = library.current().pads;
    std::vector<AudioEngine::PreloadRequest> requests;
    std::set<juce::String> files;
    juce::String plan = library.current().id;
    const auto add = [&](int index, bool priority)
    {
        if (index < 0 || index >= static_cast<int>(pads.size()) || pads[static_cast<std::size_t>(index)].fileMissing) return;
        const auto& pad = pads[static_cast<std::size_t>(index)];
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        const auto key = pad.file.getFullPathName() + "|" + revision + "|" + juce::String(pad.settings.trimStart, 9);
        if (!files.insert(key.toLowerCase()).second) return;
        plan += "|" + key + (priority ? ":priority" : ":background");
        requests.push_back({pad.file, revision, priority, pad.key.isNotEmpty(), pad.settings.trimStart});
    };
    for (std::size_t i = 0; i < pads.size(); ++i) if (pads[i].key.isNotEmpty()) add(static_cast<int>(i), true);
    for (const auto index : visiblePadIndices) add(index, true);
    for (std::size_t i = 0; i < pads.size(); ++i) add(static_cast<int>(i), false);
    if (plan == lastPreloadPlan) return;
    lastPreloadPlan = std::move(plan);
    std::uint64_t generation;
    {
        const std::lock_guard<std::mutex> lock(preloadStateLock);
        generation = preloadGeneration.fetch_add(1) + 1;
        preloadRemaining.store(static_cast<int>(requests.size()));
    }
    preloadLoader.removeAllJobs(true, 0);
    // One bounded worker job, rather than one queued job per library entry.
    preloadLoader.addJob(new PlaybackJob([this, generation, requests = std::move(requests)]
    {
        audio.preloadClips(requests, [this, generation] { return generation != preloadGeneration.load(); },
            [this, generation](int remaining)
            {
                const std::lock_guard<std::mutex> lock(preloadStateLock);
                if (generation == preloadGeneration.load()) preloadRemaining.store(remaining);
            });
    }), true);
}
void MainComponent::triggerPad(int index, bool down)
{
    const auto& pads = library.current().pads;
    if (index < 0 || index >= static_cast<int>(pads.size())) return;
    const auto id = static_cast<std::uint64_t>(index + 1);
    auto settings = pads[index].settings;
    if (down) settings.requestedAtNs = AudioCore::nowNs();
    if (pads[index].useGlobalMode) settings.mode = library.defaultMode;
    if (!down)
    {
        if (pressedPads.releaseNeedsStop(index)) audio.release(id);
        return;
    }
    pressedPads.press(index, settings.mode);
    if (smokeTest) return;
    if (pads[index].fileMissing) { setStatus("File non trovato. Usa Ricollega o Aggiorna cartelle.", true); return; }
    if (settings.trimEnd > 0 && settings.trimStart >= settings.trimEnd) { setStatus("Il taglio iniziale deve precedere la fine.", true); return; }
    const auto file = pads[index].file;
    const auto revision = juce::String(pads[index].modifiedMs) + ":" + juce::String(pads[index].fileSizeBytes);
    const auto name = pads[index].name;
    const auto report = [this, &name](AudioEngine::PlayResult result)
    {
        switch (result)
        {
            case AudioEngine::PlayResult::queued: setStatus("In riproduzione: " + name); break;
            case AudioEngine::PlayResult::invalidTrim: setStatus("Il taglio iniziale supera la durata del file.", true); break;
            case AudioEngine::PlayResult::noOutput: setStatus("Seleziona un'uscita nella scheda Audio e uscite.", true); break;
            case AudioEngine::PlayResult::queueFull: setStatus("Coda audio piena o uscita non disponibile.", true); break;
            case AudioEngine::PlayResult::notCached: break;
        }
    };
    if (pendingPads.count(index)) return;
    const auto cached = audio.playPrepared(id, file, revision, settings);
    if (cached != AudioEngine::PlayResult::notCached) { report(cached); return; }
    pendingPads.insert(index);
    const auto generation = playGeneration;
    juce::Component::SafePointer<MainComponent> safe(this);
    setStatus("Caricamento: " + name);
    playbackLoader.addJob(new PlaybackJob([this, safe, generation, index, id, file, name, settings, revision]
    {
        const ClipData* ignored = nullptr;
        auto result = audio.preparePlaybackClip(file, ignored, revision, settings.trimStart, true);
        juce::MessageManager::callAsync([safe, generation, index, id, file, revision, name, settings, result]
        {
            if (!safe) return;
            if (generation != safe->playGeneration)
            {
                if (result.wasOk()) safe->audio.releasePreparedReservation(file, revision, settings.trimStart);
                return;
            }
            safe->pendingPads.erase(index);
            if (result.failed()) { safe->setStatus(result.getErrorMessage(), true); return; }
            if (settings.mode == PlayMode::holdLoop && !safe->pressedPads.isHeld(index))
            {
                safe->audio.releasePreparedReservation(file, revision, settings.trimStart);
                safe->setStatus("Suono pronto: " + name);
                return;
            }
            const auto queued = safe->audio.playPrepared(id, file, revision, settings, true);
            if (queued == AudioEngine::PlayResult::queued) safe->setStatus("In riproduzione: " + name);
            else if (queued == AudioEngine::PlayResult::invalidTrim) safe->setStatus("Il taglio iniziale supera la durata del file.", true);
            else if (queued == AudioEngine::PlayResult::noOutput) safe->setStatus("Seleziona un'uscita nella scheda Audio e uscite.", true);
            else safe->setStatus("Coda audio piena o uscita non disponibile.", true);
        });
    }), true);
}
void MainComponent::handleKey(const juce::String& key, bool down)
{
    if (binding && down) { bindKey(key); return; }
    if (key == "0001:0") { if (down) stopAll(); return; }
    if (key == library.djStopKey)
    {
        djKeyDown = down; audio.pause(djKeyDown || djMouseDown);
        djButton.setButtonText(djKeyDown || djMouseDown ? "DJ IN PAUSA" : "Pausa DJ"); return;
    }
    if (const auto found = keyIndex.find(key); found != keyIndex.end())
    {
        if (down) selectPad(found->second);
        triggerPad(found->second, down);
    }
}
bool MainComponent::textEditorFocused() const
{
    auto* focus = juce::Component::getCurrentlyFocusedComponent();
    while (focus) { if (dynamic_cast<juce::TextEditor*>(focus)) return true; focus = focus->getParentComponent(); }
    return false;
}
bool MainComponent::keyPressed(const juce::KeyPress&, juce::Component*) { return false; }
bool MainComponent::keyStateChanged(bool, juce::Component*)
{
    if (rawKeyboard.enabled() || rawKeyboard.activating() || rawKeyboard.identifying()) return false;
    if (!hasKeyboardFocus(true) || (textEditorFocused() && !binding))
    {
        for (const auto& key : heldKeys) handleKey(key, false);
        heldKeys.clear();
    }
    return false;
}
bool MainComponent::localKeyIsNavigation(const juce::String& key, const juce::Component* focus, bool shift) const
{
    if (binding) return false;
    if ((key == "001C:0" || key == "001C:2")
        && (dynamic_cast<const PadTile*>(focus) != nullptr || focus == &keyboard)) return true;
    return focus == &keyboard && (key == "004B:2" || key == "004D:2" || key == "0048:2" || key == "0050:2"
        || (key == "0044:0" && shift));
}
void MainComponent::handleLocalKey(const juce::String& key, bool down)
{
    if (rawKeyboard.enabled() || rawKeyboard.activating() || rawKeyboard.identifying()) return;
    if (!down)
    {
        if (heldKeys.erase(key) != 0) handleKey(key, false);
        return;
    }
    if (!hasKeyboardFocus(true) || (textEditorFocused() && !binding)) return;
    auto* focus = juce::Component::getCurrentlyFocusedComponent();
    // Preserve keyboard navigation/activation of the focused graphical controls.
    if (localKeyIsNavigation(key, focus, juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())) return;
    if (!heldKeys.insert(key).second) return;
    handleKey(key, true);
}
void MainComponent::stopAll()
{
    ++playGeneration; pendingPads.clear(); pressedPads.clear(); audio.stop();
    editorPanel.resetPreview();
    editorPlaybackRequested = false;
    editorPlaybackSeen = false;
    editorLoopPreviewRequested = false;
    editorScrubbing = false;
    editorScrubVoiceActive = false;
    audio.main.core.trackVoice(0);
    djMouseDown = false; djKeyDown = false; audio.pause(false); djButton.setButtonText("Pausa DJ");
}
}
