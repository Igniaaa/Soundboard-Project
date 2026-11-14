#include "MainComponent.h"
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
#include "ProgramAppearance.h"
#include "AudioSettingsPanel.h"
#include "MainScreenPanel.h"
#endif

namespace dualdeck
{
void MainComponent::semanticTextColour(juce::Label& label, int role)
{
    label.getProperties().set("appearance-text-role", role);
    label.setColour(juce::Label::textColourId, uiColour(label, role));
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (mainScreenPanel) mainScreenPanel->refreshControl(label.getComponentID());
    if (programAppearance) programAppearance->refreshStyle(label.getComponentID());
#endif
}
void MainComponent::initialiseProgramAppearance()
{
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    initialiseMainScreen();
    programAppearance = std::make_unique<ProgramAppearance>([this](const UiTheme& theme)
    { look.applyTheme(theme); if (audioSettingsPanel) audioSettingsPanel->applyTheme(theme); if (mainScreenPanel) mainScreenPanel->applyTheme(theme); sendLookAndFeelChange(); repaint(); menuItemsChanged(); });
    programAppearance->retireControls(mainScreenPanel->controlIds());
    editorPanel.registerAppearance(*programAppearance);
    programAppearance->applyLayout(editorPanel);
    const auto result = programAppearance->loadAppearance(smokeTest ? juce::File() : projectRoot.getChildFile("data/foleys-program-ui.xml"));
    if (result.failed()) setStatus(result.getErrorMessage(), true);
#endif
}
void MainComponent::initialiseMainScreen()
{
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    label(brandMark, "D", 19); brandMark.setJustificationType(juce::Justification::centred);
    brandMark.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
    label(brandName, "DualDeck", 21); brandName.setFont(juce::Font(juce::FontOptions(21.0f, juce::Font::bold)));
    label(emptyState, "", 18); emptyState.setJustificationType(juce::Justification::centred); emptyState.setInterceptsMouseClicks(false, false);
    outputMeter.read = [this] { return audio.main.core.meter(); }; addAndMakeVisible(outputMeter);
    std::vector<MainScreenPanel::Control> controls;
#define ADD(type, control) controls.push_back({#control, type, control})
    ADD("ComboBox", profiles); ADD("ComboBox", globalMode); ADD("ComboBox", sortBy); ADD("ComboBox", padFolderFilter); ADD("ComboBox", padMode); ADD("ComboBox", keyboardDevice);
    ADD("TextEditor", search); ADD("TextEditor", padName);
    ADD("Button", boardTab); ADD("Button", audioTab); ADD("Button", keyboardTab); ADD("Button", importButton);
    ADD("ToggleButton", soundInspectorToggle);
    ADD("Button", newProfile); ADD("Button", renameProfile); ADD("Button", deleteProfile); ADD("Button", sortDirection); ADD("Button", refreshFolders);
    ADD("Button", identifyKeyboard); ADD("Button", scanKeyboards); ADD("Button", replaceFile); ADD("Button", editorButton); ADD("Button", stopButton); ADD("Button", djButton);
    ADD("Button", previous); ADD("Button", next); ADD("Button", tilesButton); ADD("Button", keyboardButton); ADD("Button", bindButton); ADD("Button", unbindButton);
    ADD("ToggleButton", captureKeyboard); ADD("ToggleButton", favoriteButton); ADD("ToggleButton", favoritesOnly); ADD("ToggleButton", volumeOverride); ADD("ToggleButton", globalPitchToggle);
    ADD("Slider", globalPitch); ADD("Slider", outputVolume); ADD("Slider", monitorVolume); ADD("Slider", padVolume); ADD("Slider", padSpeed); ADD("Slider", fadeIn); ADD("Slider", fadeOut);
    ADD("Label", folderSummary); ADD("Label", padFolderHeading); ADD("Label", keyboardHeading); ADD("Label", keyboardHelp); ADD("Label", keyboardStatus); ADD("Label", lastKey); ADD("Label", inputStatus);
    ADD("Label", title); ADD("Label", profileLabel); ADD("Label", modeLabel); ADD("Label", pageLabel); ADD("Label", statusLabel); ADD("Label", padHeading); ADD("Label", padFile); ADD("Label", keyInfo);
    ADD("Label", outputMasterLabel); ADD("Label", monitorMasterLabel); ADD("Label", volumeLabel); ADD("Label", speedLabel); ADD("Label", fadeLabel); ADD("Label", padModeLabel);
    ADD("Label", brandMark); ADD("Label", brandName); ADD("Label", emptyState); ADD("Meter", outputMeter);
#undef ADD
    controls.push_back({"graphicalKeyboard", "Keyboard", keyboardViewport});
    controls.push_back({"audioPage", "AudioPage", *audioSettingsPanel});
    for (std::size_t i = 0; i < tiles.size(); ++i) controls.push_back({"padTile" + juce::String(static_cast<int>(i) + 1), "Pad", *tiles[i]});
#if defined(DUALDECK_FOLEYS_DESIGNER)
    controls.push_back({"editAppearance", "Button", editAppearanceButton}); controls.push_back({"saveAppearance", "Button", saveAppearanceButton}); controls.push_back({"resetAppearance", "Button", resetAppearanceButton});
#endif
    mainScreenPanel = std::make_unique<MainScreenPanel>(controls, smokeTest ? juce::File() : projectRoot.getChildFile("data/foleys-main-ui.xml"),
        smokeTest ? juce::File() : projectRoot.getChildFile("data/foleys-program-ui.xml"));
    addAndMakeVisible(*mainScreenPanel);
    mainScreenPanel->onLayoutChanged = [this]
    {
        centre = mainScreenPanel->elementBounds("boardContent"); inspector = mainScreenPanel->elementBounds("inspector");
        keyboard.setSize(juce::jmax(800, keyboardViewport.getWidth()), juce::jmax(360, keyboardViewport.getHeight() - keyboardViewport.getScrollBarThickness()));
    };
    mainScreenPanel->onAppearanceResult = [this](const juce::Result& result) { setStatus(result.wasOk() ? "Layout principale aggiornato." : result.getErrorMessage(), result.failed()); };
    resized(); rebuildTiles();
    if (!statusIsError && mainScreenPanel->getAppearanceLoadResult().failed()) setStatus(mainScreenPanel->getAppearanceLoadResult().getErrorMessage(), true);
#endif
}
void MainComponent::showMainScreenDesigner()
{
#if defined(DUALDECK_FOLEYS_DESIGNER)
    hideAppearanceEditors(); stopAll(); mainScreenPanel->showDesigner();
#endif
}
void MainComponent::showProgramAppearance()
{
#if defined(DUALDECK_FOLEYS_DESIGNER)
    if (mainScreenPanel) mainScreenPanel->hideDesigner();
    audioSettingsPanel->hideDesigner(); editAppearanceButton.setButtonText("Modifica aspetto");
    programAppearance->showDesigner(editorViewport.isVisible() ? static_cast<juce::Component&>(editorPanel) : *this);
#endif
}
void MainComponent::hideAppearanceEditors()
{
#if defined(DUALDECK_FOLEYS_DESIGNER)
    if (mainScreenPanel) mainScreenPanel->hideDesigner();
    if (programAppearance) programAppearance->hideDesigner();
    if (audioSettingsPanel) audioSettingsPanel->hideDesigner();
    editAppearanceButton.setButtonText("Modifica aspetto");
#endif
}
void MainComponent::saveCurrentAppearance()
{
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    const auto result = programAppearance->isDesignerVisible() || editorViewport.isVisible() ? programAppearance->saveAppearance()
        : view == 1 && !mainScreenPanel->isDesignerVisible() ? audioSettingsPanel->saveAppearance() : mainScreenPanel->saveAppearance();
    setStatus(result.wasOk() ? "Aspetto del programma salvato." : result.getErrorMessage(), result.failed());
#endif
}
void MainComponent::resetCurrentAppearance()
{
#if defined(DUALDECK_FOLEYS_DESIGNER)
    const bool program = programAppearance->isDesignerVisible() || editorViewport.isVisible();
    const bool audioPage = view == 1 && !program && !mainScreenPanel->isDesignerVisible();
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Ripristina aspetto",
        audioPage ? "Ripristinare l'aspetto iniziale della pagina audio?" : program ? "Ripristinare il tema e l'aspetto dell'editor?" : "Ripristinare il layout iniziale della schermata principale?", "Ripristina", "Annulla", this,
        juce::ModalCallbackFunction::create([safe, audioPage, program](int answer)
        {
            if (!safe || answer == 0) return;
            const auto result = audioPage ? safe->audioSettingsPanel->resetAppearance() : program ? safe->programAppearance->resetAppearance() : safe->mainScreenPanel->resetAppearance();
            safe->setStatus(result.wasOk() ? "Aspetto iniziale ripristinato." : result.getErrorMessage(), result.failed());
        }));
#endif
}
}
