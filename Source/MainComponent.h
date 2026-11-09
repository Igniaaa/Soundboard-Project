#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "AudioEngine.h"
#include "RawKeyboard.h"
#include "PadEditor.h"
#include "PadPressState.h"
#include "UiTheme.h"
#if defined(DUALDECK_UI_EDITOR)
#include "UiLayoutEditor.h"
#endif
#include <set>
#include <map>

namespace dualdeck
{
class AudioSettingsPanel;
class ProgramAppearance;
class MainScreenPanel;
namespace palette
{
inline const juce::Colour background {0xff0b111b}, panel {0xff121c29}, tile {0xff182536};
inline const juce::Colour line {0xff28384b}, text {0xffedf3f9}, muted {0xff8c9eb3};
inline const juce::Colour accent {0xff63dfc4}, blue {0xff7aa8ff}, warning {0xffffc579};
}
class DeckLookAndFeel final : public juce::LookAndFeel_V4, public UiThemeProvider
{
public:
    DeckLookAndFeel();
    void applyTheme(const UiTheme&);
    const UiTheme& getUiTheme() const override { return theme; }
    juce::Font getLabelFont(juce::Label& label) override
    {
        // JUCE assigns getComboBoxFont() to its internal label, so that font is already scaled.
        return dynamic_cast<juce::ComboBox*>(label.getParentComponent()) ? label.getFont()
            : label.getFont().withHeight((dynamic_cast<juce::Slider*>(label.getParentComponent())
                ? uiFontHeight(*label.getParentComponent(), label.getFont().getHeight()) : label.getFont().getHeight()) * uiFontScale(label));
    }
    juce::Font getTextButtonFont(juce::TextButton& button, int height) override
    { auto base = LookAndFeel_V4::getTextButtonFont(button, height); return base.withHeight(uiFontHeight(button, base.getHeight()) * uiFontScale(button)); }
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    { auto base = LookAndFeel_V4::getComboBoxFont(box); return base.withHeight(uiFontHeight(box, base.getHeight()) * uiFontScale(box)); }
    juce::Font getPopupMenuFont() override
    { auto base = LookAndFeel_V4::getPopupMenuFont(); return base.withHeight(base.getHeight() * theme.fontScale); }
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override;
    int getMenuBarItemWidth(juce::MenuBarComponent&, int, const juce::String&) override;
    void drawMenuBarBackground(juce::Graphics&, int, int, bool, juce::MenuBarComponent&) override;
    void drawMenuBarItem(juce::Graphics&, int, int, int, const juce::String&, bool, bool, bool, juce::MenuBarComponent&) override;
private:
    UiTheme theme;
};
class PadTile final : public juce::Component
{
public:
    juce::String title, subtitle, binding;
    int index = -1, displayNumber = 0;
    bool selected = false, favorite = false, missing = false;
    std::function<void(int, bool)> press;
    std::function<void(int)> select, contextMenu;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
private:
    bool held = false;
    bool keyboardHeld = false;
};
struct KeyboardKey { juce::String label, code; float width = 1, height = 1; };
const std::vector<std::vector<KeyboardKey>>& keyboardLayout();
class KeyboardView final : public juce::Component
{
public:
    const Profile* profile = nullptr;
    std::vector<int> padNumbers;
    juce::String selectedKey;
    std::function<void(const juce::String&, bool)> press;
    std::function<void(const juce::String&)> assign;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    void updateAccessibleKey();
    int padNumber(int index) const;
private:
    friend juce::Result runFullKeyboardTests(const juce::File&);
    juce::String focusedCode() const;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> hitboxes;
    juce::String held, keyboardHeldCode;
    int focusedRow = 0, focusedColumn = 0;
    bool keyboardHeld = false;
};
class MainComponent final : public juce::Component, private juce::Timer,
                            private juce::KeyListener, public juce::FileDragAndDropTarget, public juce::MenuBarModel
{
public:
    explicit MainComponent(bool smoke = false, bool uiEditor = false);
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void prepareSnapshot(int section);
    void showAudioUiDesigner();
    int preferredHeight(int width) const;
    void requestClose(std::function<void()> onSaved);
    enum MenuAction
    {
        importFolder = 100, updateFolders, createProfile, renameProfileAction, removeProfile, saveLibrary, quitApp,
        undoText = 120, redoText, cutText, copyText, pasteText, selectAllText, editSound, replaceSound, assignKey, clearKey,
        showBoard = 140, showPads, showKeyboard, showAudio, showDedicatedKeyboard,
        customiseScreen = 160, customiseProgram, saveStyle, resetStyle, finishStyle,
        rescanAudio = 180, rescanKeyboards, stopPlayback, showHelp, showAbout
    };
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int, const juce::String&) override;
    void menuItemSelected(int, int) override;
private:
    friend juce::Result runPadOrderingUiTests();
    friend juce::Result runFoleysAudioUiTests();
    friend juce::Result runMainScreenUiTests();
    friend juce::Result runFullKeyboardTests(const juce::File&);
    friend juce::Result runPreloadTests(const juce::File&);
    friend juce::Result runPreloadProbe(juce::StringArray&);
    using juce::Component::keyPressed;
    using juce::Component::keyStateChanged;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged(bool, juce::Component*) override;
    void refresh();
    void updateEditorViewportBounds();
    void selectPad(int);
    void rebuildTiles();
    void preloadProfile(const std::vector<int>& visiblePadIndices);
    void setView(int);
    void triggerPad(int, bool);
    void handleKey(const juce::String&, bool);
    void handleLocalKey(const juce::String&, bool);
    bool localKeyIsNavigation(const juce::String&, const juce::Component* focus, bool shift) const;
    void stopAll();
    void save();
    bool applyAudio();
    void refreshDevices();
    void chooseAudioFolder();
    void scanProfileFolders(const juce::String& profileId, const std::vector<juce::File>& extraFolders = {});
    void removeCurrentProfile();
    void renameCurrentProfile();
    void showPadMenu(int);
    void updateAudioControls();
    void setStatus(const juce::String&, bool error = false);
    void changePad();
    void bindKey(const juce::String&);
    void refreshKeyboards();
    void updateKeyboardStatus();
    void replaceAudioFile();
    void openPadEditor();
    bool previewEditorLoop(bool enabled, double start, double end);
    bool previewEditorPlay(double seconds);
    void previewEditorScrub(double seconds);
    void beginEditorScrub();
    void endEditorScrub(double seconds);
    void stopEditorPreview();
    bool textEditorFocused() const;
    Pad* selectedPad();
    int padNumber(int index) const;
    void label(juce::Label&, const juce::String&, float size = 13.0f);
    void semanticTextColour(juce::Label&, int role);
    void initialiseProgramAppearance();
    void initialiseMainScreen();
    void showMainScreenDesigner();
    void showProgramAppearance();
    void hideAppearanceEditors();
    void saveCurrentAppearance();
    void resetCurrentAppearance();

    DeckLookAndFeel look;
    Library library;
    AudioEngine audio;
    RawKeyboard rawKeyboard;
    std::vector<KeyboardDevice> keyboardDevices;
    juce::ThreadPool playbackLoader {1};
    juce::ThreadPool preloadLoader {1};
    juce::ThreadPool folderLoader {1};
    juce::ThreadPool waveformLoader {1};
    juce::ThreadPool saveLoader {1};
    std::atomic<std::uint64_t> saveGeneration {0}, savedGeneration {0};
    juce::CriticalSection saveLock;
    std::shared_ptr<Library> pendingSave;
    std::uint64_t pendingSaveGeneration = 0;
    bool saveWorkerQueued = false;
    bool closePending = false;
    std::function<void()> closeReady;
    std::shared_ptr<std::atomic<bool>> scanCancelled;
    bool scanningFolders = false;
    juce::String scanningProfile;
    juce::File projectRoot, libraryFile;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips {this, 600};
    juce::Component::SafePointer<juce::TextEditor> menuTextTarget;
    juce::ComboBox profiles, globalMode, outputDevice, monitorDevice, inputDevice, padMode, keyboardDevice, sortBy;
    juce::TextEditor search, padName;
    juce::TextButton boardTab {"Soundboard"}, audioTab {"Audio e uscite"}, importButton {"+  Collega cartella"};
    juce::TextButton newProfile {"+"}, renameProfile {"Rinomina"};
    juce::TextButton deleteProfile {"Rimuovi"}, sortDirection {"Crescente"}, refreshFolders {"Aggiorna cartelle"};
    juce::Label folderSummary, padFolderHeading;
    juce::ComboBox padFolderFilter;
    std::vector<juce::File> padFolders;
    juce::String padFolderProfile;
    juce::TextButton keyboardTab {"Tastiera dedicata"}, identifyKeyboard {"Riconosci con F12"}, scanKeyboards {"Aggiorna tastiere"}, replaceFile {"Cambia file"};
    juce::TextButton editorButton {"Audio ed effetti"};
    PadEditorPanel editorPanel;
    juce::Viewport editorViewport;
#if defined(DUALDECK_UI_EDITOR)
    std::unique_ptr<UiLayoutEditor> layoutEditor;
#endif
    juce::String editingPadId, editingProfileId;
    double editingClipDuration = 0.0;
    bool editorPlaybackRequested = false;
    bool editorPlaybackSeen = false, editorLoopPreviewRequested = false, editorScrubbing = false;
    bool editorScrubVoiceActive = false;
    juce::ToggleButton captureKeyboard {"Attiva tastiera dedicata"};
    juce::Label keyboardHeading, keyboardHelp, keyboardStatus, lastKey, inputStatus;
    juce::TextButton stopButton {"STOP TUTTO"}, djButton {"Pausa DJ"};
    juce::TextButton previous {"<"}, next {">"}, tilesButton {"Pad"}, keyboardButton {"Tastiera"};
    juce::TextButton bindButton {"Associa un tasto"}, unbindButton {"Libera"};
    juce::TextButton applyButton {"Applica uscite"}, scanButton {"Aggiorna dispositivi"};
    juce::ToggleButton favoriteButton {"Preferito"}, favoritesOnly {"Solo preferiti"}, monitorToggle {"Ascolto aggiuntivo in cuffia"};
    juce::ToggleButton volumeOverride {"Usa il volume del suono"}, globalPitchToggle {"Attiva pitch globale"};
    juce::ToggleButton microphoneToggle {"Aggiungi il microfono all'uscita"};
    juce::ToggleButton microphoneMonitorToggle {"Ascolta il microfono in cuffia"};
    juce::Slider globalPitch, outputVolume, monitorVolume, microphoneVolume, padVolume, padSpeed, fadeIn, fadeOut;
    juce::Label title, profileLabel, modeLabel, pageLabel, statusLabel, padHeading, padFile, keyInfo;
    juce::Label outputLabel, monitorLabel;
    juce::Label microphoneLabel, microphoneGainLabel;
    juce::Label outputMasterLabel, monitorMasterLabel;
    juce::Label volumeLabel, speedLabel, fadeLabel, padModeLabel;
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    std::unique_ptr<AudioSettingsPanel> audioSettingsPanel;
#endif
#if defined(DUALDECK_FOLEYS_DESIGNER)
    juce::TextButton editAppearanceButton {"Modifica aspetto"}, saveAppearanceButton {"Salva aspetto"}, resetAppearanceButton {"Ripristina"};
#endif
    std::array<std::unique_ptr<PadTile>, 24> tiles;
    KeyboardView keyboard;
    juce::Viewport keyboardViewport;
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    class Meter final : public juce::Component
    {
    public:
        std::function<float()> read;
        void paint(juce::Graphics& g) override
        {
            const float level = read ? juce::jlimit(0.0f, 1.0f, read()) : 0;
            auto b = getLocalBounds().toFloat(); g.setColour(uiColour(*this, uiColours::line)); g.fillRoundedRectangle(b, 2.5f);
            g.setColour(uiColour(*this, level > 0.9f ? uiColours::warning : uiColours::accent)); g.fillRoundedRectangle(b.withWidth(b.getWidth() * level), 2.5f);
        }
    } outputMeter;
    juce::Label brandMark, brandName, emptyState;
    std::unique_ptr<MainScreenPanel> mainScreenPanel;
#endif
    std::vector<int> filtered, padNumbers;
    std::map<juce::String, int> keyIndex;
    std::set<juce::String> heldKeys;
    PadPressState pressedPads;
    std::set<int> pendingPads;
    juce::String lastPreloadPlan;
    std::atomic<std::uint64_t> preloadGeneration {0};
    std::atomic<int> preloadRemaining {0};
    std::mutex preloadStateLock;
    juce::String statusMessage;
    int displayedPreloadRemaining = 0;
    bool statusIsError = false;
    juce::String lastSearch;
    std::uint32_t searchChangedAt = 0;
    bool searchPending = false;
    int view = 0, page = 0, selection = -1, dirtyTicks = 0;
    bool keyboardMode = false, binding = false, updating = false, smokeTest = false, writable = true, dirty = false;
    bool djMouseDown = false, djKeyDown = false;
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    std::unique_ptr<ProgramAppearance> programAppearance;
#endif
    bool rawKeyboardReady = false;
    int missingFiles = 0;
    std::uint64_t playGeneration = 0;
    juce::Rectangle<int> centre, inspector, footer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
juce::Result runPadOrderingUiTests();
}
