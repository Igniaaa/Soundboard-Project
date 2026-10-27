#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <vector>

namespace dualdeck
{
// Debug-only overlay. It intercepts input so editing cannot activate soundboard controls.
class UiLayoutEditor final : public juce::Component
{
public:
    UiLayoutEditor(juce::Component& owner, juce::File layoutFile, std::function<void(int)> showView);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void applyOverrides();
    void load();

private:
    struct Offset
    {
        int x = 0, y = 0, width = 0, height = 0;
        bool operator==(const Offset& other) const
        {
            return x == other.x && y == other.y && width == other.width && height == other.height;
        }
        bool isZero() const { return x == 0 && y == 0 && width == 0 && height == 0; }
    };
    struct Hit { juce::Component* component = nullptr; juce::String key; };
    Hit hitAt(juce::Component&, const juce::String&, juce::Point<int>) const;
    void assignIds(juce::Component&);
    juce::String childKey(const juce::Component&, const juce::String&) const;
    void applyToChildren(juce::Component&, const juce::String&);
    void save();
    void reset();
    void undo();
    void pushUndo();
    void refreshDirty();
    void updateInfo();
    juce::String describe(const juce::Component&) const;

    juce::Component& owner;
    juce::File layoutFile;
    std::function<void(int)> showView;
    std::map<juce::String, Offset> offsets;
    std::map<juce::String, Offset> savedOffsets;
    std::vector<std::map<juce::String, Offset>> undoStack;
    juce::Component::SafePointer<juce::Component> selected, hovered;
    juce::String selectedKey;
    juce::Point<int> dragStart;
    juce::Rectangle<int> dragBounds;
    Offset dragOffset;
    bool resizing = false, dragging = false, gestureChanged = false, dirty = false;
    juce::TextButton boardButton {"Pad"}, audioButton {"Audio"}, keyboardButton {"Tastiera"};
    juce::TextButton editorButton {"Editor suono"}, saveButton {"Salva layout"}, undoButton {"Annulla"};
    juce::TextButton resetButton {"Ripristina"};
    juce::Label info;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UiLayoutEditor)
};
}
