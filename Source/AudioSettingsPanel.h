#pragma once
#include <foleys_gui_magic/foleys_gui_magic.h>
#include "UiTheme.h"

namespace dualdeck
{
// The controller retains ownership of the controls and all audio callbacks.
class AudioSettingsPanel final : public juce::Component
{
public:
    struct Control { const char* id; juce::Component& component; };
    explicit AudioSettingsPanel(std::initializer_list<Control> controls, const juce::File& appearanceFile = {});
    void paint(juce::Graphics&) override;
    void resized() override;
    void rebuild();
    void applyTheme(const UiTheme&);
    void showDesigner();
    void hideDesigner();
    bool isDesignerVisible() const { return designerVisible; }
    juce::Result saveAppearance();
    juce::Result loadAppearance();
    juce::Result resetAppearance();
    juce::Result getAppearanceLoadResult() const { return loadResult; }

private:
    friend juce::Result runFoleysAudioUiTests();
    void refreshRootColours();
    void activateLayout(const juce::ValueTree& layout);
    void syncDesigner();
    void updateDesignerWindow();
    juce::Result validateLayout(const juce::ValueTree& layout);
    juce::ValueTree embeddedLayout() const;
    foleys::MagicGUIState state;
    foleys::MagicGUIBuilder builder {state};
    bool designerRequested = false;
    bool designerVisible = false;
    bool designerReady = false;
    juce::Component::SafePointer<juce::Component> designerHost;
    juce::File layoutFile;
    juce::Result loadResult = juce::Result::ok();
    UiTheme theme;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPanel)
};
}
