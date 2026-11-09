#include "ProgramAppearance.h"
#include <foleys_gui_magic/foleys_gui_magic.h>
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
const juce::StringArray themeNames {"app-background", "app-panel", "app-surface", "app-line", "app-text", "app-muted", "app-accent", "app-blue", "app-warning"};
std::vector<std::pair<juce::String, int>> mappings(juce::Component* c)
{
    if (dynamic_cast<juce::TextButton*>(c)) return {{"button-color", juce::TextButton::buttonColourId}, {"text-color", juce::TextButton::textColourOffId}};
    if (dynamic_cast<juce::ToggleButton*>(c)) return {{"text-color", juce::ToggleButton::textColourId}, {"tick-color", juce::ToggleButton::tickColourId}};
    if (dynamic_cast<juce::ComboBox*>(c)) return {{"control-background", juce::ComboBox::backgroundColourId}, {"text-color", juce::ComboBox::textColourId}, {"control-outline", juce::ComboBox::outlineColourId}, {"arrow-color", juce::ComboBox::arrowColourId}};
    if (dynamic_cast<juce::TextEditor*>(c)) return {{"control-background", juce::TextEditor::backgroundColourId}, {"text-color", juce::TextEditor::textColourId}, {"control-outline", juce::TextEditor::outlineColourId}, {"accent-color", juce::TextEditor::focusedOutlineColourId}};
    if (dynamic_cast<juce::Slider*>(c)) return {{"slider-track", juce::Slider::trackColourId}, {"slider-thumb", juce::Slider::thumbColourId}, {"slider-background", juce::Slider::backgroundColourId}, {"text-color", juce::Slider::textBoxTextColourId}};
    if (dynamic_cast<juce::Label*>(c)) return {{"text-color", juce::Label::textColourId}};
    return {{"surface-color", uiColours::tile}, {"panel-color", uiColours::panel}, {"canvas-color", uiColours::background},
        {"text-color", uiColours::text}, {"muted-color", uiColours::muted}, {"accent-color", uiColours::accent},
        {"line-color", uiColours::line}, {"warning-color", uiColours::warning}};
}
struct ProgramRegistry
{
    struct Colour { juce::String name; int id; juce::Colour original; bool explicitColour; };
    struct Entry
    {
        juce::Component::SafePointer<juce::Component> component, root;
        juce::Rectangle<int> baseline;
        bool movable = true;
        std::vector<Colour> colours;
        std::optional<juce::Font> font;
    };
    std::map<juce::String, Entry> controls;
    juce::Component::SafePointer<juce::Component> previewRoot;
    juce::Image preview;
};
class ProgramControlItem final : public foleys::GuiItem
{
public:
    ProgramControlItem(foleys::MagicGUIBuilder& b, const juce::ValueTree& n) : GuiItem(b, n)
    {
        std::vector<std::pair<juce::String, int>> names;
        for (auto* name : {"text-color", "button-color", "tick-color", "control-background", "control-outline", "arrow-color", "accent-color", "slider-track", "slider-thumb", "slider-background", "surface-color", "panel-color", "canvas-color", "muted-color", "line-color", "warning-color"})
            names.emplace_back(name, 0);
        setColourTranslation(std::move(names));
    }
    juce::Component* getWrappedComponent() override { return nullptr; }
    void update() override {}
    std::vector<foleys::SettableProperty> getSettableProperties() const override
    {
        return {{configNode, "offset-x", foleys::SettableProperty::Number, 0, {}},
            {configNode, "offset-y", foleys::SettableProperty::Number, 0, {}},
            {configNode, "width-delta", foleys::SettableProperty::Number, 0, {}},
            {configNode, "height-delta", foleys::SettableProperty::Number, 0, {}},
            {configNode, "font-size", foleys::SettableProperty::Number, 0, {}},
            {configNode, "font-scale", foleys::SettableProperty::Number, 1.0, {}}};
    }
};
class ProgramThemeItem final : public foleys::GuiItem
{
    class Preview final : public juce::Component
    {
    public:
        ProgramRegistry* registry = nullptr;
        void paint(juce::Graphics& g) override
        { if (registry && registry->preview.isValid()) g.drawImageAt(registry->preview, 0, 0); }
    } preview;
public:
    ProgramThemeItem(foleys::MagicGUIBuilder& b, const juce::ValueTree& n) : GuiItem(b, n)
    {
        std::vector<std::pair<juce::String, int>> names;
        for (const auto& name : themeNames) names.emplace_back(name, 0);
        setColourTranslation(std::move(names));
        preview.registry = getMagicState().getObjectWithType<ProgramRegistry>("programControls");
        addAndMakeVisible(preview);
    }
    juce::Component* getWrappedComponent() override { return &preview; }
    void update() override {}
    std::vector<foleys::SettableProperty> getSettableProperties() const override
    { return {{configNode, "font-scale", foleys::SettableProperty::Number, 1.0, {}}, {configNode, "corner-radius", foleys::SettableProperty::Number, 10.0, {}}}; }
};
class PreviewContent final : public juce::Component
{
public:
    PreviewContent(juce::Component& canvas, std::function<void()> save, std::function<void()> reset, std::function<void()> close)
    {
        viewport.setViewedComponent(&canvas, false);
        viewport.setScrollBarsShown(true, true);
        addAndMakeVisible(viewport);
        for (auto* b : {&saveButton, &resetButton, &closeButton}) addAndMakeVisible(b);
        addAndMakeVisible(status);
        saveButton.onClick = std::move(save); resetButton.onClick = std::move(reset); closeButton.onClick = std::move(close);
        status.setText("Seleziona un controllo o il tema nell'albero.", juce::dontSendNotification);
    }
    void resized() override
    {
        auto area = getLocalBounds(); auto bar = area.removeFromTop(48).reduced(8);
        saveButton.setBounds(bar.removeFromLeft(126)); bar.removeFromLeft(8);
        resetButton.setBounds(bar.removeFromLeft(112)); bar.removeFromLeft(8);
        closeButton.setBounds(bar.removeFromRight(136)); status.setBounds(bar.reduced(8, 0)); viewport.setBounds(area);
    }
    void message(const juce::String& text) { status.setText(text, juce::dontSendNotification); }
private:
    juce::Viewport viewport;
    juce::TextButton saveButton {"Salva aspetto"}, resetButton {"Ripristina"}, closeButton {"Fine modifica"};
    juce::Label status;
};
class PreviewWindow final : public juce::DocumentWindow
{
public:
    PreviewWindow(std::unique_ptr<PreviewContent> content, std::function<void()> close)
        : DocumentWindow("Personalizza DualDeck", juce::Colour(0xff121c29), closeButton), onClose(std::move(close))
    {
        setUsingNativeTitleBar(true); setContentOwned(content.release(), true);
        setResizable(true, false); setResizeLimits(760, 480, 2400, 1600); centreWithSize(1120, 800);
    }
    void closeButtonPressed() override { if (onClose) onClose(); }
    std::function<void()> onClose;
};
bool validNumber(const juce::var& value, double limit)
{
    auto utf8 = value.toString().toUTF8(); char* end = nullptr;
    const double parsed = std::strtod(utf8.getAddress(), &end);
    return end != utf8.getAddress() && *end == 0 && std::isfinite(parsed) && std::abs(parsed) <= limit;
}
double boundedNumber(const juce::var& value, double fallback, double low, double high)
{
    return value.isVoid() || !validNumber(value, 1.0e9) ? fallback : juce::jlimit(low, high, value.toString().getDoubleValue());
}
}

struct ProgramAppearance::Impl final : private juce::ValueTree::Listener, private juce::Timer
{
    explicit Impl(std::function<void(const UiTheme&)> callback) : themeChanged(std::move(callback))
    {
        registry = state.createAndAddObject<ProgramRegistry>("programControls");
        builder.registerFactory("AppControl", [](foleys::MagicGUIBuilder& b, const juce::ValueTree& n) -> std::unique_ptr<foleys::GuiItem> { return std::make_unique<ProgramControlItem>(b, n); });
        builder.registerFactory("AppTheme", [](foleys::MagicGUIBuilder& b, const juce::ValueTree& n) -> std::unique_ptr<foleys::GuiItem> { return std::make_unique<ProgramThemeItem>(b, n); });
    }
    ~Impl() override
    {
        stopTimer(); hide(); tree.removeListener(this);
        // The toolbox belongs to builder; keep its host alive until it is detached.
        window.reset(); canvas.setLookAndFeel(nullptr);
    }
    juce::ValueTree nodeFor(const juce::String& id) const
    {
        std::function<juce::ValueTree(juce::ValueTree)> find = [&](juce::ValueTree n) -> juce::ValueTree
        {
            if (n["id"].toString() == id) return n;
            for (auto child : n) { auto found = find(child); if (found.isValid()) return found; }
            return {};
        };
        return find(tree.getChildWithName("View"));
    }
    juce::ValueTree defaults() const
    {
        auto result = juce::ValueTree::fromXml("<magic><Styles><Style name='DualDeck'><Nodes/><Classes/><Types><View margin='0' padding='0' border='0' background-color='00000000'/><AppControl margin='0' padding='0' border='0' background-color='00000000'/><AppTheme margin='0' padding='0' border='0' background-color='00000000'/></Types></Style></Styles><View display='contents' margin='0' padding='0' border='0' background-color='00000000'/></magic>");
        auto view = result.getChildWithName("View");
        juce::ValueTree themeNode("AppTheme"); themeNode.setProperty("id", "programTheme", nullptr);
        const UiTheme initial;
        for (int i = 0; i < themeNames.size(); ++i) themeNode.setProperty(juce::Identifier(themeNames[i]), initial.colours[static_cast<std::size_t>(i)].toString(), nullptr);
        themeNode.setProperty("font-scale", 1.0, nullptr); themeNode.setProperty("corner-radius", 10.0, nullptr);
        view.addChild(themeNode, -1, nullptr);
        for (const auto& pair : registry->controls)
        {
            juce::ValueTree control("AppControl"); control.setProperty("id", pair.first, nullptr);
            view.addChild(control, -1, nullptr);
        }
        return result;
    }
    juce::Result validate(const juce::ValueTree& layout) const
    {
        if (!layout.hasType("magic") || !layout.getChildWithName("Styles").isValid() || !layout.getChildWithName("View").isValid())
            return juce::Result::fail("Aspetto del programma non valido: caricato quello iniziale.");
        std::set<juce::String> ids, controlIds; int count = 0, themes = 0;
        std::function<bool(juce::ValueTree, int)> visit = [&](juce::ValueTree n, int depth)
        {
            if (++count > 512 || depth > 24) return false;
            if (!n.hasType("View") && !n.hasType("AppControl") && !n.hasType("AppTheme")) return false;
            if (!n.hasType("View") && n.getNumChildren() != 0) return false;
            const auto id = n["id"].toString();
            if (id.isNotEmpty() && !ids.insert(id).second) return false;
            if (n.hasType("AppControl"))
            {
                if (registry->controls.count(id) == 0) return false;
                controlIds.insert(id);
            }
            if (n.hasType("AppTheme") && (id != "programTheme" || ++themes > 1)) return false;
            for (auto* property : {"offset-x", "offset-y", "width-delta", "height-delta", "font-size", "font-scale", "corner-radius"})
                if (n.hasProperty(property) && !validNumber(n[property], 10000.0)) return false;
            for (auto child : n) if (!visit(child, depth + 1)) return false;
            return true;
        };
        if (!visit(layout.getChildWithName("View"), 0) || themes != 1 || controlIds.size() != registry->controls.size())
            return juce::Result::fail("L'aspetto deve conservare il tema e tutti gli ID dei controlli. Usa Ripristina se necessario.");
        return juce::Result::ok();
    }
    void activate(const juce::ValueTree& layout)
    {
        const juce::ScopedValueSetter<bool> guard(syncing, true);
        tree.removeListener(this); tree = layout; state.setGuiValueTree(tree);
        builder.updateStylesheet();
        if (window) builder.createGUI(canvas);
        tree.addListener(this); applyTheme();
        for (auto& pair : registry->controls) { applyBounds(pair.first, pair.second); applyStyle(pair.first, pair.second); }
        syncPreview(); dirty = false;
    }
    juce::var property(const juce::String& id, const char* name)
    {
        auto node = nodeFor(id);
        juce::ValueTree definedHere;
        const auto value = builder.getStylesheet().getStyleProperty(name, node, true, &definedHere);
        return definedHere.isValid() ? value : juce::var();
    }
    void applyTheme()
    {
        UiTheme next;
        for (int i = 0; i < themeNames.size(); ++i)
        {
            const auto value = property("programTheme", themeNames[i].toRawUTF8()).toString();
            if (value.isNotEmpty()) next.colours[static_cast<std::size_t>(i)] = builder.getStylesheet().getColour(value);
        }
        const auto scale = property("programTheme", "font-scale"), radius = property("programTheme", "corner-radius");
        next.fontScale = static_cast<float>(boundedNumber(scale, 1.0, 0.75, 1.5));
        next.cornerRadius = static_cast<float>(boundedNumber(radius, 10.0, 0.0, 24.0));
        theme = next; if (themeChanged) themeChanged(theme);
    }
    juce::Colour themed(juce::Colour original) const
    {
        const UiTheme initial;
        for (std::size_t i = 0; i < initial.colours.size(); ++i) if (original == initial.colours[i]) return theme.colours[i];
        if (original == juce::Colour(0xffe7f1ff)) return theme.colours[4];
        if (original == juce::Colour(0xffa9b9ca)) return theme.colours[5];
        return original;
    }
    void applyStyle(const juce::String& id, ProgramRegistry::Entry& entry)
    {
        auto* c = entry.component.getComponent(); if (!c) return;
        const auto scale = boundedNumber(property(id, "font-scale"), 1.0, 0.6, 2.0);
        const auto sizeOverride = boundedNumber(property(id, "font-size"), 0.0, 0.0, 40.0);
        const bool fontChanged = static_cast<double>(c->getProperties().getWithDefault("appearance-font-scale", 1.0)) != scale
            || static_cast<double>(c->getProperties().getWithDefault("appearance-font-size", 0.0)) != sizeOverride;
        c->getProperties().set("appearance-font-scale", scale);
        c->getProperties().set("appearance-font-size", sizeOverride);
        if (fontChanged) c->sendLookAndFeelChange();
        for (const auto& mapping : entry.colours)
        {
            const auto overrideColour = property(id, mapping.name.toRawUTF8()).toString();
            if (overrideColour.isNotEmpty()) c->setColour(mapping.id, builder.getStylesheet().getColour(overrideColour));
            else if (mapping.name == "text-color" && c->getProperties().contains("appearance-text-role"))
                c->setColour(mapping.id, c->findColour(static_cast<int>(c->getProperties()["appearance-text-role"]), true));
            else if (mapping.explicitColour) c->setColour(mapping.id, themed(mapping.original));
            else c->removeColour(mapping.id);
        }
        const auto fontSize = property(id, "font-size");
        if (entry.font)
        {
            const auto size = static_cast<float>(boundedNumber(fontSize, 0, 0, 40));
            const auto font = entry.font->withHeight(size <= 0 ? entry.font->getHeight() : juce::jmax(8.0f, size));
            if (auto* label = dynamic_cast<juce::Label*>(c)) label->setFont(font);
            if (auto* text = dynamic_cast<juce::TextEditor*>(c)) text->applyFontToAllText(font.withHeight(font.getHeight() * uiFontScale(*c)));
        }
        c->repaint();
    }
    void applyBounds(const juce::String& id, ProgramRegistry::Entry& entry)
    {
        auto* c = entry.component.getComponent(); auto node = nodeFor(id);
        if (!c || !entry.movable || entry.baseline.isEmpty() || !node.isValid()) return;
        const auto base = entry.baseline;
        auto number = [&](const char* name) { return static_cast<int>(boundedNumber(node.getProperty(name), 0, -10000, 10000)); };
        int x = base.getX() + number("offset-x"), y = base.getY() + number("offset-y");
        int w = juce::jmax(24, base.getWidth() + number("width-delta"));
        int h = juce::jmax(12, base.getHeight() + number("height-delta"));
        if (auto* parent = c->getParentComponent())
        {
            w = juce::jmin(w, juce::jmax(24, parent->getWidth())); h = juce::jmin(h, juce::jmax(12, parent->getHeight()));
            x = juce::jlimit(0, juce::jmax(0, parent->getWidth() - w), x);
            y = juce::jlimit(0, juce::jmax(0, parent->getHeight() - h), y);
        }
        c->setBounds(x, y, w, h);
    }
    void syncPreview()
    {
        if (!window || !registry->previewRoot) return;
        const juce::ScopedValueSetter<bool> guard(syncing, true);
        auto* root = registry->previewRoot.getComponent();
        canvas.setSize(root->getWidth(), root->getHeight());
        for (auto& pair : registry->controls)
        {
            auto node = nodeFor(pair.first); auto& entry = pair.second; auto* c = entry.component.getComponent();
            if (!c || !node.isValid()) continue;
            const auto bounds = root->getLocalArea(c, c->getLocalBounds());
            node.setProperty("pos-x", bounds.getX(), nullptr); node.setProperty("pos-y", bounds.getY(), nullptr);
            node.setProperty("pos-width", bounds.getWidth(), nullptr); node.setProperty("pos-height", bounds.getHeight(), nullptr);
        }
        builder.updateLayout(canvas.getLocalBounds());
        applyProxyVisibility(); snapshot();
    }
    void applyProxyVisibility()
    {
        auto* root = registry->previewRoot.getComponent(); if (!root) return;
        // Foleys can make all siblings visible again while processing position or media-size changes.
        for (const auto& pair : registry->controls)
        {
            const auto& entry = pair.second; auto* c = entry.component.getComponent();
            if (!c) continue;
            if (auto* item = builder.findGuiItem(nodeFor(pair.first)))
            {
                bool visible = entry.root == root;
                for (auto* p = c; p && p != root; p = p->getParentComponent()) visible = visible && p->isVisible();
                item->setVisible(visible);
            }
        }
    }
    void snapshot()
    {
        if (auto* root = registry->previewRoot.getComponent()) registry->preview = root->createComponentSnapshot(root->getLocalBounds());
        canvas.repaint();
    }
    void adoptCurrentTree()
    {
        const auto current = state.getGuiTree(); if (current == tree) return;
        const juce::ScopedValueSetter<bool> guard(syncing, true);
        tree.removeListener(this); tree = current; tree.addListener(this); builder.updateStylesheet();
        applyTheme();
        for (auto& pair : registry->controls) { applyBounds(pair.first, pair.second); applyStyle(pair.first, pair.second); }
        syncPreview(); dirty = true;
    }
    void valueTreePropertyChanged(juce::ValueTree& node, const juce::Identifier& name) override
    {
        if (syncing) return;
        const juce::ScopedValueSetter<bool> guard(syncing, true);
        const auto found = registry->controls.find(node["id"].toString());
        if (found != registry->controls.end())
        {
            auto& entry = found->second;
            const auto position = name.toString();
            if (position.startsWith("pos-") && entry.movable && registry->previewRoot && entry.component)
            {
                const auto base = registry->previewRoot->getLocalArea(entry.component->getParentComponent(), entry.baseline);
                if (position == "pos-x") node.setProperty("offset-x", static_cast<int>(node[name]) - base.getX(), nullptr);
                if (position == "pos-y") node.setProperty("offset-y", static_cast<int>(node[name]) - base.getY(), nullptr);
                if (position == "pos-width") node.setProperty("width-delta", static_cast<int>(node[name]) - base.getWidth(), nullptr);
                if (position == "pos-height") node.setProperty("height-delta", static_cast<int>(node[name]) - base.getHeight(), nullptr);
            }
            applyBounds(found->first, entry); applyStyle(found->first, entry);
        }
        else
        {
            applyTheme();
            for (auto& pair : registry->controls) applyStyle(pair.first, pair.second);
        }
        dirty = true;
        if (content) content->message("Modifiche non salvate");
        // Resynchronise only after ordinary offset edits; native drag updates all four positions itself.
        if (!name.toString().startsWith("pos-")) syncPreview(); else { applyProxyVisibility(); snapshot(); }
    }
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override { changedStructure(); }
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override { changedStructure(); }
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override { changedStructure(); }
    void changedStructure()
    {
        if (syncing) return;
        const juce::ScopedValueSetter<bool> guard(syncing, true);
        builder.updateStylesheet();
        applyTheme();
        for (auto& pair : registry->controls) applyStyle(pair.first, pair.second);
        syncPreview(); dirty = true;
        if (content) content->message("Modifiche non salvate");
    }
    void timerCallback() override { adoptCurrentTree(); snapshot(); }
    void show(juce::Component& root)
    {
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
        registry->previewRoot = &root;
        if (!window)
        {
            auto controls = std::make_unique<PreviewContent>(canvas,
                [this] { report(save()); }, [this] { report(reset()); }, [this] { hide(); });
            content = controls.get();
            window = std::make_unique<PreviewWindow>(std::move(controls), [this] { hide(); });
            window->setLookAndFeel(&root.getLookAndFeel()); canvas.setLookAndFeel(&root.getLookAndFeel());
            builder.createGUI(canvas);
        }
        window->setVisible(true); window->toFront(true); syncPreview();
        if (!toolboxRequested)
        {
            toolboxRequested = true; builder.attachToolboxToWindow(canvas);
            juce::Component::SafePointer<juce::Component> safe(&canvas);
            juce::MessageManager::callAsync([this, safe] { if (safe) { toolboxReady = true; updateToolbox(); } });
        }
        else if (toolboxReady) updateToolbox();
        startTimer(350);
#else
        juce::ignoreUnused(root);
#endif
    }
    void updateToolbox()
    {
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
        if (!window || !toolboxReady) return;
        const bool visible = window->isVisible(); auto& toolbox = builder.getMagicToolBox();
        window->removeKeyListener(&toolbox); if (visible) window->addKeyListener(&toolbox);
        toolbox.setVisible(visible);
        for (int i = 0; i < toolbox.getNumChildComponents(); ++i)
            if (auto* b = dynamic_cast<juce::TextButton*>(toolbox.getChildComponent(i)))
            {
                if (b->getClickingTogglesState()) b->setToggleState(visible, juce::sendNotificationSync);
                if (b->getButtonText().startsWith("File")) b->onClick = [this]
                {
                    // Save/recovery use the same validated model as the application's menus.
                    juce::PopupMenu menu;
                    juce::Component::SafePointer<juce::Component> safe(&canvas);
                    menu.addItem("Salva aspetto", [this, safe] { if (safe) report(save()); });
                    menu.addItem("Ripristina aspetto", [this, safe] { if (safe) report(reset()); });
                    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(window.get()));
                };
            }
        builder.setEditMode(visible); if (visible) toolbox.toFront(true);
#endif
    }
    void hide() { stopTimer(); if (window) { window->setVisible(false); updateToolbox(); } }
    void report(const juce::Result& result) { if (content) content->message(result.wasOk() ? "Aspetto salvato" : result.getErrorMessage()); }
    juce::Result save()
    {
        adoptCurrentTree();
        const auto valid = validate(tree); if (valid.failed()) return valid;
        auto xml = tree.createXml();
        if (!xml || xml->toString().getNumBytesAsUTF8() > 1024 * 1024) return juce::Result::fail("Aspetto troppo grande per essere salvato.");
        if (file == juce::File() || file.getParentDirectory().createDirectory().failed()) return juce::Result::fail("Impossibile creare il file dell'aspetto.");
        juce::TemporaryFile temporary(file);
        if (!xml->writeTo(temporary.getFile()) || !temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Impossibile salvare l'aspetto. Controlla che la cartella sia scrivibile.");
        dirty = false; return juce::Result::ok();
    }
    juce::Result reset()
    {
        auto previous = tree.createCopy(); const bool wasDirty = dirty;
        activate(defaults()); const auto result = save();
        if (result.failed()) { activate(previous); dirty = wasDirty; }
        return result;
    }
    std::function<void(const UiTheme&)> themeChanged;
    foleys::MagicGUIState state;
    juce::Component canvas;
    foleys::MagicGUIBuilder builder {state};
    std::unique_ptr<PreviewWindow> window;
    PreviewContent* content = nullptr;
    ProgramRegistry* registry = nullptr;
    juce::ValueTree tree;
    juce::File file;
    UiTheme theme;
    juce::StringArray retiredControls;
    bool syncing = false, dirty = false, toolboxRequested = false, toolboxReady = false;
};

ProgramAppearance::ProgramAppearance(std::function<void(const UiTheme&)> callback) : impl(std::make_unique<Impl>(std::move(callback))) {}
ProgramAppearance::~ProgramAppearance() = default;
void ProgramAppearance::registerControl(const juce::String& id, juce::Component& c, juce::Component& root, bool movable)
{
    jassert(impl->registry->controls.count(id) == 0);
    auto& entry = impl->registry->controls[id]; entry.component = &c; entry.root = &root; entry.movable = movable;
    if (c.getComponentID().isEmpty()) c.setComponentID(id);
    for (const auto& mapping : mappings(&c)) entry.colours.push_back({mapping.first, mapping.second, c.findColour(mapping.second, true), c.isColourSpecified(mapping.second)});
    if (auto* label = dynamic_cast<juce::Label*>(&c)) entry.font = label->getFont();
    if (auto* text = dynamic_cast<juce::TextEditor*>(&c)) entry.font = text->getFont();
}
void ProgramAppearance::applyLayout(juce::Component& root)
{
    for (auto& pair : impl->registry->controls)
        if (pair.second.root == &root && pair.second.component)
        {
            pair.second.baseline = pair.second.component->getBounds();
            impl->applyBounds(pair.first, pair.second);
        }
    if (isDesignerVisible()) impl->syncPreview();
}
void ProgramAppearance::refreshStyle(const juce::String& id)
{
    const auto found = impl->registry->controls.find(id);
    if (found != impl->registry->controls.end()) impl->applyStyle(found->first, found->second);
}
juce::Result ProgramAppearance::loadAppearance(const juce::File& file)
{
    impl->file = file;
    auto layout = file.existsAsFile() && file.getSize() <= 1024 * 1024 ? juce::ValueTree::fromXml(file.loadFileAsString()) : juce::ValueTree();
    if (layout.isValid() && !impl->retiredControls.isEmpty())
    {
        std::function<void(juce::ValueTree)> archive = [&](juce::ValueTree node)
        {
            for (int i = node.getNumChildren(); --i >= 0;)
            {
                auto child = node.getChild(i);
                if (child.hasType("AppControl") && impl->retiredControls.contains(child["id"].toString()))
                {
                    auto backup = layout.getOrCreateChildWithName("LegacyMainControls", nullptr);
                    node.removeChild(child, nullptr); backup.addChild(child, -1, nullptr);
                }
                else archive(child);
            }
        };
        archive(layout.getChildWithName("View"));
    }
    const auto result = file == juce::File() || !file.exists() ? juce::Result::ok() : impl->validate(layout);
    impl->activate(layout.isValid() && result.wasOk() ? layout : impl->defaults()); return result;
}
juce::Result ProgramAppearance::saveAppearance() { return impl->save(); }
void ProgramAppearance::retireControls(const juce::StringArray& ids) { impl->retiredControls = ids; }
juce::Result ProgramAppearance::resetAppearance() { return impl->reset(); }
juce::ValueTree ProgramAppearance::findNode(const juce::String& id) const { return impl->nodeFor(id); }
void ProgramAppearance::showDesigner(juce::Component& root) { impl->show(root); }
void ProgramAppearance::hideDesigner() { impl->hide(); }
bool ProgramAppearance::isDesignerVisible() const { return impl->window && impl->window->isVisible(); }
juce::Result runProgramAppearancePreviewTests()
{
    juce::Component board, editor;
    board.setSize(640, 480); editor.setSize(640, 700);
    juce::TextButton visible("Visible"), hidden("Hidden"), other("Other screen");
    board.addAndMakeVisible(visible); board.addChildComponent(hidden); editor.addAndMakeVisible(other);
    visible.setBounds(40, 80, 150, 30); hidden.setBounds(50, 90, 150, 30); other.setBounds(60, 100, 150, 30);
    ProgramAppearance model([](const UiTheme&) {});
    model.registerControl("a.visible", visible, board); model.registerControl("b.hidden", hidden, board); model.registerControl("z.other", other, editor);
    model.applyLayout(board); model.applyLayout(editor); model.loadAppearance({});
    auto& impl = *model.impl; impl.registry->previewRoot = &board;
    auto controls = std::make_unique<PreviewContent>(impl.canvas, [] {}, [] {}, [] {});
    impl.window = std::make_unique<PreviewWindow>(std::move(controls), [] {});
    impl.builder.createGUI(impl.canvas); impl.syncPreview();
    auto validVisibility = [&]
    {
        auto* a = impl.builder.findGuiItem(model.findNode("a.visible"));
        auto* b = impl.builder.findGuiItem(model.findNode("b.hidden"));
        auto* c = impl.builder.findGuiItem(model.findNode("z.other"));
        return a && b && c && a->isVisible() && !b->isVisible() && !c->isVisible();
    };
    if (!validVisibility()) return juce::Result::fail("Preview exposes hidden or different-screen proxies");
    model.findNode("a.visible").setProperty("pos-y", 97, nullptr);
    if (visible.getY() != 97 || !validVisibility()) return juce::Result::fail("Native Foleys drag resurrected hidden proxies or lost its offset");
    visible.setBounds(40, 110, 180, 30); model.applyLayout(board);
    if (visible.getY() != 127 || !validVisibility()) return juce::Result::fail("Preview offsets failed after responsive reflow");
    auto* themeItem = impl.builder.findGuiItem(model.findNode("programTheme"));
    if (!themeItem || themeItem->getWrappedComponent()->getBounds() != themeItem->getLocalBounds())
        return juce::Result::fail("Preview image and editable proxies have different origins");
    return juce::Result::ok();
}
}
