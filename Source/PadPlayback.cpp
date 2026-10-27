#include "MainComponent.h"
#if JUCE_WINDOWS
 #include <windows.h>
#endif

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
bool scanDown(const juce::String& code)
{
#if JUCE_WINDOWS
    auto scan = static_cast<UINT>(code.upToFirstOccurrenceOf(":", false, false).getHexValue32());
    if (code.endsWith(":2")) scan |= 0xe000;
    const auto vk = MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
    return vk != 0 && (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
#else
    juce::ignoreUnused(code); return false;
#endif
}
}
void MainComponent::preloadVisiblePage(const std::vector<int>& padIndices)
{
    if (smokeTest) return;
    const auto& pads = library.current().pads;
    const auto count = juce::jmin<int>(6, static_cast<int>(padIndices.size()));
    juce::String pageKey = library.current().id;
    for (int i = 0; i < count; ++i)
    {
        const auto index = padIndices[static_cast<std::size_t>(i)];
        if (index < 0 || index >= static_cast<int>(pads.size())) continue;
        const auto& pad = pads[static_cast<std::size_t>(index)];
        pageKey += "|" + pad.file.getFullPathName() + ":" + juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
    }
    if (pageKey == lastPreloadPage) return;
    lastPreloadPage = pageKey;
    for (int i = 0; i < count; ++i)
    {
        const int index = padIndices[static_cast<std::size_t>(i)];
        if (index < 0 || index >= static_cast<int>(pads.size()) || pads[static_cast<std::size_t>(index)].fileMissing) continue;
        const auto& pad = pads[static_cast<std::size_t>(index)];
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        const auto key = pad.file.getFullPathName() + "|" + revision;
        if (audio.isCached(pad.file, revision) || !preloadingFiles.insert(key).second) continue;
        const auto file = pad.file;
        juce::Component::SafePointer<MainComponent> safe(this);
        preloadLoader.addJob(new PlaybackJob([this, safe, key, file, revision]
        {
            const ClipData* ignored = nullptr;
            const auto result = audio.loadClip(file, ignored, revision);
            juce::MessageManager::callAsync([safe, key, result]
            {
                if (!safe) return;
                safe->preloadingFiles.erase(key);
                juce::ignoreUnused(result);
            });
        }), true);
    }
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
    const auto cached = audio.playCached(id, file, revision, settings);
    if (cached != AudioEngine::PlayResult::notCached) { report(cached); return; }
    pendingPads.insert(index);
    const auto generation = playGeneration;
    juce::Component::SafePointer<MainComponent> safe(this);
    setStatus("Caricamento: " + name);
    playbackLoader.addJob(new PlaybackJob([this, safe, generation, index, id, file, name, settings, revision]
    {
        const ClipData* ignored = nullptr;
        auto result = audio.loadClip(file, ignored, revision, true);
        juce::MessageManager::callAsync([safe, generation, index, id, file, revision, name, settings, result]
        {
            if (!safe) return;
            if (generation != safe->playGeneration)
            {
                if (result.wasOk()) safe->audio.releaseReservation(file, revision);
                return;
            }
            safe->pendingPads.erase(index);
            if (result.failed()) { safe->setStatus(result.getErrorMessage(), true); return; }
            if (settings.mode == PlayMode::holdLoop && !safe->pressedPads.isHeld(index))
            {
                safe->audio.releaseReservation(file, revision);
                safe->setStatus("Suono pronto: " + name);
                return;
            }
            const auto queued = safe->audio.playReserved(id, file, revision, settings);
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
    if (rawKeyboard.enabled() || rawKeyboard.identifying()) return false;
    if (!hasKeyboardFocus(true) || (textEditorFocused() && !binding))
    {
        for (const auto& key : heldKeys) handleKey(key, false);
        heldKeys.clear(); keyboardArmed = false; return false;
    }
    std::set<juce::String> codes;
    for (const auto& row : keyboardLayout()) for (const auto& key : row) codes.insert(key.code);
    for (const auto& assignedKey : keyIndex) codes.insert(assignedKey.first);
    codes.insert(library.djStopKey);
    // Return activates the focused tile; do not also trigger a pad bound to Return.
    if (dynamic_cast<PadTile*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr
        || juce::Component::getCurrentlyFocusedComponent() == &keyboard)
    {
        codes.erase("001C:0");
        if (heldKeys.erase("001C:0") != 0) handleKey("001C:0", false);
    }
    if (!keyboardArmed)
    {
        // Keys already held when focus returns must be released before triggering.
        for (const auto& code : codes) if (scanDown(code)) heldKeys.insert(code);
        keyboardArmed = true; return false;
    }
    for (const auto& code : codes)
    {
        const bool down = scanDown(code); const bool wasDown = heldKeys.count(code) != 0;
        if (down == wasDown) continue;
        if (down) heldKeys.insert(code); else heldKeys.erase(code);
        handleKey(code, down);
    }
    return false;
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
