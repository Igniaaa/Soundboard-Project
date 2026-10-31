#include "MainComponent.h"
#include "KeyboardBridge.h"
#if JUCE_WINDOWS
 #include <windows.h>
 #include <dwmapi.h>
#endif

namespace dualdeck
{
class DeckWindow final : public juce::DocumentWindow
{
public:
    explicit DeckWindow(bool smoke, bool uiEditor) : DocumentWindow("DualDeck - JUCE Edition", palette::background, allButtons)
    {
        setUsingNativeTitleBar(true);
        auto content = std::make_unique<juce::Viewport>();
        deck = new MainComponent(smoke, uiEditor);
        content->setViewedComponent(deck, true);
        viewport = content.get();
        setContentOwned(content.release(), true);
        setResizable(true, false); setResizeLimits(960, 640, 2400, 1600);
        centreWithSize(1380, 880); setVisible(!smoke);
        if (!smoke) matchNativeTitleBar();
    }
    MainComponent& board() const { return *deck; }
    void resized() override
    {
        DocumentWindow::resized();
        if (viewport != nullptr && deck != nullptr)
        {
            const int width = juce::jmax(900, viewport->getWidth() - 20);
            deck->setSize(width, juce::jmax(deck->preferredHeight(width), viewport->getHeight()));
        }
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
private:
    void matchNativeTitleBar()
    {
#if JUCE_WINDOWS
        const auto hwnd = static_cast<HWND>(getWindowHandle());
        if (hwnd == nullptr) return;
        const BOOL dark = TRUE;
        ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        const COLORREF caption = RGB(palette::background.getRed(), palette::background.getGreen(), palette::background.getBlue());
        const COLORREF text = RGB(palette::text.getRed(), palette::text.getGreen(), palette::text.getBlue());
        ::DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
        ::DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
#endif
    }
    juce::Viewport* viewport = nullptr;
    MainComponent* deck = nullptr;
};
class DualDeckApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "DualDeck JUCE"; }
    const juce::String getApplicationVersion() override { return "0.7.0"; }
    bool moreThanOneInstanceAllowed() override
    {
        const auto command = getCommandLineParameters();
        return command.contains("--self-test") || command.contains("--audio-smoke")
            || command.contains("--keyboard-broker") || command.contains("--keyboard-driver-status")
            || command.contains("--keyboard-smoke") || command.contains("--smoke-test")
            || command.contains("--performance-test") || command.contains("--interaction-smoke")
            || command.contains("--ui-editor-snapshot");
    }
    void initialise(const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens(commandLine, true); args.removeEmptyStrings();
        for (auto& arg : args) arg = arg.unquoted();
        if (args.size() > 0 && args[0] == "--keyboard-broker")
        {
            const int result = args.size() == 4
                ? runKeyboardBroker(static_cast<std::uintptr_t>(args[1].getLargeIntValue()),
                                    static_cast<std::uintptr_t>(args[2].getLargeIntValue()),
                                    juce::File(args[3]))
                : 2;
            setApplicationReturnValue(result); quit(); return;
        }
        if (args.size() > 0 && args[0] == "--keyboard-driver-status")
        {
            const auto destination = args.size() > 1 ? juce::File(args[1])
                : findProjectRoot().getChildFile("test-output/keyboard-driver-status.json");
            destination.getParentDirectory().createDirectory();
            const auto result = writeKeyboardDriverStatus(KeyboardBridge::runtimeLibrary(), destination);
            if (result.failed() && !destination.existsAsFile())
                destination.replaceWithText(result.getErrorMessage());
            setApplicationReturnValue(result.wasOk() ? 0 : 1); quit(); return;
        }
        if (args.contains("--self-test"))
        {
            auto root = findProjectRoot();
            const auto resultDirectory = args.size() > 1 ? juce::File(args[1]) : root.getChildFile("test-output");
            resultDirectory.createDirectory();
            juce::StringArray report;
            auto result = runLibraryTests(resultDirectory.getChildFile("fixtures"));
            report.add("Library load / round trip / invalid input: " + (result.wasOk() ? juce::String("PASS") : result.getErrorMessage()));
            int failures = result.failed() ? 1 : 0;
            result = runLibraryFeatureTests(resultDirectory.getChildFile("catalog-fixtures"));
            report.add("Folders / deduplication / ordering / profile removal: " + (result.wasOk() ? juce::String("PASS") : result.getErrorMessage()));
            if (result.failed()) ++failures;
            const auto currentFile = root.getChildFile("data/library.json");
            if (currentFile.existsAsFile())
            {
                Library existing;
                result = existing.load(currentFile);
                report.add("Existing library: " + juce::String(existing.totalPads()) + " pads; " + (result.wasOk() ? juce::String("PASS") : result.getErrorMessage()));
                if (result.failed()) ++failures;
                std::set<juce::String> formats;
                AudioEngine engine;
                int missing = 0;
                bool cacheChecked = false;
                for (const auto& profile : existing.profiles) for (const auto& pad : profile.pads)
                {
                    if (!pad.file.existsAsFile()) { ++missing; continue; }
                    const auto extension = pad.file.getFileExtension().toLowerCase();
                    if (!formats.insert(extension).second) continue;
                    const ClipData* clip = nullptr; result = engine.loadClip(pad.file, clip);
                    report.add("Decode " + extension + ": " + (result.wasOk() ? juce::String("PASS (") + juce::String(clip->duration(), 2) + " s)" : result.getErrorMessage()));
                    if (result.failed()) ++failures;
                    if (result.wasOk() && !cacheChecked)
                    {
                        cacheChecked = true;
                        const bool initiallyAbsent = !engine.isCached(pad.file, "updated-file");
                        const ClipData* updated = nullptr;
                        const auto reload = engine.loadClip(pad.file, updated, "updated-file");
                        const bool ok = initiallyAbsent && reload.wasOk() && updated != clip
                            && engine.isCached(pad.file) && engine.isCached(pad.file, "updated-file");
                        report.add("Updated-file cache revision: " + juce::String(ok ? "PASS" : "FAIL"));
                        if (!ok) ++failures;
                        if (ok)
                        {
                            const auto bytes = clip->samples.size() * sizeof(float);
                            AudioEngine bounded(bytes);
                            const ClipData* pinned = nullptr;
                            const auto firstLoad = bounded.loadClip(pad.file, pinned, "bounded-a");
                            AudioCore pinCore; pinCore.prepare(pinned ? pinned->sampleRate : 48000.0);
                            VoiceSettings pinSettings; pinSettings.mode = PlayMode::holdLoop;
                            const bool pinnedVoice = firstLoad.wasOk() && pinCore.trigger(1, pinned, pinSettings);
                            float left[1] {}, right[1] {};
                            pinCore.render(left, right, 1);
                            const ClipData* second = nullptr;
                            const auto blocked = bounded.loadClip(pad.file, second, "bounded-b");
                            const bool protectedWhilePlaying = pinnedVoice && blocked.failed() && bounded.isCached(pad.file, "bounded-a");
                            pinCore.stop(); pinCore.render(left, right, 1);
                            const auto replaced = bounded.loadClip(pad.file, second, "bounded-b");
                            const bool evictedAfterRelease = replaced.wasOk() && !bounded.isCached(pad.file, "bounded-a")
                                && bounded.isCached(pad.file, "bounded-b") && bounded.cacheSize() <= bytes;
                            report.add("Cache pin / eviction after voice release: " + juce::String(protectedWhilePlaying && evictedAfterRelease ? "PASS" : "FAIL"));
                            if (!protectedWhilePlaying || !evictedAfterRelease) ++failures;
                            AudioEngine lru(bytes * 2);
                            const ClipData* unused = nullptr;
                            const auto lruA = lru.loadClip(pad.file, unused, "lru-a");
                            const auto lruB = lru.loadClip(pad.file, unused, "lru-b");
                            const bool refreshedA = lru.isCached(pad.file, "lru-a");
                            const auto lruC = lru.loadClip(pad.file, unused, "lru-c");
                            const bool oldestRemoved = lruA.wasOk() && lruB.wasOk() && refreshedA && lruC.wasOk()
                                && lru.isCached(pad.file, "lru-a") && !lru.isCached(pad.file, "lru-b")
                                && lru.isCached(pad.file, "lru-c") && lru.cacheSize() <= bytes * 2;
                            report.add("Least-recently-used eviction order: " + juce::String(oldestRemoved ? "PASS" : "FAIL"));
                            if (!oldestRemoved) ++failures;
                        }
                    }
                }
                report.add("Unavailable referenced files (not modified): " + juce::String(missing));
            }
            MicrophoneRing ring;
            ring.reset(1000.0);
            float inputLeft[100] {}, inputRight[100] {}, outputLeft[4] {}, outputRight[4] {};
            for (int i = 0; i < 100; ++i) { inputLeft[i] = i * 0.001f; inputRight[i] = i * 0.0005f; }
            ring.push(inputLeft, inputRight, 100);
            ring.read(outputLeft, outputRight, 4, 2000.0);
            const bool ringOk = std::abs(outputLeft[0] - 0.085f) < 0.002f
                && std::abs(outputLeft[1] - 0.0855f) < 0.002f
                && std::abs(outputRight[0] - 0.0425f) < 0.002f;
            report.add("Microphone monitor ring / resampling: " + juce::String(ringOk ? "PASS" : "FAIL"));
            if (!ringOk) ++failures;
            report.add("Failures: " + juce::String(failures));
            resultDirectory.getChildFile("library-tests.txt").replaceWithText(report.joinIntoString("\n"));
            setApplicationReturnValue(failures == 0 ? 0 : 1); quit(); return;
        }
        if (args.contains("--performance-test"))
        {
            const auto destination = args.size() > 1 ? juce::File(args[1])
                : findProjectRoot().getChildFile("test-output/performance.txt");
            destination.getParentDirectory().createDirectory();
            juce::StringArray report;
            report.add("DualDeck performance probe (milliseconds, this computer)");
            Library existing;
            const auto startLoad = juce::Time::getMillisecondCounterHiRes();
            const auto loaded = existing.load(findProjectRoot().getChildFile("data/library.json"));
            report.add("Current library load: " + juce::String(juce::Time::getMillisecondCounterHiRes() - startLoad, 2)
                + " ms / " + juce::String(existing.totalPads()) + " pads");
            int failures = loaded.failed() ? 1 : 0;
            if (loaded.wasOk())
            {
                AudioEngine engine;
                std::set<juce::String> seen;
                int measured = 0;
                for (const auto& profile : existing.profiles)
                    for (const auto& pad : profile.pads)
                    {
                        if (measured >= 6 || !pad.file.existsAsFile()
                            || !seen.insert(pad.file.getFullPathName()).second) continue;
                        const ClipData* clip = nullptr;
                        const auto before = juce::Time::getMillisecondCounterHiRes();
                        const auto result = engine.loadClip(pad.file, clip, "performance-probe");
                        report.add("Cold decode " + pad.file.getFileExtension() + " / " + pad.file.getFileName()
                            + ": " + juce::String(juce::Time::getMillisecondCounterHiRes() - before, 2)
                            + " ms / " + (result.wasOk() ? juce::String("PASS") : result.getErrorMessage()));
                        if (result.failed()) ++failures;
                        ++measured;
                    }
            }
            Profile synthetic;
            synthetic.id = "performance"; synthetic.name = "Performance"; synthetic.sortField = SortField::name;
            synthetic.pads.reserve(50000);
            const auto folder = destination.getParentDirectory().getChildFile("not-created");
            for (int i = 0; i < 50000; ++i)
            {
                Pad pad; pad.id = juce::String(i); pad.name = "Suono " + juce::String(i);
                pad.file = folder.getChildFile("group" + juce::String(i / 5000)).getChildFile("sound" + juce::String(i) + ".wav");
                synthetic.pads.push_back(std::move(pad));
            }
            const auto beforeSort = juce::Time::getMillisecondCounterHiRes();
            const auto ordered = orderedPads(synthetic, "Suono 4");
            report.add("Search + natural sort / 50,000 pads: "
                + juce::String(juce::Time::getMillisecondCounterHiRes() - beforeSort, 2)
                + " ms / " + juce::String(ordered.size()) + " matches");
            const auto beforePages = juce::Time::getMillisecondCounterHiRes();
            const auto pages = folderPadPages(synthetic, ordered);
            report.add("Folder pagination / 50,000 pads: "
                + juce::String(juce::Time::getMillisecondCounterHiRes() - beforePages, 2)
                + " ms / " + juce::String(pages.size()) + " pages");
            synthetic.pads.resize(20000);
            const auto beforeCopy = juce::Time::getMillisecondCounterHiRes();
            Library snapshot; snapshot.profiles.push_back(synthetic);
            report.add("UI snapshot copy / 20,000 pads: "
                + juce::String(juce::Time::getMillisecondCounterHiRes() - beforeCopy, 2) + " ms");
            Library large; large.profiles.push_back(std::move(synthetic));
            const auto fixture = destination.getParentDirectory().getChildFile("large-library.json");
            const auto beforeSave = juce::Time::getMillisecondCounterHiRes();
            const auto saved = large.save(fixture);
            report.add("Save / 20,000 pads: "
                + juce::String(juce::Time::getMillisecondCounterHiRes() - beforeSave, 2)
                + " ms / " + (saved.wasOk() ? juce::String("PASS") : saved.getErrorMessage()));
            if (saved.failed()) ++failures;
            if (saved.wasOk())
            {
                Library reloaded;
                const auto beforeReload = juce::Time::getMillisecondCounterHiRes();
                const auto result = reloaded.load(fixture);
                const bool ok = result.wasOk() && reloaded.totalPads() == 20000;
                report.add("Reload / 20,000 pads: "
                    + juce::String(juce::Time::getMillisecondCounterHiRes() - beforeReload, 2)
                    + " ms / " + juce::String(ok ? "PASS" : "FAIL"));
                if (!ok) ++failures;
            }
            report.add("Software onset metric: available in the app footer after a pad is pressed; excludes driver and Discord latency.");
            report.add("Failures: " + juce::String(failures));
            destination.replaceWithText(report.joinIntoString("\n"));
            setApplicationReturnValue(failures == 0 ? 0 : 1); quit(); return;
        }
        if (args.contains("--interaction-smoke"))
        {
            const auto destination = args.size() > 1 ? juce::File(args[1])
                : findProjectRoot().getChildFile("test-output/ui-interactions.txt");
            destination.getParentDirectory().createDirectory();
            juce::StringArray report;
            int failures = 0;
            PadTile tile; tile.index = 7;
            int selected = -1, presses = 0, releases = 0;
            tile.select = [&](int index) { selected = index; };
            tile.press = [&](int, bool down) { if (down) ++presses; else ++releases; };
            const bool padHandled = tile.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
            tile.keyStateChanged(false);
            const bool padOk = padHandled && selected == 7 && presses == 1 && releases == 1;
            report.add("Pad Enter press/release: " + juce::String(padOk ? "PASS" : "FAIL"));
            if (!padOk) ++failures;
            KeyboardView keyboard;
            juce::String assigned, keyDown, keyUp;
            keyboard.assign = [&](const juce::String& key) { assigned = key; };
            keyboard.press = [&](const juce::String& key, bool down) { if (down) keyDown = key; else keyUp = key; };
            const bool arrowHandled = keyboard.keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
            const bool enterHandled = keyboard.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
            keyboard.keyStateChanged(false);
            const bool assignmentHandled = keyboard.keyPressed(juce::KeyPress(juce::KeyPress::F10Key,
                juce::ModifierKeys::shiftModifier, 0));
            const bool keyboardOk = arrowHandled && enterHandled && assignmentHandled
                && keyDown == "003B:0" && keyUp == keyDown && assigned == keyDown;
            report.add("Graphical keyboard arrows / Enter / Shift+F10: "
                + juce::String(keyboardOk ? "PASS" : "FAIL"));
            if (!keyboardOk) ++failures;
            const auto ordering = runPadOrderingUiTests();
            report.add("Pad numbering / sorting / pages / filters / keyboard: "
                + (ordering.wasOk() ? juce::String("PASS") : ordering.getErrorMessage()));
            if (ordering.failed()) ++failures;
            report.add("Physical two-keyboard timing and screen-reader speech still require a person at the device.");
            report.add("Failures: " + juce::String(failures));
            destination.replaceWithText(report.joinIntoString("\n"));
            setApplicationReturnValue(failures == 0 ? 0 : 1); quit(); return;
        }
        if (args.contains("--keyboard-smoke"))
        {
            const auto destination = args.size() > 1 ? juce::File(args[1]) : findProjectRoot().getChildFile("test-output/keyboard-devices.txt");
            destination.getParentDirectory().createDirectory();
            RawKeyboard receiver;
            const auto result = receiver.initialise();
            juce::StringArray report;
            report.add("Raw Input registration: " + (result.wasOk() ? juce::String("PASS") : result.getErrorMessage()));
            const auto devices = RawKeyboard::devices();
            report.add("Keyboard devices: " + juce::String(devices.size()));
            int failures = result.failed() ? 1 : 0;
            for (const auto& device : devices)
            {
                const auto selected = receiver.select(device.id);
                report.add(device.name + ": " + (selected.wasOk() ? juce::String("PASS") : selected.getErrorMessage()));
                if (selected.failed()) ++failures;
            }
            if (receiver.enabled()) { report.add("FAIL: input unexpectedly enabled at startup"); ++failures; }
            report.add("Background capture left disabled; physical keystrokes require a manual check.");
            report.add("Failures: " + juce::String(failures));
            destination.replaceWithText(report.joinIntoString("\n"));
            setApplicationReturnValue(failures == 0 ? 0 : 1); quit(); return;
        }
        if (args.contains("--audio-smoke"))
        {
            const auto destination = args.size() > 1 ? juce::File(args[1]) : findProjectRoot().getChildFile("test-output/audio-devices.txt");
            destination.getParentDirectory().createDirectory();
            AudioEngine engine; juce::StringArray report; int failures = 0;
            const auto names = engine.main.deviceNames();
            if (names.isEmpty()) { report.add("FAIL: no audio outputs"); ++failures; }
            for (const auto& name : names)
            {
                const auto result = engine.configure(name, {}, false);
                if (result.wasOk()) juce::Thread::sleep(120);
                report.add(name + ": " + (result.wasOk() ? engine.main.details() : result.getErrorMessage()));
                if (result.failed()) ++failures;
                engine.main.close();
            }
            for (int i = 1; i < names.size(); ++i)
            {
                const auto result = engine.configure(names[0], names[i], true);
                if (result.wasOk()) juce::Thread::sleep(120);
                const bool ok = result.wasOk() && engine.hasMonitor();
                report.add("Dual output: " + names[0] + " + " + names[i] + ": " + (ok ? juce::String("PASS") : result.getErrorMessage()));
                if (!ok) ++failures;
                engine.main.close(); engine.monitor.close();
            }
            if (!names.isEmpty())
            {
                const auto result = engine.configure(names[0], names[0], true);
                const bool ok = result.wasOk() && !engine.hasMonitor();
                report.add("Same endpoint deduplication: " + juce::String(ok ? "PASS" : "FAIL"));
                if (!ok) ++failures;
                const auto invalid = engine.configure(names[0], "DualDeck missing monitor test device", true);
                const bool closed = invalid.failed() && !engine.main.isOpen() && !engine.hasMonitor();
                report.add("Failed monitor leaves both outputs closed: " + juce::String(closed ? "PASS" : "FAIL"));
                if (!closed) ++failures;
            }
            const auto inputs = engine.main.inputDeviceNames();
            report.add("Microphone inputs found: " + juce::String(inputs.size()));
            if (!names.isEmpty() && !inputs.isEmpty())
            {
                engine.setMicrophoneGain(0.0f); // Open the input without sending captured sound to an output.
                const auto result = engine.configure(names[0], {}, false, inputs[0], true);
                if (result.wasOk()) juce::Thread::sleep(120);
                report.add("Microphone input + muted output: " + inputs[0] + ": "
                    + (result.wasOk() ? juce::String("PASS") : result.getErrorMessage()));
                if (result.failed()) ++failures;
                engine.main.close();
            }
            report.add("Failures: " + juce::String(failures));
            destination.replaceWithText(report.joinIntoString("\n")); setApplicationReturnValue(failures == 0 ? 0 : 1); quit(); return;
        }
        const bool smoke = args.contains("--smoke-test");
        const bool startupCheck = args.contains("--startup-check");
        const bool uiEditorSnapshot = args.contains("--ui-editor-snapshot");
        bool uiEditor = false;
#if defined(DUALDECK_UI_EDITOR)
        uiEditor = (!smoke && !startupCheck) || uiEditorSnapshot;
#endif
        window = std::make_unique<DeckWindow>(smoke, uiEditor);
        if (startupCheck || uiEditorSnapshot) window->setVisible(false);
        if (smoke || startupCheck || uiEditorSnapshot)
        {
            const auto destination = args.size() > 1 ? juce::File(args[1]) : findProjectRoot().getChildFile("test-output/board.png");
            const int section = args.size() > 2 ? args[2].getIntValue() : 0;
            if (args.size() > 4) window->centreWithSize(args[3].getIntValue(), args[4].getIntValue());
            window->board().prepareSnapshot(section);
            juce::Timer::callAfterDelay(350, [this, destination]
            {
                destination.getParentDirectory().createDirectory();
                auto image = window->getContentComponent()->createComponentSnapshot(window->getContentComponent()->getLocalBounds());
                juce::FileOutputStream stream(destination);
                if (stream.openedOk()) { stream.setPosition(0); stream.truncate(); juce::PNGImageFormat format; format.writeImageToStream(image, stream); }
                else setApplicationReturnValue(1);
                quit();
            });
        }
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override
    {
        if (window) window->board().requestClose([this] { quit(); });
        else quit();
    }
    void anotherInstanceStarted(const juce::String&) override { if (window) window->toFront(true); }
private:
    std::unique_ptr<DeckWindow> window;
};
}
START_JUCE_APPLICATION(dualdeck::DualDeckApplication)
