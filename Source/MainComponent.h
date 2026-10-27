#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "AudioEngine.h"
#include "RawKeyboard.h"
#include "PadEditor.h"
#include "PadPressState.h"
#if defined(DUALDECK_UI_EDITOR)
#include "UiLayoutEditor.h"
#endif
#include <set>
#include <map>

namespace dualdeck
{
namespace palette
{
inline const juce::Colour background {0xff0b111b}, panel {0xff121c29}, tile {0xff182536};
inline const juce::Colour line {0xff28384b}, text {0xffedf3f9}, muted {0xff8c9eb3};
inline const juce::Colour accent {0xff63dfc4}, blue {0xff7aa8ff}, warning {0xffffc579};
}
class DeckLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DeckLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
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
struct KeyboardKey { juce::String label, code; float width = 1; };
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
    juce::String focusedCode() const;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> hitboxes;
    juce::String held, keyboardHeldCode;
    int focusedRow = 0, focusedColumn = 0;
    bool keyboardHeld = false;
};
class MainComponent final : public juce::Component, private juce::Timer,
                            private juce::KeyListener, public juce::FileDragAndDropTarget
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
    int preferredHeight(int width) const;
    void requestClose(std::function<void()> onSaved);
private:
    friend juce::Result runPadOrderingUiTests();
    using juce::Component::keyPressed;
    using juce::Component::keyStateChanged;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged(bool, juce::Component*) override;
    void refresh();
    void updateEditorViewportBounds();
    void selectPad(int);
    void rebuildTiles();
    void preloadVisiblePage(const std::vector<int>& padIndices);
    void setView(int);
    void triggerPad(int, bool);
    void handleKey(const juce::String&, bool);
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
    juce::ToggleButton captureKeyboard {"Attiva i tasti della tastiera selezionata"};
    juce::Label keyboardHeading, keyboardHelp, keyboardStatus, lastKey, inputStatus;
    juce::TextButton stopButton {"STOP TUTTO"}, djButton {"Pausa DJ"};
    juce::TextButton previous {"<"}, next {">"}, tilesButton {"Pad"}, keyboardButton {"Tastiera"};
    juce::TextButton bindButton {"Associa un tasto"}, unbindButton {"Libera"};
    juce::TextButton applyButton {"Applica uscite"}, scanButton {"Aggiorna dispositivi"};
    juce::ToggleButton favoriteButton {"Preferito"}, favoritesOnly {"Solo preferiti"}, monitorToggle {"Ascolto aggiuntivo in cuffia"};
    juce::ToggleButton volumeOverride {"Usa il volume del suono"}, globalPitchToggle {"Attiva pitch globale"};
    juce::ToggleButton microphoneToggle {"Mix microfono fisico nell'uscita principale"};
    juce::ToggleButton microphoneMonitorToggle {"Ascolta il microfono anche in cuffia"};
    juce::Slider globalPitch, outputVolume, monitorVolume, microphoneVolume, padVolume, padSpeed, fadeIn, fadeOut;
    juce::Label title, subTitle, profileLabel, modeLabel, pageLabel, statusLabel, padHeading, padFile, keyInfo;
    juce::Label outputLabel, monitorLabel, outputDetails, monitorDetails, audioHelp;
    juce::Label microphoneLabel, microphoneGainLabel, microphoneHelp;
    juce::Label outputMasterLabel, monitorMasterLabel;
    juce::Label volumeLabel, speedLabel, fadeLabel, padModeLabel;
    std::array<std::unique_ptr<PadTile>, 24> tiles;
    KeyboardView keyboard;
    std::vector<int> filtered, padNumbers;
    std::map<juce::String, int> keyIndex;
    std::set<juce::String> heldKeys;
    PadPressState pressedPads;
    std::set<int> pendingPads;
    std::set<juce::String> preloadingFiles;
    juce::String lastPreloadPage;
    juce::String lastSearch;
    std::uint32_t searchChangedAt = 0;
    bool searchPending = false;
    int view = 0, page = 0, selection = -1, dirtyTicks = 0;
    bool keyboardMode = false, binding = false, updating = false, smokeTest = false, writable = true, dirty = false;
    bool djMouseDown = false, djKeyDown = false;
    bool keyboardArmed = false;
    bool rawKeyboardReady = false;
    int missingFiles = 0;
    std::uint64_t playGeneration = 0;
    juce::Rectangle<int> centre, inspector, footer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
juce::Result runPadOrderingUiTests();
}
