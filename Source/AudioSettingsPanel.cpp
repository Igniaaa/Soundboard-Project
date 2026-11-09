#include "AudioSettingsPanel.h"
#include "AudioSettingsXml.h"
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
#include <foleys_gui_magic/Editor/foleys_ToolBox.h>
#endif
#include <map>
#include <optional>
#include <set>

namespace dualdeck
{
namespace
{
int colourRole(juce::Colour colour)
{
    const UiTheme defaults;
    for (std::size_t i = 0; i < defaults.colours.size(); ++i) if (colour == defaults.colours[i]) return static_cast<int>(i);
    return -1;
}
void linkDefaultRoles(juce::ValueTree tree, const UiTheme& palette)
{
    auto link = [&](juce::ValueTree node, const char* property, int role)
    {
        const juce::Identifier tag("theme-role-" + juce::String(property));
        if (!node.isValid() || node.hasProperty(tag)) return;
        if (node[property].toString() == palette.colours[static_cast<std::size_t>(role)].toString()) node.setProperty(tag, role, nullptr);
    };
    link(tree.getChildWithName("View"), "background-color", 1);
    auto classes = tree.getChildWithName("Styles").getChild(0).getChildWithName("Classes");
    link(classes.getChildWithName("section"), "background-color", 2);
    link(classes.getChildWithName("section"), "border-color", 3);
    link(classes.getChildWithName("primary"), "button-color", 6);
    link(classes.getChildWithName("primary"), "text-color", 0);
}
void translateTreeColours(juce::ValueTree tree, const UiTheme& before, const UiTheme& after)
{
    for (int i = 0; i < tree.getNumProperties(); ++i)
    {
        const auto name = tree.getPropertyName(i); const auto value = tree[name].toString();
        const juce::Identifier tag("theme-role-" + name.toString());
        if (!name.toString().startsWith("theme-role-") && tree.hasProperty(tag))
        {
            const int role = tree[tag];
            if (role < 0 || role >= 9) continue;
            // A colour explicitly changed in the audio designer becomes an independent override.
            if (value == before.colours[static_cast<std::size_t>(role)].toString()) tree.setProperty(name, after.colours[static_cast<std::size_t>(role)].toString(), nullptr);
            else tree.removeProperty(tag, nullptr);
        }
    }
    for (auto child : tree) translateTreeColours(child, before, after);
}
const juce::StringArray paletteKeys {"background", "panel", "tile", "line", "text", "muted", "accent", "blue", "warning"};
void storePalette(juce::ValueTree tree, const UiTheme& palette)
{
    auto node = tree.getOrCreateChildWithName("ProgramPalette", nullptr);
    for (int i = 0; i < paletteKeys.size(); ++i) node.setProperty(juce::Identifier(paletteKeys[i]), palette.colours[static_cast<std::size_t>(i)].toString(), nullptr);
}
UiTheme storedPalette(juce::ValueTree tree)
{
    UiTheme palette; const auto node = tree.getChildWithName("ProgramPalette");
    for (int i = 0; i < paletteKeys.size(); ++i)
        if (node.hasProperty(juce::Identifier(paletteKeys[i]))) palette.colours[static_cast<std::size_t>(i)] = juce::Colour::fromString(node[juce::Identifier(paletteKeys[i])].toString());
    return palette;
}
std::vector<std::pair<juce::String, int>> colourMapping(juce::Component* component)
{
    if (dynamic_cast<juce::TextButton*>(component))
        return {{"button-color", juce::TextButton::buttonColourId}, {"text-color", juce::TextButton::textColourOffId}};
    if (dynamic_cast<juce::ToggleButton*>(component))
        return {{"text-color", juce::ToggleButton::textColourId}, {"tick-color", juce::ToggleButton::tickColourId}};
    if (dynamic_cast<juce::ComboBox*>(component))
        return {{"control-background", juce::ComboBox::backgroundColourId}, {"text-color", juce::ComboBox::textColourId},
                {"control-outline", juce::ComboBox::outlineColourId}, {"arrow-color", juce::ComboBox::arrowColourId}};
    if (dynamic_cast<juce::Slider*>(component))
        return {{"slider-track", juce::Slider::trackColourId}, {"slider-thumb", juce::Slider::thumbColourId},
                {"slider-background", juce::Slider::backgroundColourId}, {"text-color", juce::Slider::textBoxTextColourId}};
    if (dynamic_cast<juce::Label*>(component)) return {{"text-color", juce::Label::textColourId}};
    return {};
}

struct ControlRegistry
{
    struct Entry
    {
        juce::Component::SafePointer<juce::Component> component;
        std::vector<std::pair<int, juce::Colour>> colours;
        std::vector<int> colourRoles;
        std::optional<juce::Font> font;
    };
    explicit ControlRegistry(std::initializer_list<AudioSettingsPanel::Control> source)
    {
        for (const auto& sourceControl : source)
        {
            auto& entry = controls[sourceControl.id];
            entry.component = &sourceControl.component;
            for (const auto& mapping : colourMapping(&sourceControl.component))
            {
                entry.colours.emplace_back(mapping.second, sourceControl.component.findColour(mapping.second, true));
                entry.colourRoles.push_back(colourRole(entry.colours.back().second));
            }
            if (auto* label = dynamic_cast<juce::Label*>(&sourceControl.component)) entry.font = label->getFont();
        }
    }
    std::map<juce::String, Entry> controls;
};

class AudioControlItem final : public foleys::GuiItem
{
public:
    AudioControlItem(foleys::MagicGUIBuilder& builder, const juce::ValueTree& node)
        : GuiItem(builder, node)
    {
        if (auto* registry = getMagicState().getObjectWithType<ControlRegistry>("audioControls"))
        {
            const auto found = registry->controls.find(node.getProperty("id").toString());
            if (found != registry->controls.end())
            {
                control = found->second.component;
                originalColours = found->second.colours;
                originalFont = found->second.font;
            }
        }
        if (control == nullptr)
        {
            // The designer requests an unbound prototype (no ID) to discover
            // editable properties. Supply the supported names without a widget.
            setColourTranslation({{"text-color", 0}, {"button-color", 0}, {"tick-color", 0},
                {"control-background", 0}, {"control-outline", 0}, {"arrow-color", 0},
                {"slider-track", 0}, {"slider-thumb", 0}, {"slider-background", 0}});
            return;
        }
        title = control->getTitle(); description = control->getDescription();
        help = control->getHelpText(); accessible = control->isAccessible();
        focusOrder = control->getExplicitFocusOrder();
        addAndMakeVisible(control.getComponent());
        setColourTranslation(colourMapping(control.getComponent()));
    }

    ~AudioControlItem() override
    {
        // Foleys can construct the replacement tree before destroying the old one.
        if (control != nullptr && control->getParentComponent() == this)
            removeChildComponent(control.getComponent());
    }

    juce::Component* getWrappedComponent() override { return control.getComponent(); }
    void updateColours() override
    {
        if (control != nullptr)
            for (const auto& colour : originalColours) control->setColour(colour.first, colour.second);
        GuiItem::updateColours();
    }
    std::vector<foleys::SettableProperty> getSettableProperties() const override
    {
        if (originalFont || control == nullptr)
            return {{configNode, "font-size", foleys::SettableProperty::Number, originalFont ? originalFont->getHeight() : 14.0f, {}}};
        return {};
    }
    void update() override
    {
        if (control == nullptr) return;
        // GuiItem configures these automatically; retain the controller's metadata
        // unless the layout explicitly supplies a replacement.
        control->setTitle(configNode.hasProperty("title") ? configNode["title"].toString() : title);
        control->setDescription(configNode.hasProperty("description") ? configNode["description"].toString() : description);
        control->setHelpText(configNode.hasProperty("help-text") ? configNode["help-text"].toString() : help);
        control->setAccessible(accessible);
        control->setExplicitFocusOrder(focusOrder);
        if (originalFont)
            if (auto* label = dynamic_cast<juce::Label*>(control.getComponent()))
            {
                juce::ValueTree definedHere;
                const auto size = magicBuilder.getStylesheet().getStyleProperty("font-size", configNode, true, &definedHere);
                label->setFont(!definedHere.isValid() ? *originalFont : originalFont->withHeight(juce::jlimit(8.0f, 40.0f, static_cast<float>(size))));
            }
    }

private:
    juce::Component::SafePointer<juce::Component> control;
    juce::String title, description, help;
    bool accessible = true;
    int focusOrder = 0;
    std::vector<std::pair<int, juce::Colour>> originalColours;
    std::optional<juce::Font> originalFont;
};
}

AudioSettingsPanel::AudioSettingsPanel(std::initializer_list<Control> controls, const juce::File& appearanceFile)
    : layoutFile(appearanceFile)
{
    setComponentID("foleysAudioSettings");
    state.createAndAddObject<ControlRegistry>("audioControls", controls);
    builder.registerJUCEFactories();
    builder.registerFactory("AudioControl", [](foleys::MagicGUIBuilder& gui, const juce::ValueTree& node) -> std::unique_ptr<foleys::GuiItem>
    { return std::make_unique<AudioControlItem>(gui, node); });
    loadResult = loadAppearance();
}

void AudioSettingsPanel::paint(juce::Graphics&) { refreshRootColours(); }
void AudioSettingsPanel::refreshRootColours()
{
    // Upstream RootItem updates tooltip/child colours but omits its own decorator.
    // Refresh before children paint so designer Refresh/reload is covered too.
    // This base call only updates decorator colours; it does not request a repaint.
    if (auto* root = builder.findGuiItem(builder.getGuiRootNode()))
    {
        root->foleys::GuiItem::updateColours();
    }
}
void AudioSettingsPanel::resized() { builder.updateLayout(getLocalBounds()); refreshRootColours(); }
void AudioSettingsPanel::rebuild() { builder.updateComponents(); resized(); }
void AudioSettingsPanel::applyTheme(const UiTheme& next)
{
    if (theme.colours == next.colours) { theme = next; return; }
    linkDefaultRoles(state.getGuiTree(), theme);
    translateTreeColours(state.getGuiTree(), theme, next);
    if (auto* registry = state.getObjectWithType<ControlRegistry>("audioControls"))
        for (auto& entry : registry->controls)
            for (std::size_t i = 0; i < entry.second.colours.size(); ++i)
                if (entry.second.colourRoles[i] >= 0) entry.second.colours[i].second = next.colours[static_cast<std::size_t>(entry.second.colourRoles[i])];
    theme = next; storePalette(state.getGuiTree(), theme); rebuild();
}
void AudioSettingsPanel::showDesigner()
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    designerVisible = true;
    if (!designerRequested)
    {
        designerRequested = true;
        builder.attachToolboxToWindow(*this);
    }
    syncDesigner();
#endif
}

void AudioSettingsPanel::hideDesigner()
{
    designerVisible = false;
    if (designerRequested) syncDesigner();
}

void AudioSettingsPanel::syncDesigner()
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (designerReady) { updateDesignerWindow(); return; }
    juce::Component::SafePointer<AudioSettingsPanel> safe(this);
    juce::MessageManager::callAsync([safe]
    {
        if (safe == nullptr) return;
        safe->designerReady = true;
        safe->designerHost = safe->getTopLevelComponent();
        safe->updateDesignerWindow();
    });
#endif
}

void AudioSettingsPanel::updateDesignerWindow()
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    auto& toolbox = builder.getMagicToolBox();
    // Upstream registers editor shortcuts on the host window even when hidden.
    if (designerHost != nullptr)
    {
        designerHost->removeKeyListener(&toolbox);
        if (designerVisible) designerHost->addKeyListener(&toolbox);
    }
    toolbox.setVisible(designerVisible);
    for (int i = 0; i < toolbox.getNumChildComponents(); ++i)
        if (auto* button = dynamic_cast<juce::TextButton*>(toolbox.getChildComponent(i)))
            if (button->getClickingTogglesState()) button->setToggleState(designerVisible, juce::sendNotificationSync);
    builder.setEditMode(designerVisible);
    if (designerVisible) toolbox.toFront(true);
#endif
}

juce::ValueTree AudioSettingsPanel::embeddedLayout() const
{
    auto layout = juce::ValueTree::fromXml(juce::String::fromUTF8(audioSettingsXml, static_cast<int>(sizeof(audioSettingsXml) - 1)));
    linkDefaultRoles(layout, UiTheme{});
    translateTreeColours(layout, UiTheme{}, theme);
    storePalette(layout, theme);
    return layout;
}

void AudioSettingsPanel::activateLayout(const juce::ValueTree& layout)
{
    const auto loadedTheme = storedPalette(layout);
    linkDefaultRoles(layout, loadedTheme);
    if (auto* registry = state.getObjectWithType<ControlRegistry>("audioControls"))
        for (auto& entry : registry->controls)
            for (std::size_t i = 0; i < entry.second.colours.size(); ++i)
                if (entry.second.colourRoles[i] >= 0) entry.second.colours[i].second = loadedTheme.colours[static_cast<std::size_t>(entry.second.colourRoles[i])];
    theme = loadedTheme;
    state.setGuiValueTree(layout);
    builder.createGUI(*this);
    resized();
    if (designerRequested) syncDesigner();
}

juce::Result AudioSettingsPanel::validateLayout(const juce::ValueTree& layout)
{
    auto* registry = state.getObjectWithType<ControlRegistry>("audioControls");
    if (!layout.hasType("magic") || !layout.getChildWithName("Styles").isValid() || !layout.getChildWithName("View").isValid())
        return juce::Result::fail("Aspetto non valido. Usa Ripristina per tornare al layout iniziale.");
    const auto factories = builder.getFactoryNames();
    std::set<juce::String> ids, controls;
    int count = 0;
    std::function<bool(const juce::ValueTree&, int)> visit = [&](const juce::ValueTree& node, int depth)
    {
        if (++count > 256 || depth > 32 || (!node.hasType("View") && !factories.contains(node.getType().toString()))) return false;
        if (!node.hasType("View") && node.getNumChildren() != 0) return false;
        const auto id = node["id"].toString();
        if (id.isNotEmpty() && !ids.insert(id).second) return false;
        if (node.hasType("AudioControl"))
        {
            if (registry == nullptr || registry->controls.count(id) == 0) return false;
            controls.insert(id);
        }
        for (auto child : node) if (!visit(child, depth + 1)) return false;
        return true;
    };
    if (!visit(layout.getChildWithName("View"), 0) || registry == nullptr || controls.size() != registry->controls.size())
        return juce::Result::fail("L'aspetto deve conservare tutti i controlli audio e i loro ID. Usa Ripristina se necessario.");
    return juce::Result::ok();
}

juce::Result AudioSettingsPanel::loadAppearance()
{
    if (layoutFile == juce::File() || !layoutFile.exists())
    {
        activateLayout(embeddedLayout());
        return juce::Result::ok();
    }
    auto layout = layoutFile.existsAsFile() && layoutFile.getSize() <= 512 * 1024
        ? juce::ValueTree::fromXml(layoutFile.loadFileAsString()) : juce::ValueTree();
    auto result = validateLayout(layout);
    activateLayout(result.wasOk() ? layout : embeddedLayout());
    return result;
}

juce::Result AudioSettingsPanel::saveAppearance()
{
    const auto valid = validateLayout(state.getGuiTree());
    if (valid.failed()) return valid;
    if (layoutFile == juce::File() || layoutFile.getParentDirectory().createDirectory().failed())
        return juce::Result::fail("Impossibile creare il file dell'aspetto.");
    juce::TemporaryFile temporary(layoutFile);
    auto xml = state.getGuiTree().createXml();
    if (!xml || !xml->writeTo(temporary.getFile()))
        return juce::Result::fail("Impossibile scrivere il file dell'aspetto.");
    if (temporary.getFile().getSize() > 512 * 1024)
        return juce::Result::fail("Aspetto troppo grande: riduci le personalizzazioni prima di salvare.");
    if (!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Impossibile salvare l'aspetto. Controlla che la cartella sia scrivibile.");
    return juce::Result::ok();
}

juce::Result AudioSettingsPanel::resetAppearance()
{
    const auto previous = state.getGuiTree().createCopy();
    activateLayout(embeddedLayout());
    const auto result = saveAppearance();
    if (result.failed()) activateLayout(previous);
    return result;
}
}
