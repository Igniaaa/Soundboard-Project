#include "MainScreenPanel.h"
#include "MainScreenXml.h"
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
#include <foleys_gui_magic/Editor/foleys_ToolBox.h>
#endif
#include <map>
#include <optional>
#include <set>
#include <cmath>
#include <cstdlib>

namespace dualdeck
{
namespace
{
const juce::StringArray paletteNames {"background", "panel", "tile", "line", "text", "muted", "accent", "blue", "warning"};
std::vector<std::pair<juce::String, int>> colourMapping(const juce::String& type)
{
    if (type == "Button") return {{"button-color", juce::TextButton::buttonColourId}, {"button-on-color", juce::TextButton::buttonOnColourId},
        {"text-color", juce::TextButton::textColourOffId}, {"text-on-color", juce::TextButton::textColourOnId}};
    if (type == "ToggleButton") return {{"text-color", juce::ToggleButton::textColourId}, {"tick-color", juce::ToggleButton::tickColourId}};
    if (type == "ComboBox") return {{"control-background", juce::ComboBox::backgroundColourId}, {"text-color", juce::ComboBox::textColourId}, {"control-outline", juce::ComboBox::outlineColourId}, {"arrow-color", juce::ComboBox::arrowColourId}};
    if (type == "Slider") return {{"slider-track", juce::Slider::trackColourId}, {"slider-thumb", juce::Slider::thumbColourId}, {"slider-background", juce::Slider::backgroundColourId}, {"text-color", juce::Slider::textBoxTextColourId}};
    if (type == "Label") return {{"text-color", juce::Label::textColourId}};
    if (type == "TextEditor") return {{"control-background", juce::TextEditor::backgroundColourId}, {"text-color", juce::TextEditor::textColourId}, {"control-outline", juce::TextEditor::outlineColourId}, {"accent-color", juce::TextEditor::focusedOutlineColourId}};
    return {{"surface-color", uiColours::tile}, {"panel-color", uiColours::panel}, {"canvas-color", uiColours::background}, {"text-color", uiColours::text},
        {"muted-color", uiColours::muted}, {"accent-color", uiColours::accent}, {"line-color", uiColours::line}, {"warning-color", uiColours::warning}};
}
struct LiveRegistry
{
    struct Colour { int id, role; juce::Colour original; bool specified; };
    struct Entry
    {
        juce::String type, title, description, help;
        juce::Component::SafePointer<juce::Component> component;
        std::vector<Colour> colours;
        std::optional<juce::Font> font;
        bool accessible = true; int focusOrder = 0;
        void rememberMetadata()
        {
            if (!component) return;
            title = component->getTitle(); description = component->getDescription(); help = component->getHelpText();
            accessible = component->isAccessible(); focusOrder = component->getExplicitFocusOrder();
        }
    };
    std::map<juce::String, Entry> entries;
};
double number(juce::var value, double fallback, double low, double high)
{
    if (value.isVoid()) return fallback;
    const double parsed = value.toString().getDoubleValue();
    return std::isfinite(parsed) ? juce::jlimit(low, high, parsed) : fallback;
}
class LiveControlItem final : public foleys::GuiItem
{
public:
    LiveControlItem(foleys::MagicGUIBuilder& b, const juce::ValueTree& n) : GuiItem(b, n)
    {
        setColourTranslation(colourMapping(n.getType().toString()));
        if (auto* registry = getMagicState().getObjectWithType<LiveRegistry>("mainControls"))
        {
            const auto found = registry->entries.find(n["id"].toString());
            if (found != registry->entries.end()) entry = &found->second;
        }
        if (entry && entry->component) addAndMakeVisible(entry->component.getComponent());
    }
    ~LiveControlItem() override
    {
        if (entry && entry->component && entry->component->getParentComponent() == this) removeChildComponent(entry->component.getComponent());
    }
    juce::Component* getWrappedComponent() override { return entry ? entry->component.getComponent() : nullptr; }
    juce::var explicitProperty(const char* name) const
    {
        juce::ValueTree defined;
        const auto value = magicBuilder.getStylesheet().getStyleProperty(name, configNode, true, &defined);
        return defined.isValid() ? value : juce::var();
    }
    void updateColours() override
    {
        if (entry && entry->component)
        {
            auto& c = *entry->component;
            for (const auto& colour : entry->colours)
            {
                if (colour.role >= 0) c.setColour(colour.id, c.getLookAndFeel().findColour(uiColours::background + colour.role));
                else if (colour.specified) c.setColour(colour.id, colour.original);
                else c.removeColour(colour.id);
            }
            if (dynamic_cast<juce::Label*>(&c) && c.getProperties().contains("appearance-text-role"))
                c.setColour(juce::Label::textColourId, uiColour(c, static_cast<int>(c.getProperties()["appearance-text-role"])));
        }
        GuiItem::updateColours();
    }
    void update() override
    {
        if (!entry || !entry->component) return;
        auto& c = *entry->component;
        c.setTitle(configNode.hasProperty("accessibility-title") ? configNode["accessibility-title"].toString() : entry->title);
        c.setDescription(configNode.hasProperty("accessibility-description") ? configNode["accessibility-description"].toString() : entry->description);
        c.setHelpText(entry->help); c.setAccessible(entry->accessible); c.setExplicitFocusOrder(entry->focusOrder);
        const auto scale = number(explicitProperty("font-scale"), 1.0, 0.6, 2.0);
        const auto size = number(explicitProperty("font-size"), 0.0, 0.0, 40.0);
        const bool changed = static_cast<double>(c.getProperties().getWithDefault("appearance-font-scale", 1.0)) != scale
            || static_cast<double>(c.getProperties().getWithDefault("appearance-font-size", 0.0)) != size;
        c.getProperties().set("appearance-font-scale", scale); c.getProperties().set("appearance-font-size", size);
        if (changed) c.sendLookAndFeelChange();
        if (entry->font)
        {
            auto font = size <= 0 ? *entry->font : entry->font->withHeight(static_cast<float>(juce::jmax(8.0, size)));
            if (auto* label = dynamic_cast<juce::Label*>(&c)) label->setFont(font);
            if (auto* text = dynamic_cast<juce::TextEditor*>(&c)) text->applyFontToAllText(font.withHeight(font.getHeight() * uiFontScale(c)));
        }
    }
    std::vector<foleys::SettableProperty> getSettableProperties() const override
    { return {{configNode, "font-size", foleys::SettableProperty::Number, 0, {}}, {configNode, "font-scale", foleys::SettableProperty::Number, 1.0, {}}}; }
private:
    LiveRegistry::Entry* entry = nullptr;
};
class PadGridItem final : public foleys::Container
{
public:
    PadGridItem(foleys::MagicGUIBuilder& b, const juce::ValueTree& n) : Container(b, n) {}
    void resized() override { updateLayout(); }
    void updateLayout() override { Container::updateLayout(); arrange(); }
    void arrange()
    {
        const int count = static_cast<int>(std::distance(begin(), end())); if (count == 0) return;
        const int gap = static_cast<int>(number(getProperty("gap"), 9, 0, 40));
        const int minWidth = static_cast<int>(number(getProperty("min-pad-width"), 120, 80, 400));
        const int minHeight = static_cast<int>(number(getProperty("min-pad-height"), 100, 60, 400));
        const int maxColumns = static_cast<int>(number(getProperty("max-columns"), 6, 1, 12));
        const auto area = getClientBounds(); auto* box = (*begin())->getParentComponent();
        auto* viewport = box ? box->findParentComponentOfClass<juce::Viewport>() : nullptr;
        // The first pass may reveal a scrollbar; the second uses the reduced visible width.
        for (int pass = 0; pass < 2; ++pass)
        {
            const int available = juce::jmax(1, viewport ? viewport->getMaximumVisibleWidth() : area.getWidth());
            const int columns = juce::jlimit(1, maxColumns, (available + gap) / (minWidth + gap));
            const int rows = (count + columns - 1) / columns;
            const int width = juce::jmax(1, (available - gap * (columns - 1)) / columns);
            const int height = juce::jmax(minHeight, (area.getHeight() - gap * (rows - 1)) / rows);
            int index = 0;
            for (auto& item : *this) { item->setBounds((index % columns) * (width + gap), (index / columns) * (height + gap), width, height); ++index; }
            if (box) box->setSize(available, rows * height + (rows - 1) * gap);
        }
    }
    std::vector<foleys::SettableProperty> getSettableProperties() const override
    { return {{configNode, "min-pad-width", foleys::SettableProperty::Number, 120, {}}, {configNode, "min-pad-height", foleys::SettableProperty::Number, 100, {}},
        {configNode, "gap", foleys::SettableProperty::Number, 9, {}}, {configNode, "max-columns", foleys::SettableProperty::Number, 6, {}}}; }
};
juce::ValueTree findById(juce::ValueTree node, const juce::String& id)
{
    if (node["id"].toString() == id) return node;
    for (auto child : node) { auto result = findById(child, id); if (result.isValid()) return result; }
    return {};
}
}

MainScreenPanel::MainScreenPanel(const std::vector<Control>& controls, const juce::File& file, const juce::File& legacy)
    : layoutFile(file), legacyFile(legacy)
{
    setComponentID("foleysMainScreen");
    if (!controls.empty()) setLookAndFeel(&controls.front().component.getLookAndFeel());
    auto* registry = state.createAndAddObject<LiveRegistry>("mainControls");
    const UiTheme defaults;
    for (const auto& control : controls)
    {
        auto& entry = registry->entries[control.id]; entry.type = control.type; entry.component = &control.component; entry.rememberMetadata();
        for (const auto& mapping : colourMapping(control.type))
        {
            const auto colour = control.component.findColour(mapping.second, true); int role = -1;
            for (int i = 0; i < 9; ++i) if (colour == defaults.colours[static_cast<std::size_t>(i)]) role = i;
            entry.colours.push_back({mapping.second, role, colour, control.component.isColourSpecified(mapping.second)});
        }
        if (auto* label = dynamic_cast<juce::Label*>(&control.component)) entry.font = label->getFont();
        if (auto* text = dynamic_cast<juce::TextEditor*>(&control.component)) entry.font = text->getFont();
    }
    for (auto* type : {"Button", "ComboBox", "Slider", "ToggleButton", "TextEditor", "Label", "Pad", "Keyboard", "AudioPage", "Meter"})
        builder.registerFactory(type, [](foleys::MagicGUIBuilder& b, const juce::ValueTree& n) -> std::unique_ptr<foleys::GuiItem> { return std::make_unique<LiveControlItem>(b, n); });
    builder.registerFactory("PadGrid", [](foleys::MagicGUIBuilder& b, const juce::ValueTree& n) -> std::unique_ptr<foleys::GuiItem>
    { auto grid = std::make_unique<PadGridItem>(b, n); grid->createSubComponents(); return grid; });
    loadResult = loadAppearance();
}
MainScreenPanel::~MainScreenPanel()
{
    closing = true; hideDesigner(); cancelPendingUpdate(); observedTree.removeListener(this);
    for (auto& source : visibilitySources) if (source) source->removeComponentListener(this);
}
void MainScreenPanel::paint(juce::Graphics&)
{
    // Foleys' deferred class/media refresh can reveal sibling page containers after our layout pass.
    // Enforce controller visibility immediately before any child is rendered.
    syncVisibility();
    if (auto* root = builder.findGuiItem(builder.getGuiRootNode())) root->foleys::GuiItem::updateColours();
}
void MainScreenPanel::childrenChanged()
{
    if (updating || closing) return;
    if (auto* root = dynamic_cast<foleys::Container*>(builder.findGuiItem(builder.getGuiRootNode())))
        if (root->getParentComponent() == this) { root->foleys::Container::updateColours(); observeVisibility(); }
    triggerAsyncUpdate();
}
void MainScreenPanel::observeVisibility()
{
    for (auto& source : visibilitySources) if (source) source->removeComponentListener(this);
    visibilitySources.clear();
    auto watch = [&](juce::Component* source)
    {
        if (!source) return;
        source->addComponentListener(this); visibilitySources.emplace_back(source);
    };
    std::function<void(juce::ValueTree)> visit = [&](juce::ValueTree node)
    {
        if (auto* item = builder.findGuiItem(node)) { watch(item); watch(item->getWrappedComponent()); }
        for (auto child : node) visit(child);
    };
    visit(builder.getGuiRootNode()); rebuildingVisibility = false; syncVisibility();
}
void MainScreenPanel::componentVisibilityChanged(juce::Component&)
{ if (!updating && !syncingVisibility && !rebuildingVisibility && !closing) syncVisibility(); }
void MainScreenPanel::componentBeingDeleted(juce::Component&)
{ if (!closing) { rebuildingVisibility = true; triggerAsyncUpdate(); } }
void MainScreenPanel::resized()
{
    const juce::ScopedValueSetter<bool> guard(updating, true);
    if (auto* root = dynamic_cast<foleys::Container*>(builder.findGuiItem(builder.getGuiRootNode()))) root->foleys::Container::updateColours();
    builder.updateLayout(getLocalBounds()); applyInspectorLayout(); syncVisibility();
    if (onLayoutChanged) onLayoutChanged();
}
void MainScreenPanel::rebuild()
{
    const juce::ScopedValueSetter<bool> guard(updating, true);
    builder.updateComponents(); observeVisibility(); resized(); if (designerRequested) updateDesignerWindow();
}
juce::ValueTree MainScreenPanel::findNode(const juce::String& id) { return findById(state.getGuiTree().getChildWithName("View"), id); }
juce::Component* MainScreenPanel::wrappedControl(const juce::String& id)
{
    auto* registry = state.getObjectWithType<LiveRegistry>("mainControls"); const auto found = registry->entries.find(id);
    return found != registry->entries.end() ? found->second.component.getComponent() : nullptr;
}
juce::Rectangle<int> MainScreenPanel::elementBounds(const juce::String& id)
{
    if (auto* item = builder.findGuiItem(findNode(id))) return getLocalArea(item, item->getLocalBounds());
    return {};
}
void MainScreenPanel::setInspectorOpen(bool open)
{
    if (inspectorOpen == open) return;
    inspectorOpen = open;
    if (auto* toggle = dynamic_cast<juce::ToggleButton*>(wrappedControl("soundInspectorToggle")))
        toggle->setToggleState(inspectorOpen, juce::dontSendNotification);
    resized();
}
void MainScreenPanel::applyInspectorLayout()
{
    auto inspectorNode = findNode("inspector");
    auto* inspectorItem = builder.findGuiItem(inspectorNode);
    auto* parent = inspectorNode.isValid() ? dynamic_cast<foleys::Container*>(builder.findGuiItem(inspectorNode.getParent())) : nullptr;
    if (!inspectorItem || !parent) return;
    const auto width = inspectorOpen ? 304.0f : 0.0f;
    const auto setLayoutProperty = [&inspectorNode](const juce::Identifier& name, const juce::var& value)
    {
        if (inspectorNode.getProperty(name) != value) inspectorNode.setProperty(name, value, nullptr);
    };
    if (inspectorOpen)
    {
        setLayoutProperty("max-width", width);
        setLayoutProperty("min-width", width);
        setLayoutProperty("width", width);
    }
    else
    {
        setLayoutProperty("min-width", width);
        setLayoutProperty("max-width", width);
        setLayoutProperty("width", width);
    }
    setLayoutProperty("flex-grow", 0.0f);
    setLayoutProperty("flex-shrink", 0.0f);

    auto& flex = inspectorItem->getFlexItem();
    flex.width = flex.minWidth = flex.maxWidth = width;
    flex.flexBasis = width;
    flex.flexGrow = flex.flexShrink = 0.0f;
    flex.margin = inspectorOpen ? juce::FlexItem::Margin(0.0f, 0.0f, 0.0f, 22.0f) : juce::FlexItem::Margin();
    parent->updateLayout();
}
juce::StringArray MainScreenPanel::controlIds()
{
    juce::StringArray ids; for (const auto& entry : state.getObjectWithType<LiveRegistry>("mainControls")->entries) ids.add(entry.first); return ids;
}
void MainScreenPanel::refreshControl(const juce::String& id, bool remember)
{
    auto* registry = state.getObjectWithType<LiveRegistry>("mainControls"); const auto found = registry->entries.find(id); if (found == registry->entries.end()) return;
    if (remember) found->second.rememberMetadata();
    if (auto* item = builder.findGuiItem(findNode(id))) { item->updateColours(); item->update(); }
}
void MainScreenPanel::applyTheme(const UiTheme& next)
{
    const juce::ScopedValueSetter<bool> guard(updating, true); theme = next;
    auto palette = builder.getStylesheet().getCurrentPalette();
    for (int i = 0; i < 9; ++i) palette.setProperty(juce::Identifier(paletteNames[i]), theme.colours[static_cast<std::size_t>(i)].toString(), nullptr);
    for (const auto& id : controlIds()) refreshControl(id);
    if (auto* root = dynamic_cast<foleys::Container*>(builder.findGuiItem(builder.getGuiRootNode()))) root->foleys::Container::updateColours();
    builder.updateLayout(getLocalBounds()); repaint(); syncVisibility();
}
void MainScreenPanel::setScreen(int view, bool keyboardMode, int visiblePads, bool isEmpty)
{
    section = view; keyboard = keyboardMode; padCount = juce::jlimit(0, 24, visiblePads); empty = isEmpty;
    for (const auto& pair : std::vector<std::pair<juce::String, bool>> {{"board", section == 0}, {"settings", section != 0}, {"audio", section == 1},
        {"dedicated", section == 2}, {"pads", !keyboard}, {"keyboard", keyboard}, {"empty", empty},
#if defined(DUALDECK_FOLEYS_DESIGNER)
        {"audioActions", section == 1}
#else
        {"audioActions", false}
#endif
    }) state.getPropertyAsValue(pair.first).setValue(pair.second);
    syncVisibility();
}
void MainScreenPanel::migrateLayout(juce::ValueTree layout)
{
    if (!layout.isValid()) return;
    const auto view = layout.getChildWithName("View");
    auto toolbar = findById(view, "profileToolbar");
    auto toggle = findById(view, "soundInspectorToggle");

    if (toolbar.isValid() && !toggle.isValid())
    {
        bool hasFlexibleSpacer = false;
        for (auto child : toolbar)
            hasFlexibleSpacer = hasFlexibleSpacer || child.hasProperty("flex-grow");
        if (!hasFlexibleSpacer)
        {
            juce::ValueTree spacer("View");
            spacer.setProperty("flex-grow", "1", nullptr);
            spacer.setProperty("min-width", "0", nullptr);
            spacer.setProperty("min-height", "68", nullptr);
            spacer.setProperty("max-height", "68", nullptr);
            toolbar.addChild(spacer, -1, nullptr);
        }
        toggle = juce::ValueTree("ToggleButton");
        toggle.setProperty("id", "soundInspectorToggle", nullptr);
        toggle.setProperty("min-width", "132", nullptr);
        toggle.setProperty("max-width", "132", nullptr);
        toggle.setProperty("min-height", "38", nullptr);
        toggle.setProperty("max-height", "38", nullptr);
        toolbar.addChild(toggle, -1, nullptr);
    }
}
void MainScreenPanel::syncVisibility()
{
    if (syncingVisibility || closing) return;
    const juce::ScopedValueSetter<bool> guard(syncingVisibility, true);
    std::function<void(juce::ValueTree, bool)> visit = [&](juce::ValueTree node, bool parentVisible)
    {
        bool visible = parentVisible;
        if (node.hasProperty("visibility")) visible = visible && static_cast<bool>(state.getPropertyAsValue(node["visibility"].toString()).getValue());
        const auto id = node["id"].toString();
        if (id == "inspector") visible = visible && inspectorOpen;
        if (id.startsWith("padTile")) visible = visible && id.substring(7).getIntValue() <= padCount;
        if (auto* item = builder.findGuiItem(node)) { item->setVisible(visible); if (auto* component = item->getWrappedComponent()) component->setVisible(visible); }
        for (auto child : node) visit(child, visible);
    };
    visit(builder.getGuiRootNode(), true);
}
juce::ValueTree MainScreenPanel::embeddedLayout() const
{
    auto layout = juce::ValueTree::fromXml(juce::String::fromUTF8(mainScreenXml));
#if !defined(DUALDECK_FOLEYS_DESIGNER)
    for (const auto* id : {"editAppearance", "saveAppearance", "resetAppearance"}) { auto node = findById(layout, id); node.getParent().removeChild(node, nullptr); }
#endif
    return layout;
}
void MainScreenPanel::activateLayout(juce::ValueTree layout)
{
    const juce::ScopedValueSetter<bool> guard(updating, true);
    if (auto inspectorNode = findById(layout.getChildWithName("View"), "inspector"); inspectorNode.isValid())
    {
        const auto width = inspectorOpen ? 304.0f : 0.0f;
        inspectorNode.setProperty("width", width, nullptr);
        inspectorNode.setProperty("min-width", width, nullptr);
        inspectorNode.setProperty("max-width", width, nullptr);
        inspectorNode.setProperty("flex-grow", 0.0f, nullptr);
        inspectorNode.setProperty("flex-shrink", 0.0f, nullptr);
    }
    observedTree.removeListener(this); state.setGuiValueTree(layout); builder.createGUI(*this);
    observedTree = state.getGuiTree(); observedTree.addListener(this);
    observeVisibility();
    applyTheme(theme); resized(); if (designerReady) updateDesignerWindow();
}
juce::Result MainScreenPanel::validateLayout(juce::ValueTree tree)
{
    if (!tree.hasType("magic") || !tree.getChildWithName("Styles").isValid() || !tree.getChildWithName("View").isValid()) return juce::Result::fail("Layout principale non valido.");
    const juce::StringArray numeric {"min-width", "max-width", "width", "min-height", "max-height", "height", "pos-x", "pos-y", "pos-width", "pos-height", "font-size", "font-scale", "radius", "min-pad-width", "min-pad-height", "gap", "max-columns"};
    auto validNumber = [](juce::String value)
    {
        value = value.trim(); if (value.isEmpty() || value == "auto") return true;
        if (value.endsWithChar('%')) value = value.dropLastCharacters(1);
        auto utf8 = value.toUTF8(); char* end = nullptr; const auto parsed = std::strtod(utf8.getAddress(), &end);
        return end != utf8.getAddress() && *end == 0 && std::isfinite(parsed) && std::abs(parsed) <= 100000.0;
    };
    int total = 0;
    std::function<bool(juce::ValueTree, int)> safeProperties = [&](juce::ValueTree node, int depth)
    {
        if (++total > 2048 || depth > 32) return false;
        for (int i = 0; i < node.getNumProperties(); ++i)
        {
            const auto name = node.getPropertyName(i).toString();
            if (numeric.contains(name) && !validNumber(node[juce::Identifier(name)].toString())) return false;
            if (name == "margin" || name == "padding" || name == "border")
                for (const auto& token : juce::StringArray::fromTokens(node[juce::Identifier(name)].toString(), " ,", {})) if (!validNumber(token)) return false;
        }
        for (auto child : node) if (!safeProperties(child, depth + 1)) return false;
        return true;
    };
    if (!safeProperties(tree, 0)) return juce::Result::fail("Il layout contiene dimensioni non valide.");
    const auto& entries = state.getObjectWithType<LiveRegistry>("mainControls")->entries;
    std::set<juce::String> ids, controls; int count = 0;
    std::function<bool(juce::ValueTree, int)> visit = [&](juce::ValueTree node, int depth)
    {
        if (++count > 512 || depth > 32) return false;
        const auto id = node["id"].toString(); if (id.isNotEmpty() && !ids.insert(id).second) return false;
        if (!node.hasType("View") && !node.hasType("PadGrid"))
        {
            const auto found = entries.find(id);
            if (found == entries.end() || node.getType().toString() != found->second.type || node.getNumChildren() != 0) return false;
            controls.insert(id);
        }
        for (auto child : node) if (!visit(child, depth + 1)) return false;
        return true;
    };
    if (!visit(tree.getChildWithName("View"), 0) || controls.size() != entries.size()) return juce::Result::fail("Conserva tutti gli elementi e gli ID della schermata principale. Usa Ripristina se necessario.");
    return juce::Result::ok();
}
juce::Result MainScreenPanel::loadAppearance()
{
    juce::ValueTree layout;
    if (layoutFile.existsAsFile() && layoutFile.getSize() <= 1024 * 1024) layout = juce::ValueTree::fromXml(layoutFile.loadFileAsString());
    migrateLayout(layout);
    const auto result = layoutFile == juce::File() || !layoutFile.exists() ? juce::Result::ok() : validateLayout(layout);
    if (!layout.isValid() || result.failed()) layout = embeddedLayout();
    if (!layoutFile.exists() && legacyFile.existsAsFile() && legacyFile.getSize() <= 1024 * 1024)
    {
        const auto old = juce::ValueTree::fromXml(legacyFile.loadFileAsString());
        if (old.hasType("magic"))
        {
            // Preserve old geometry as archival data; the new screen uses native flex/grid layout.
            juce::ValueTree backup("LegacyMainAppearance");
            auto oldClasses = old.getChildWithName("Styles").getChild(0).getChildWithName("Classes");
            const auto oldNodes = old.getChildWithName("Styles").getChild(0).getChildWithName("Nodes");
            const auto oldPalette = old.getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
            auto newClasses = layout.getChildWithName("Styles").getChild(0).getChildWithName("Classes");
            auto newNodes = layout.getChildWithName("Styles").getChild(0).getChildWithName("Nodes");
            auto resolvedColour = [&](juce::var value)
            {
                auto text = value.toString(); int depth = 0;
                while (text.startsWithChar('$') && depth++ < 16 && oldPalette.hasProperty(juce::Identifier(text.substring(1)))) text = oldPalette[juce::Identifier(text.substring(1))].toString();
                return juce::var(depth >= 16 ? juce::String("00000000") : text);
            };
            auto copyAppearance = [&](juce::ValueTree source, juce::ValueTree target, bool overwrite = true)
            {
                for (int i = 0; i < source.getNumProperties(); ++i)
                {
                    const auto name = source.getPropertyName(i); const auto key = name.toString();
                    if (!overwrite && target.hasProperty(name)) continue;
                    if (key.endsWith("-color")) target.setProperty(name, resolvedColour(source[name]), nullptr);
                    else if (key == "font-size" || key == "font-scale") target.setProperty(name, source[name], nullptr);
                }
            };
            for (const auto& id : controlIds())
            {
                auto previous = findById(old.getChildWithName("View"), id);
                if (!previous.isValid()) previous = findById(old.getChildWithName("LegacyMainControls"), id);
                auto target = findById(layout.getChildWithName("View"), id);
                const auto oldStyle = oldNodes.getChildWithName(juce::Identifier(id));
                if (oldStyle.isValid())
                {
                    juce::ValueTree converted {juce::Identifier(id)}; copyAppearance(oldStyle, converted); newNodes.addChild(converted, -1, nullptr);
                }
                if (!previous.hasType("AppControl") || !target.isValid()) continue;
                backup.addChild(previous.createCopy(), -1, nullptr); copyAppearance(previous, target);
                for (const auto& name : juce::StringArray::fromTokens(previous["class"].toString(), " ", {}))
                {
                    auto source = oldClasses.getChildWithName(juce::Identifier(name)); if (!source.isValid()) continue;
                    const juce::Identifier migratedName("legacy_" + name);
                    if (!newClasses.getChildWithName(migratedName).isValid())
                    {
                        juce::ValueTree converted(migratedName); converted.copyPropertiesAndChildrenFrom(source, nullptr); copyAppearance(source, converted); newClasses.addChild(converted, -1, nullptr);
                    }
                    // ID styles preserve old appearance without replacing the new layout classes.
                    auto idStyle = newNodes.getOrCreateChildWithName(juce::Identifier(id), nullptr);
                    copyAppearance(source, idStyle, false);
                }
            }
            if (backup.getNumChildren() > 0) layout.addChild(backup, -1, nullptr);
        }
    }
    activateLayout(layout); return result;
}
juce::Result MainScreenPanel::importLayout(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024) return juce::Result::fail("File del layout principale non valido o troppo grande.");
    auto layout = juce::ValueTree::fromXml(file.loadFileAsString());
    migrateLayout(layout);
    const auto result = validateLayout(layout); if (result.failed()) return result;
    cancelPendingUpdate(); activateLayout(layout); return juce::Result::ok();
}
juce::Result MainScreenPanel::saveAppearance()
{
    const auto valid = validateLayout(state.getGuiTree()); if (valid.failed()) return valid;
    auto xml = state.getGuiTree().createXml();
    if (!xml || xml->toString().getNumBytesAsUTF8() > 1024 * 1024) return juce::Result::fail("Layout troppo grande per essere salvato.");
    if (layoutFile == juce::File() || layoutFile.getParentDirectory().createDirectory().failed()) return juce::Result::fail("Impossibile creare il file del layout principale.");
    juce::TemporaryFile temp(layoutFile);
    if (!xml->writeTo(temp.getFile()) || !temp.overwriteTargetFileWithTemporary()) return juce::Result::fail("Impossibile salvare il layout principale.");
    return juce::Result::ok();
}
juce::Result MainScreenPanel::resetAppearance()
{
    auto previous = state.getGuiTree().createCopy(); activateLayout(embeddedLayout()); auto result = saveAppearance(); if (result.failed()) activateLayout(previous); return result;
}
void MainScreenPanel::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) { if (!updating) triggerAsyncUpdate(); }
void MainScreenPanel::valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) { if (!updating) triggerAsyncUpdate(); }
void MainScreenPanel::valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) { if (!updating) triggerAsyncUpdate(); }
void MainScreenPanel::handleAsyncUpdate()
{
    const juce::ScopedValueSetter<bool> guard(updating, true); builder.updateStylesheet();
    observeVisibility();
    for (const auto& id : controlIds()) refreshControl(id);
    resized();
}
void MainScreenPanel::showDesigner()
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    designerVisible = true;
    if (!designerRequested)
    {
        designerRequested = true; builder.attachToolboxToWindow(*this);
        juce::Component::SafePointer<MainScreenPanel> safe(this);
        juce::MessageManager::callAsync([safe] { if (safe) { safe->designerReady = true; safe->designerHost = safe->getTopLevelComponent(); safe->updateDesignerWindow(); } });
    }
    else if (designerReady) updateDesignerWindow();
#endif
}
void MainScreenPanel::hideDesigner() { designerVisible = false; if (designerReady) updateDesignerWindow(); }
void MainScreenPanel::updateDesignerWindow()
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (!designerReady) return;
    auto& toolbox = builder.getMagicToolBox();
    if (designerHost) { designerHost->removeKeyListener(&toolbox); if (designerVisible) designerHost->addKeyListener(&toolbox); }
    toolbox.setVisible(designerVisible);
    for (int i = 0; i < toolbox.getNumChildComponents(); ++i)
        if (auto* button = dynamic_cast<juce::TextButton*>(toolbox.getChildComponent(i)))
        {
            if (button->getClickingTogglesState()) button->setToggleState(designerVisible, juce::sendNotificationSync);
            if (button->getButtonText().startsWith("File")) button->onClick = [this]
            {
                juce::Component::SafePointer<MainScreenPanel> safe(this);
                auto report = [safe](const juce::Result& result) { if (safe && safe->onAppearanceResult) safe->onAppearanceResult(result); };
                juce::PopupMenu menu;
                menu.addItem("Importa layout...", [safe, report]
                {
                    if (!safe) return;
                    safe->chooser = std::make_unique<juce::FileChooser>("Importa layout principale", safe->layoutFile.getParentDirectory(), "*.xml");
                    safe->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [safe, report](const juce::FileChooser& selected) { if (safe && selected.getResult().existsAsFile()) report(safe->importLayout(selected.getResult())); });
                });
                menu.addItem("Salva layout", [safe, report] { if (safe) report(safe->saveAppearance()); });
                menu.addItem("Ripristina layout", [safe, report] { if (safe) report(safe->resetAppearance()); });
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
            };
        }
    builder.setEditMode(designerVisible); syncVisibility(); if (designerVisible) toolbox.toFront(true);
#endif
}
}
