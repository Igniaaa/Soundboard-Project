#include "../Source/MainComponent.h"
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
#include "../Source/MainScreenPanel.h"
#include "../Source/ProgramAppearance.h"
#endif

namespace dualdeck
{
juce::Result runMainScreenUiTests()
{
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    MainComponent board(true); board.stopTimer();
    if (board.findChildWithID("foleysMainScreen") == nullptr || !board.mainScreenPanel)
        return juce::Result::fail("Main screen has no live Foleys host");
    auto& panel = *board.mainScreenPanel;
    if (panel.validateLayout(panel.state.getGuiTree()).failed()) return juce::Result::fail("Embedded main-screen layout has incomplete control bindings");
    for (const auto& id : panel.controlIds())
    {
        auto* c = panel.wrappedControl(id);
        auto* item = c ? dynamic_cast<foleys::GuiItem*>(c->getParentComponent()) : nullptr;
        if (!c || !item || item->getWrappedComponent() != c || !panel.isParentOf(c))
            return juce::Result::fail("Control is a preview rather than a live GuiItem: " + id);
    }
    if (!panel.findNode("search").hasType("TextEditor") || !panel.findNode("outputVolume").hasType("Slider")
        || !panel.findNode("profiles").hasType("ComboBox") || !panel.findNode("padTile1").hasType("Pad")
        || !dynamic_cast<foleys::Container*>(panel.builder.findGuiItem(panel.findNode("padGrid"))))
        return juce::Result::fail("Main-screen factories are not typed Foleys elements");
    board.library = Library{}; board.library.ensureProfile();
    for (int i = 0; i < 30; ++i)
    {
        Pad pad; pad.id = "live-" + juce::String(i); pad.name = "Suono " + juce::String(i + 1);
        pad.file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("live-" + juce::String(i) + ".wav");
        pad.useGlobalMode = false; pad.settings.mode = PlayMode::holdLoop;
        board.library.current().pads.push_back(pad);
    }
    board.refresh(); board.setView(0); board.selectPad(0);
    panel.builder.findGuiItem(panel.findNode("settingsPage"))->setVisible(true);
    panel.builder.findGuiItem(panel.findNode("dedicatedPage"))->setVisible(true);
    board.keyboardDevice.setVisible(true);
    auto rendered = board.createComponentSnapshot(board.getLocalBounds());
    if (!rendered.isValid() || panel.builder.findGuiItem(panel.findNode("settingsPage"))->isVisible() || board.keyboardDevice.isVisible())
        return juce::Result::fail("Deferred Foleys refresh painted an inactive page over the soundboard");
    for (int width : {1380, 940})
    {
        board.setSize(width, board.preferredHeight(width)); board.resized();
        std::vector<juce::Rectangle<int>> rectangles;
        for (int i = 0; i < 24; ++i)
        {
            auto* tile = board.tiles[static_cast<std::size_t>(i)].get(); const auto box = panel.getLocalArea(tile, tile->getLocalBounds());
            if (!tile->isVisible() || box.getWidth() < 80 || box.getHeight() < 60 || !panel.getLocalBounds().contains(box))
                return juce::Result::fail("Responsive pad layout clipped a live pad");
            for (const auto& other : rectangles) if (box.intersects(other)) return juce::Result::fail("Live pads overlap");
            rectangles.push_back(box);
        }
        const auto content = panel.elementBounds("boardContent");
        for (auto* control : std::initializer_list<juce::Component*> {&board.profiles, &board.newProfile, &board.renameProfile, &board.deleteProfile, &board.globalMode})
            if (!content.contains(panel.getLocalArea(control, control->getLocalBounds()))) return juce::Result::fail("Small-width profile toolbar overflows the board content");
    }
    board.outputVolume.setValue(42, juce::sendNotificationSync);
    if (std::abs(board.library.outputVolume - 0.42f) > 0.001f) return juce::Result::fail("Live master slider lost its callback");
    board.tiles[0]->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
    if (!board.pressedPads.isHeld(board.tiles[0]->index)) return juce::Result::fail("Live pad no longer starts playback");
    board.tiles[0]->keyStateChanged(false);
    if (board.pressedPads.isHeld(board.tiles[0]->index)) return juce::Result::fail("Live pad release was lost");
    const auto title = board.tiles[0]->getTitle();
    board.search.setText("#3", false); board.search.onTextChange();
    if (board.filtered.size() != 1 || board.tiles[0]->displayNumber != 3) return juce::Result::fail("Live search no longer filters pads");
    board.search.clear(); board.rebuildTiles();
    board.page = 1; board.rebuildTiles();
    if (!board.tiles[5]->isVisible() || board.tiles[6]->isVisible()) return juce::Result::fail("Live page visibility exposes empty pad slots");
    board.setView(2); board.setView(0); panel.rebuild();
    if (board.outputVolume.getValue() != 42 || board.tiles[0]->getTitle().isEmpty() || title.isEmpty()) return juce::Result::fail("Rebuilding lost live values or accessible pad titles");
    board.keyboardMode = true; board.setView(0); panel.rebuild();
    if (!board.keyboardViewport.isVisible() || board.tiles[0]->isVisible() || !panel.elementBounds("graphicalKeyboard").contains(panel.getLocalArea(&board.keyboardViewport, board.keyboardViewport.getLocalBounds())))
        return juce::Result::fail("Native keyboard page visibility or layout is invalid");
    board.keyboardMode = false; board.setView(0);
    panel.findNode("padGrid").setProperty("pos-height", 180, nullptr); panel.handleUpdateNowIfNeeded();
    auto* padBox = board.tiles[0]->getParentComponent()->getParentComponent();
    auto* padViewport = padBox->findParentComponentOfClass<juce::Viewport>();
    if (!padViewport) return juce::Result::fail("Pad grid has no scrolling viewport");
    for (const auto& tile : board.tiles)
        if (tile->getParentComponent()->getRight() > padViewport->getMaximumVisibleWidth())
            return juce::Result::fail("Pad grid's final column extends under its scrollbar");
    panel.findNode("padGrid").removeProperty("pos-height", nullptr); panel.handleUpdateNowIfNeeded();
    const auto fixtures = findProjectRoot().getChildFile("test-output/live-main-fixtures").getChildFile(juce::Uuid().toString());
    fixtures.createDirectory();
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup {fixtures};
    panel.layoutFile = fixtures.getChildFile("main.xml");
    board.setSize(1380, 880); board.resized();
    const auto oldSearch = panel.getLocalArea(&board.search, board.search.getLocalBounds());
    panel.findNode("search").setProperty("margin", "0 25 0 0", nullptr);
    panel.findNode("search").setProperty("font-size", 18.0, nullptr); panel.findNode("search").setProperty("font-scale", 1.2, nullptr);
    panel.findNode("stopButton").setProperty("font-size", 20.0, nullptr);
    panel.findNode("padTile1").setProperty("surface-color", "ff314159", nullptr);
    panel.findNode("graphicalKeyboard").setProperty("accent-color", "ff7788cc", nullptr);
    panel.findNode("keyboardDevice").setProperty("control-background", "ff775544", nullptr);
    auto classes = panel.state.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Classes");
    juce::ValueTree colourClass("newMainColour"); colourClass.setProperty("button-color", "ff225588", nullptr); classes.addChild(colourClass, -1, nullptr);
    panel.findNode("stopButton").setProperty("class", "newMainColour", nullptr);
    panel.handleUpdateNowIfNeeded();
    if (panel.getLocalArea(&board.search, board.search.getLocalBounds()).getWidth() != oldSearch.getWidth() - 13
        || std::abs(board.search.getFont().getHeight() - 21.6f) > 0.01f
        || std::abs(board.look.getTextButtonFont(board.stopButton, board.stopButton.getHeight()).getHeight() - 20.0f) > 0.01f
        || board.tiles[0]->findColour(uiColours::tile, true) != juce::Colour(0xff314159)
        || board.keyboard.findColour(uiColours::accent, true) != juce::Colour(0xff7788cc))
        return juce::Result::fail("Native properties: search width " + juce::String(oldSearch.getWidth()) + " -> " + juce::String(board.search.getWidth())
            + ", text font " + juce::String(board.search.getFont().getHeight()) + ", button font " + juce::String(board.look.getTextButtonFont(board.stopButton, board.stopButton.getHeight()).getHeight())
            + ", pad " + board.tiles[0]->findColour(uiColours::tile, true).toString() + ", keyboard " + board.keyboard.findColour(uiColours::accent, true).toString());
    if (board.stopButton.findColour(juce::TextButton::buttonColourId) != juce::Colour(0xff225588)) return juce::Result::fail("New native colour class failed to apply live");
    if (panel.saveAppearance().failed()) return juce::Result::fail("Cannot save live main layout");
    const auto saved = panel.layoutFile.loadFileAsString();
    MainComponent reopened(true); reopened.stopTimer(); auto& restored = *reopened.mainScreenPanel; restored.layoutFile = panel.layoutFile;
    if (restored.loadAppearance().failed() || reopened.tiles[0]->findColour(uiColours::tile, true) != juce::Colour(0xff314159)
        || reopened.keyboardDevice.findColour(juce::ComboBox::backgroundColourId) != juce::Colour(0xff775544)
        || std::abs(reopened.search.getFont().getHeight() - 21.6f) > 0.01f)
        return juce::Result::fail("Native main-screen appearance failed to reload");
    const auto imported = fixtures.getChildFile("import.xml"); imported.replaceWithText(saved);
    if (panel.importLayout(imported).failed()) return juce::Result::fail("Validated main layout import failed");
    panel.findNode("search").setProperty("text-color", "ff1188cc", nullptr); panel.handleUpdateNowIfNeeded();
    if (board.search.findColour(juce::TextEditor::textColourId) != juce::Colour(0xff1188cc) || board.tiles[6]->isVisible())
        return juce::Result::fail("Import lost listeners or resurrected hidden pads");
    auto unsafe = juce::ValueTree::fromXml(saved); unsafe.getChildWithName("View").setProperty("pos-width", "nan", nullptr); imported.replaceWithText(unsafe.toXmlString());
    if (panel.importLayout(imported).wasOk() || panel.wrappedControl("search") != &board.search) return juce::Result::fail("Unsafe geometry was imported");
    imported.replaceWithText("<magic><Styles/><View/></magic>");
    if (panel.importLayout(imported).wasOk() || !panel.isParentOf(&board.search)) return juce::Result::fail("Incomplete import detached live controls");
    auto remove = panel.findNode("search"); remove.getParent().removeChild(remove, nullptr);
    if (panel.saveAppearance().wasOk() || panel.layoutFile.loadFileAsString() != saved) return juce::Result::fail("Incomplete main layout overwrote last valid file");
    panel.layoutFile.replaceWithText("<broken>");
    if (panel.loadAppearance().wasOk() || panel.layoutFile.loadFileAsString() != "<broken>") return juce::Result::fail("Malformed main layout failed safe fallback");
    panel.layoutFile.replaceWithText(saved); panel.loadAppearance();
    if (panel.resetAppearance().failed() || board.tiles[0]->findColour(uiColours::tile, true) != palette::tile) return juce::Result::fail("Native reset did not restore default style");
    const auto defaultXml = panel.layoutFile.loadFileAsString();
    panel.state.getGuiTree().setProperty("notes", juce::String::repeatedString("x", 1024 * 1024), nullptr);
    if (panel.saveAppearance().wasOk() || panel.layoutFile.loadFileAsString() != defaultXml) return juce::Result::fail("Oversized main layout was saved");
    panel.state.getGuiTree().removeProperty("notes", nullptr);
    const auto blocked = fixtures.getChildFile("blocked"); blocked.replaceWithText("blocked"); panel.layoutFile = blocked.getChildFile("main.xml");
    panel.findNode("stopButton").setProperty("button-color", "ff334455", nullptr); panel.handleUpdateNowIfNeeded();
    if (panel.resetAppearance().wasOk() || board.stopButton.findColour(juce::TextButton::buttonColourId) != juce::Colour(0xff334455))
        return juce::Result::fail("Failed main reset lost live customization");
    const auto legacy = fixtures.getChildFile("legacy.xml");
    const juce::String legacyXml("<magic><Styles><Style name='Old'><Nodes><search text-color='$background' font-size='19'/></Nodes><Classes><warm button-color='$brand'/></Classes><Palettes><default brand='ffaa1122' background='ff556677'/></Palettes></Style></Styles><View><AppControl id='stopButton' class='warm'/><AppControl id='search' font-scale='1.1' offset-y='9'/></View></magic>");
    legacy.replaceWithText(legacyXml); panel.legacyFile = legacy; panel.layoutFile = fixtures.getChildFile("converted.xml");
    if (panel.loadAppearance().failed() || board.search.findColour(juce::TextEditor::textColourId) != juce::Colour(0xff556677)
        || board.stopButton.findColour(juce::TextButton::buttonColourId) != juce::Colour(0xffaa1122)
        || std::abs(board.search.getFont().getHeight() - 20.9f) > 0.01f || legacy.loadFileAsString() != legacyXml
        || panel.state.getGuiTree().getChildWithName("LegacyMainAppearance").getChildWithProperty("id", "search")["offset-y"].toString() != "9")
        return juce::Result::fail("Legacy main styles, node overrides, palette references or geometry backup were lost");
    const auto archivedFile = fixtures.getChildFile("archived-program.xml");
    auto oldProgram = board.programAppearance->findNode("programTheme").getParent().getParent().createCopy();
    juce::ValueTree retired("AppControl"); retired.setProperty("id", "search", nullptr); retired.setProperty("font-size", 17, nullptr);
    oldProgram.getChildWithName("View").addChild(retired, -1, nullptr); archivedFile.replaceWithText(oldProgram.toXmlString());
    if (board.programAppearance->loadAppearance(archivedFile).failed() || board.programAppearance->findNode("search").isValid()
        || board.programAppearance->saveAppearance().failed()
        || !juce::ValueTree::fromXml(archivedFile.loadFileAsString()).getChildWithName("LegacyMainControls").getChildWithProperty("id", "search").isValid())
        return juce::Result::fail("Retired preview controls were not safely archived");
    panel.legacyFile = archivedFile; panel.layoutFile = fixtures.getChildFile("archive-converted.xml");
    if (panel.loadAppearance().failed() || std::abs(board.search.getFont().getHeight() - 17.0f) > 0.01f)
        return juce::Result::fail("Native main layout could not recover archived preview styles");
    if (board.audio.main.isOpen() || board.saveGeneration.load() != 0) return juce::Result::fail("Main UI tests changed audio or library data");
#endif
    return juce::Result::ok();
}
}
