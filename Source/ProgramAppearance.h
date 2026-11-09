#pragma once
#include "UiTheme.h"

namespace dualdeck
{
// Foleys edits a visual preview; live widgets keep their owners, state and callbacks.
class ProgramAppearance final
{
public:
    explicit ProgramAppearance(std::function<void(const UiTheme&)> themeChanged);
    ~ProgramAppearance();
    void registerControl(const juce::String& id, juce::Component& control, juce::Component& layoutRoot, bool movable = true);
    void applyLayout(juce::Component& layoutRoot);
    void refreshStyle(const juce::String& id);
    void retireControls(const juce::StringArray& ids);
    juce::Result loadAppearance(const juce::File&);
    juce::Result saveAppearance();
    juce::Result resetAppearance();
    juce::ValueTree findNode(const juce::String& id) const;
    void showDesigner(juce::Component& previewRoot);
    void hideDesigner();
    bool isDesignerVisible() const;
private:
    friend juce::Result runProgramAppearancePreviewTests();
    struct Impl;
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgramAppearance)
};
}
