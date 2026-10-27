#include "UiLayoutEditor.h"

namespace dualdeck
{
UiLayoutEditor::UiLayoutEditor(juce::Component& root, juce::File file, std::function<void(int)> view)
    : owner(root), layoutFile(std::move(file)), showView(std::move(view))
{
    setWantsKeyboardFocus(true);
    assignIds(owner);
    for (auto* button : {&boardButton, &audioButton, &keyboardButton, &editorButton, &saveButton, &undoButton, &resetButton})
        addAndMakeVisible(button);
    addAndMakeVisible(info);
    info.setColour(juce::Label::textColourId, juce::Colours::white);
    info.setFont(juce::Font(juce::FontOptions(11.0f)));
    boardButton.onClick = [this] { selected = nullptr; selectedKey.clear(); showView(0); updateInfo(); };
    audioButton.onClick = [this] { selected = nullptr; selectedKey.clear(); showView(1); updateInfo(); };
    keyboardButton.onClick = [this] { selected = nullptr; selectedKey.clear(); showView(2); updateInfo(); };
    editorButton.onClick = [this] { selected = nullptr; selectedKey.clear(); showView(3); updateInfo(); };
    saveButton.onClick = [this] { save(); };
    undoButton.onClick = [this] { undo(); };
    resetButton.onClick = [this] { reset(); };
    undoButton.setEnabled(false);
    updateInfo();
}

void UiLayoutEditor::resized()
{
    int x = 218;
    for (auto* button : {&boardButton, &audioButton, &keyboardButton, &editorButton, &saveButton, &undoButton, &resetButton})
    {
        const int width = button == &editorButton || button == &saveButton ? 112 : button == &keyboardButton ? 86 : 78;
        button->setBounds(x, 2, width, 28);
        x += width + 5;
    }
    info.setBounds(x + 4, 2, juce::jmax(100, getWidth() - x - 24), 28);
}

void UiLayoutEditor::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xf20c1827));
    g.fillRoundedRectangle(210.0f, 0.0f, static_cast<float>(juce::jmax(0, getWidth() - 210)), 32.0f, 5.0f);
    auto outline = [&](juce::Component* target, juce::Colour colour, float thickness)
    {
        if (target == nullptr || !target->isShowing()) return;
        const auto bounds = getLocalArea(target, target->getLocalBounds()).toFloat();
        g.setColour(colour);
        g.drawRect(bounds, thickness);
        if (target == selected.getComponent())
            g.fillRect(bounds.getRight() - 10.0f, bounds.getBottom() - 10.0f, 10.0f, 10.0f);
    };
    outline(hovered.getComponent(), juce::Colours::yellow.withAlpha(0.8f), 1.0f);
    outline(selected.getComponent(), juce::Colour(0xff63dfc4), 2.0f);
}

UiLayoutEditor::Hit UiLayoutEditor::hitAt(juce::Component& parent, const juce::String& path,
                                         juce::Point<int> point) const
{
    for (int i = parent.getNumChildComponents() - 1; i >= 0; --i)
    {
        auto* child = parent.getChildComponent(i);
        if (child == this || !child->isVisible()) continue;
        const auto bounds = getLocalArea(child, child->getLocalBounds());
        if (!bounds.contains(point)) continue;
        const auto key = childKey(*child, path);
        const bool isControl = dynamic_cast<juce::Button*>(child) != nullptr
            || dynamic_cast<juce::Slider*>(child) != nullptr
            || dynamic_cast<juce::ComboBox*>(child) != nullptr
            || dynamic_cast<juce::TextEditor*>(child) != nullptr;
        if (!isControl)
            if (auto deeper = hitAt(*child, key, point); deeper.component != nullptr) return deeper;
        return {child, key};
    }
    return {};
}

void UiLayoutEditor::assignIds(juce::Component& parent)
{
    for (int i = 0; i < parent.getNumChildComponents(); ++i)
    {
        auto* child = parent.getChildComponent(i);
        if (child == this) continue;
        if (child->getComponentID().isEmpty()) child->setComponentID("ui-" + juce::String(i));
        assignIds(*child);
    }
}

juce::String UiLayoutEditor::childKey(const juce::Component& child, const juce::String& path) const
{
    return path + "/" + child.getComponentID();
}

void UiLayoutEditor::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto hit = hitAt(owner, {}, event.getPosition());
    selected = hit.component;
    selectedKey = hit.key;
    hovered = hit.component;
    dragging = selected != nullptr;
    gestureChanged = false;
    if (auto* target = selected.getComponent())
    {
        dragStart = event.getPosition();
        dragBounds = target->getBounds();
        const auto found = offsets.find(selectedKey);
        dragOffset = found != offsets.end() ? found->second : Offset {};
        const auto screenBounds = getLocalArea(target, target->getLocalBounds());
        resizing = event.x >= screenBounds.getRight() - 14 && event.y >= screenBounds.getBottom() - 14;
    }
    updateInfo();
    repaint();
}

void UiLayoutEditor::mouseDrag(const juce::MouseEvent& event)
{
    auto* target = selected.getComponent();
    if (!dragging || target == nullptr) return;
    const auto difference = event.getPosition() - dragStart;
    auto bounds = dragBounds;
    auto next = dragOffset;
    if (resizing)
    {
        bounds.setSize(juce::jmax(16, dragBounds.getWidth() + difference.x),
                       juce::jmax(16, dragBounds.getHeight() + difference.y));
        next.width += bounds.getWidth() - dragBounds.getWidth();
        next.height += bounds.getHeight() - dragBounds.getHeight();
    }
    else
    {
        bounds.translate(difference.x, difference.y);
        next.x += difference.x;
        next.y += difference.y;
    }
    if (!gestureChanged && bounds != dragBounds)
    {
        pushUndo();
        gestureChanged = true;
    }
    if (!gestureChanged) return;
    target->setBounds(bounds);
    if (next.isZero()) offsets.erase(selectedKey);
    else offsets[selectedKey] = next;
    refreshDirty();
    updateInfo();
    repaint();
}

void UiLayoutEditor::mouseUp(const juce::MouseEvent&)
{
    if (gestureChanged && !undoStack.empty() && undoStack.back() == offsets)
        undoStack.pop_back();
    dragging = false;
    gestureChanged = false;
    undoButton.setEnabled(!undoStack.empty());
}

bool UiLayoutEditor::keyPressed(const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    if (key.getModifiers().isCtrlDown() && !key.getModifiers().isAltDown()
        && !key.getModifiers().isShiftDown() && (code == 'z' || code == 'Z'))
    {
        undo();
        return true;
    }
    return false;
}

void UiLayoutEditor::mouseMove(const juce::MouseEvent& event)
{
    hovered = hitAt(owner, {}, event.getPosition()).component;
    repaint();
}

void UiLayoutEditor::applyOverrides()
{
    applyToChildren(owner, {});
    repaint();
}

void UiLayoutEditor::applyToChildren(juce::Component& parent, const juce::String& path)
{
    for (int i = 0; i < parent.getNumChildComponents(); ++i)
    {
        auto* child = parent.getChildComponent(i);
        if (child == this) continue;
        const auto key = childKey(*child, path);
        if (const auto found = offsets.find(key); found != offsets.end())
        {
            auto bounds = child->getBounds();
            const auto& change = found->second;
            bounds.setBounds(bounds.getX() + change.x, bounds.getY() + change.y,
                             juce::jmax(16, bounds.getWidth() + change.width),
                             juce::jmax(16, bounds.getHeight() + change.height));
            child->setBounds(bounds);
        }
        applyToChildren(*child, key);
    }
}

void UiLayoutEditor::load()
{
    if (!layoutFile.existsAsFile()) return;
    const auto parsed = juce::JSON::parse(layoutFile.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    auto* elements = root != nullptr ? root->getProperty("elements").getDynamicObject() : nullptr;
    if (elements == nullptr) { info.setText("Layout JSON non valido", juce::dontSendNotification); return; }
    offsets.clear();
    for (const auto& property : elements->getProperties())
    {
        auto* item = property.value.getDynamicObject();
        if (item == nullptr) continue;
        offsets[property.name.toString()] = {static_cast<int>(item->getProperty("x")),
                                             static_cast<int>(item->getProperty("y")),
                                             static_cast<int>(item->getProperty("width")),
                                             static_cast<int>(item->getProperty("height"))};
    }
    savedOffsets = offsets;
    undoStack.clear();
    refreshDirty();
    updateInfo();
}

void UiLayoutEditor::save()
{
    auto* elements = new juce::DynamicObject();
    for (const auto& [key, value] : offsets)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty("x", value.x); item->setProperty("y", value.y);
        item->setProperty("width", value.width); item->setProperty("height", value.height);
        elements->setProperty(key, juce::var(item));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty("format", 1);
    root->setProperty("elements", juce::var(elements));
    if (layoutFile.getParentDirectory().createDirectory()
        && layoutFile.replaceWithText(juce::JSON::toString(juce::var(root), true)))
    {
        savedOffsets = offsets;
        refreshDirty();
        updateInfo();
    }
    else info.setText("Impossibile salvare " + layoutFile.getFullPathName(), juce::dontSendNotification);
}

void UiLayoutEditor::reset()
{
    if (offsets.empty()) return;
    pushUndo();
    offsets.clear();
    selected = nullptr; selectedKey.clear();
    refreshDirty();
    owner.resized();
    updateInfo();
}

void UiLayoutEditor::undo()
{
    if (undoStack.empty()) return;
    offsets = std::move(undoStack.back());
    undoStack.pop_back();
    dragging = false;
    gestureChanged = false;
    refreshDirty();
    owner.resized();
    updateInfo();
}

void UiLayoutEditor::pushUndo()
{
    constexpr std::size_t maxUndoSteps = 100;
    if (undoStack.size() == maxUndoSteps) undoStack.erase(undoStack.begin());
    undoStack.push_back(offsets);
    undoButton.setEnabled(true);
}

void UiLayoutEditor::refreshDirty()
{
    dirty = offsets != savedOffsets;
    undoButton.setEnabled(!undoStack.empty());
}

juce::String UiLayoutEditor::describe(const juce::Component& component) const
{
    if (auto* label = dynamic_cast<const juce::Label*>(&component)) return label->getText().substring(0, 36);
    if (auto* button = dynamic_cast<const juce::Button*>(&component)) return button->getButtonText();
    if (auto* editor = dynamic_cast<const juce::TextEditor*>(&component)) return editor->getText().substring(0, 36);
    if (auto* combo = dynamic_cast<const juce::ComboBox*>(&component)) return combo->getText();
    return component.getName();
}

void UiLayoutEditor::updateInfo()
{
    if (auto* target = selected.getComponent())
    {
        const auto bounds = target->getBounds();
        info.setText((dirty ? "* " : "") + selectedKey + "  " + describe(*target)
                     + "    x " + juce::String(bounds.getX()) + "  y " + juce::String(bounds.getY())
                     + "  w " + juce::String(bounds.getWidth()) + "  h " + juce::String(bounds.getHeight())
                     + "    Ctrl+Z = annulla",
                     juce::dontSendNotification);
    }
    else info.setText((dirty ? "* " : "") + juce::String("Trascina; angolo = dimensione; Ctrl+Z = annulla"),
                      juce::dontSendNotification);
}
}
