#pragma once
#include <foleys_gui_magic/foleys_gui_magic.h>
#include "UiTheme.h"

namespace dualdeck
{
class MainScreenPanel final : public juce::Component, private juce::ValueTree::Listener, private juce::AsyncUpdater, private juce::ComponentListener
{
public:
    struct Control { juce::String id, type; juce::Component& component; };
    MainScreenPanel(const std::vector<Control>&, const juce::File& appearanceFile = {}, const juce::File& legacyFile = {});
    ~MainScreenPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void childrenChanged() override;
    void rebuild();
    void applyTheme(const UiTheme&);
    void setScreen(int view, bool keyboardMode, int visiblePads, bool empty);
    void refreshControl(const juce::String& id, bool rememberMetadata = false);
    juce::ValueTree findNode(const juce::String& id);
    juce::Component* wrappedControl(const juce::String& id);
    juce::Rectangle<int> elementBounds(const juce::String& id);
    juce::StringArray controlIds();
    juce::Result loadAppearance();
    juce::Result importLayout(const juce::File&);
    juce::Result saveAppearance();
    juce::Result resetAppearance();
    juce::Result getAppearanceLoadResult() const { return loadResult; }
    void showDesigner();
    void hideDesigner();
    bool isDesignerVisible() const { return designerVisible; }
    std::function<void()> onLayoutChanged;
    std::function<void(const juce::Result&)> onAppearanceResult;
private:
    friend juce::Result runMainScreenUiTests();
    void activateLayout(juce::ValueTree);
    juce::ValueTree embeddedLayout() const;
    juce::Result validateLayout(juce::ValueTree);
    void syncVisibility();
    void observeVisibility();
    void componentVisibilityChanged(juce::Component&) override;
    void componentBeingDeleted(juce::Component&) override;
    void updateDesignerWindow();
    void handleAsyncUpdate() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    foleys::MagicGUIState state;
    foleys::MagicGUIBuilder builder {state};
    juce::ValueTree observedTree;
    juce::File layoutFile, legacyFile;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Result loadResult = juce::Result::ok();
    UiTheme theme;
    bool updating = false, designerVisible = false, designerRequested = false, designerReady = false;
    int section = 0, padCount = 0;
    bool keyboard = false, empty = true;
    bool syncingVisibility = false, rebuildingVisibility = false, closing = false;
    std::vector<juce::Component::SafePointer<juce::Component>> visibilitySources;
    juce::Component::SafePointer<juce::Component> designerHost;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainScreenPanel)
};
}
