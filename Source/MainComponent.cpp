#include "MainComponent.h"
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
#include "AudioSettingsPanel.h"
#include "ProgramAppearance.h"
#include "MainScreenPanel.h"
#endif
#include <limits>

namespace dualdeck
{
namespace
{
constexpr std::uint64_t editorPlayId = std::numeric_limits<std::uint64_t>::max() - 3;
constexpr std::uint64_t scrubPreviewId = std::numeric_limits<std::uint64_t>::max() - 2;
constexpr std::uint64_t loopPreviewId = std::numeric_limits<std::uint64_t>::max();
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }
class LoadJob final : public juce::ThreadPoolJob
{
public:
    explicit LoadJob(std::function<void()> function) : ThreadPoolJob("Decode clip"), work(std::move(function)) {}
    JobStatus runJob() override { work(); return jobHasFinished; }
private:
    std::function<void()> work;
};
class FolderJob final : public juce::ThreadPoolJob
{
public:
    explicit FolderJob(std::function<void(juce::ThreadPoolJob&)> function) : ThreadPoolJob("Scan folders"), work(std::move(function)) {}
    JobStatus runJob() override { work(*this); return jobHasFinished; }
private:
    std::function<void(juce::ThreadPoolJob&)> work;
};
juce::String keyLabel(const juce::String& code)
{
    for (const auto& row : keyboardLayout()) for (const auto& key : row) if (key.code == code) return key.label;
    return code;
}
}
DeckLookAndFeel::DeckLookAndFeel() { applyTheme(UiTheme{}); }
void DeckLookAndFeel::applyTheme(const UiTheme& value)
{
    theme = value;
    for (std::size_t i = 0; i < theme.colours.size(); ++i) setColour(uiColours::background + static_cast<int>(i), theme.colours[i]);
    setColour(juce::ResizableWindow::backgroundColourId, theme.colours[0]);
    setColour(juce::TextButton::buttonColourId, theme.colours[2]);
    setColour(juce::TextButton::buttonOnColourId, theme.colours[6].withAlpha(0.18f));
    setColour(juce::TextButton::textColourOffId, theme.colours[4]);
    setColour(juce::TextButton::textColourOnId, theme.colours[6]);
    setColour(juce::Label::textColourId, theme.colours[5]);
    setColour(juce::ComboBox::backgroundColourId, theme.colours[2]);
    setColour(juce::ComboBox::outlineColourId, theme.colours[3]);
    setColour(juce::ComboBox::textColourId, theme.colours[4]);
    setColour(juce::PopupMenu::backgroundColourId, theme.colours[1]);
    setColour(juce::PopupMenu::textColourId, theme.colours[4]);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme.colours[3]);
    setColour(juce::TextEditor::backgroundColourId, theme.colours[2]);
    setColour(juce::TextEditor::textColourId, theme.colours[4]);
    setColour(juce::TextEditor::outlineColourId, theme.colours[3]);
    setColour(juce::TextEditor::focusedOutlineColourId, theme.colours[6]);
    setColour(juce::Slider::thumbColourId, theme.colours[6]);
    setColour(juce::Slider::trackColourId, theme.colours[6]);
    setColour(juce::Slider::backgroundColourId, theme.colours[3]);
    setColour(juce::Slider::textBoxTextColourId, theme.colours[4]);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, theme.colours[4]);
    setColour(juce::ToggleButton::tickColourId, theme.colours[6]);
    setColour(juce::PopupMenu::highlightedTextColourId, theme.colours[6]);
}
juce::Font DeckLookAndFeel::getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&)
{
    return juce::Font(juce::FontOptions(juce::jlimit(12.0f, 16.0f, 13.0f * theme.fontScale)));
}
int DeckLookAndFeel::getMenuBarItemWidth(juce::MenuBarComponent& bar, int index, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidthInt(getMenuBarFont(bar, index, text), text) + 18;
}
void DeckLookAndFeel::drawMenuBarBackground(juce::Graphics& g, int width, int height, bool, juce::MenuBarComponent& bar)
{
    g.fillAll(uiColour(bar, uiColours::panel));
    g.setColour(uiColour(bar, uiColours::line).withAlpha(0.65f));
    g.fillRect(0, height - 1, width, 1);
}
void DeckLookAndFeel::drawMenuBarItem(juce::Graphics& g, int width, int height, int index, const juce::String& text,
    bool over, bool open, bool, juce::MenuBarComponent& bar)
{
    if (bar.isEnabled() && (over || open))
    {
        g.setColour(uiColour(bar, uiColours::accent).withAlpha(open ? 0.13f : 0.07f));
        g.fillRoundedRectangle(juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(2.0f, 3.0f), 4.0f);
    }
    auto colour = open ? uiColour(bar, uiColours::accent) : over ? uiColour(bar, uiColours::text) : uiColour(bar, uiColours::muted).brighter(0.2f);
    g.setColour(colour.withMultipliedAlpha(bar.isEnabled() ? 1.0f : 0.5f));
    g.setFont(getMenuBarFont(bar, index, text));
    g.drawFittedText(text, 0, 0, width, height - 1, juce::Justification::centred, 1);
}
void DeckLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& colour, bool over, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colour.brighter(down ? 0.15f : over ? 0.08f : 0.0f));
    g.fillRoundedRectangle(bounds, uiRadius(button, 7.0f));
    g.setColour(button.getToggleState() ? uiColour(button, uiColours::accent).withAlpha(0.45f) : uiColour(button, uiColours::line));
    g.drawRoundedRectangle(bounds, uiRadius(button, 7.0f), 1.0f);
}
void DeckLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    if (static_cast<bool>(button.getProperties().getWithDefault("inspector-toggle-icon", false)))
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto accent = uiColour(button, uiColours::accent);
        auto background = button.getToggleState() ? accent.withAlpha(0.18f) : uiColour(button, uiColours::panel);
        if (over) background = background.brighter(0.06f);
        if (down) background = background.brighter(0.08f);
        g.setColour(background);
        g.fillRoundedRectangle(bounds, uiRadius(button, 7.0f));
        g.setColour(button.getToggleState() ? accent : uiColour(button, uiColours::line));
        g.drawRoundedRectangle(bounds, uiRadius(button, 7.0f), 1.0f);

        if (!button.isEnabled()) g.setOpacity(0.5f);
        const auto icon = juce::Rectangle<float>(10.0f, (static_cast<float>(button.getHeight()) - 18.0f) * 0.5f, 18.0f, 18.0f);
        const auto iconColour = button.getToggleState() ? accent : button.findColour(juce::ToggleButton::textColourId);
        g.setColour(iconColour);
        g.drawRoundedRectangle(icon, 2.5f, 1.5f);
        const auto dividerX = icon.getRight() - 6.0f;
        g.drawLine(dividerX, icon.getY() + 1.5f, dividerX, icon.getBottom() - 1.5f, 1.4f);
        g.drawLine(icon.getX() + 3.0f, icon.getY() + 5.0f, dividerX - 2.0f, icon.getY() + 5.0f, 1.2f);
        g.drawLine(icon.getX() + 3.0f, icon.getY() + 9.0f, dividerX - 2.0f, icon.getY() + 9.0f, 1.2f);
        g.drawLine(dividerX + 2.0f, icon.getY() + 5.0f, icon.getRight() - 2.0f, icon.getY() + 5.0f, 1.2f);
        g.drawLine(dividerX + 2.0f, icon.getY() + 9.0f, icon.getRight() - 2.0f, icon.getY() + 9.0f, 1.2f);

        const auto size = uiFontHeight(button, juce::jmin(15.0f, static_cast<float>(button.getHeight()) * 0.75f)) * uiFontScale(button);
        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(juce::Font(juce::FontOptions(size)));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(36).withTrimmedRight(6), juce::Justification::centredLeft, 1);
        return;
    }

    const auto size = uiFontHeight(button, juce::jmin(15.0f, static_cast<float>(button.getHeight()) * 0.75f)) * uiFontScale(button);
    const auto tick = size * 1.1f;
    drawTickBox(g, button, 4.0f, (static_cast<float>(button.getHeight()) - tick) * 0.5f, tick, tick,
        button.getToggleState(), button.isEnabled(), over, down);
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(juce::Font(juce::FontOptions(size)));
    if (!button.isEnabled()) g.setOpacity(0.5f);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(juce::roundToInt(tick) + 10).withTrimmedRight(2), juce::Justification::centredLeft, 10);
}
void PadTile::paint(juce::Graphics& g)
{
    auto box = getLocalBounds().toFloat().reduced(1);
    g.setColour(held ? uiColour(*this, uiColours::accent).withAlpha(0.22f) : isMouseOver() ? uiColour(*this, uiColours::tile).brighter(0.06f) : uiColour(*this, uiColours::tile));
    g.fillRoundedRectangle(box, uiRadius(*this, 10));
    g.setColour(selected ? uiColour(*this, uiColours::accent) : uiColour(*this, uiColours::line));
    g.drawRoundedRectangle(box, uiRadius(*this, 10), selected ? 1.7f : 1.0f);
    g.setColour(missing ? uiColour(*this, uiColours::warning) : uiColour(*this, uiColours::accent));
    g.fillRoundedRectangle(juce::Rectangle<float>(12, 13, 3, 19), 1.5f);
    g.setFont(uiFont(*this, 12, true));
    g.drawText("#" + juce::String(displayNumber) + (binding.isEmpty() ? "" : "  /  " + binding), 23, 12, getWidth() - 42, 22, juce::Justification::centredLeft);
    if (favorite) { g.setColour(uiColour(*this, uiColours::warning)); g.fillEllipse(static_cast<float>(getWidth() - 20), 20, 5, 5); }
    g.setFont(uiFont(*this, 14, true)); g.setColour(uiColour(*this, uiColours::text));
    g.drawFittedText(title, 13, 43, getWidth() - 26, 38, juce::Justification::topLeft, 2);
    g.setFont(uiFont(*this, 11)); g.setColour(missing ? uiColour(*this, uiColours::warning) : uiColour(*this, uiColours::muted));
    g.drawText(subtitle, 13, getHeight() - 25, getWidth() - 26, 17, juce::Justification::centredLeft, true);
}
void PadTile::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    if (select) select(index);
    if (event.mods.isRightButtonDown()) { if (contextMenu) contextMenu(index); }
    else if (event.mods.isLeftButtonDown()) { held = true; if (press) press(index, true); }
    repaint();
}
void PadTile::mouseUp(const juce::MouseEvent&)
{
    if (held && press) press(index, false);
    held = false; repaint();
}
bool PadTile::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() != juce::KeyPress::returnKey) return false;
    if (!keyboardHeld)
    {
        keyboardHeld = true;
        if (select) select(index);
        if (press) press(index, true);
        repaint();
    }
    return true;
}
bool PadTile::keyStateChanged(bool)
{
    if (!keyboardHeld || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::returnKey)) return false;
    keyboardHeld = false;
    if (press) press(index, false);
    repaint();
    return true;
}
void PadTile::focusLost(FocusChangeType)
{
    if (keyboardHeld && press) press(index, false);
    keyboardHeld = false;
    repaint();
}
std::unique_ptr<juce::AccessibilityHandler> PadTile::createAccessibilityHandler()
{
    juce::AccessibilityActions actions;
    actions.addAction(juce::AccessibilityActionType::press, [this]
    {
        if (select) select(index);
        if (press) { press(index, true); press(index, false); }
    });
    actions.addAction(juce::AccessibilityActionType::showMenu, [this]
    {
        if (contextMenu) contextMenu(index);
    });
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::button, std::move(actions));
}
const std::vector<std::vector<KeyboardKey>>& keyboardLayout()
{
    static const std::vector<std::vector<KeyboardKey>> rows {
        {{"Esc","0001:0"},{"","",1},{"F1","003B:0"},{"F2","003C:0"},{"F3","003D:0"},{"F4","003E:0"},{"","",0.5f},{"F5","003F:0"},{"F6","0040:0"},{"F7","0041:0"},{"F8","0042:0"},{"","",0.5f},{"F9","0043:0"},{"F10","0044:0"},{"F11","0057:0"},{"F12","0058:0"},{"","",0.5f},{"Stamp","0037:2"},{"Scroll","0046:0"},{"Pausa","0045:4"},{"","",4.5f}},
        {{"\\","0029:0"},{"1","0002:0"},{"2","0003:0"},{"3","0004:0"},{"4","0005:0"},{"5","0006:0"},{"6","0007:0"},{"7","0008:0"},{"8","0009:0"},{"9","000A:0"},{"0","000B:0"},{"'","000C:0"},{utf8("ì"),"000D:0"},{"Back","000E:0",2},{"","",0.5f},{"Ins","0052:2"},{"Home","0047:2"},{"PgSu","0049:2"},{"","",0.5f},{"Num","0045:0"},{"Num /","0035:2"},{"Num *","0037:0"},{"Num -","004A:0"}},
        {{"Tab","000F:0",1.5f},{"Q","0010:0"},{"W","0011:0"},{"E","0012:0"},{"R","0013:0"},{"T","0014:0"},{"Y","0015:0"},{"U","0016:0"},{"I","0017:0"},{"O","0018:0"},{"P","0019:0"},{utf8("è"),"001A:0"},{"+","001B:0"},{"Invio","001C:0",1.5f,2},{"","",0.5f},{"Canc","0053:2"},{"Fine","004F:2"},{"PgGiu","0051:2"},{"","",0.5f},{"Num 7","0047:0"},{"Num 8","0048:0"},{"Num 9","0049:0"},{"Num +","004E:0",1,2}},
        {{"Caps","003A:0",1.5f},{"A","001E:0"},{"S","001F:0"},{"D","0020:0"},{"F","0021:0"},{"G","0022:0"},{"H","0023:0"},{"J","0024:0"},{"K","0025:0"},{"L","0026:0"},{utf8("ò"),"0027:0"},{utf8("à"),"0028:0"},{utf8("ù"),"002B:0"},{"","",5.5f},{"Num 4","004B:0"},{"Num 5","004C:0"},{"Num 6","004D:0"},{"","",1}},
        {{"Shift S","002A:0",1.25f},{"<","0056:0"},{"Z","002C:0"},{"X","002D:0"},{"C","002E:0"},{"V","002F:0"},{"B","0030:0"},{"N","0031:0"},{"M","0032:0"},{",","0033:0"},{".","0034:0"},{"-","0035:0"},{"Shift D","0036:0",2.75f},{"","",1.5f},{"Su","0048:2"},{"","",1.5f},{"Num 1","004F:0"},{"Num 2","0050:0"},{"Num 3","0051:0"},{"Num Inv","001C:2",1,2}},
        {{"Ctrl S","001D:0",1.25f},{"Win S","005B:2",1.25f},{"Alt","0038:0",1.25f},{"Spazio","0039:0",6.25f},{"AltGr","0038:2",1.25f},{"Win D","005C:2",1.25f},{"Menu","005D:2",1.25f},{"Ctrl D","001D:2",1.25f},{"","",0.5f},{"Sin","004B:2"},{"Giu","0050:2"},{"Des","004D:2"},{"","",0.5f},{"Num 0","0052:0",2},{"Num .","0053:0"},{"","",1}}
    };
    return rows;
}
juce::String KeyboardView::focusedCode() const
{
    const auto& rows = keyboardLayout();
    return rows[static_cast<std::size_t>(focusedRow)][static_cast<std::size_t>(focusedColumn)].code;
}
int KeyboardView::padNumber(int index) const
{
    return index >= 0 && index < static_cast<int>(padNumbers.size()) ? padNumbers[static_cast<std::size_t>(index)] : 0;
}
void KeyboardView::updateAccessibleKey()
{
    const auto& key = keyboardLayout()[static_cast<std::size_t>(focusedRow)][static_cast<std::size_t>(focusedColumn)];
    juce::String description = "Frecce per scegliere un tasto, Invio per usarlo, Maiusc+F10 per associarlo.";
    if (profile)
        for (std::size_t i = 0; i < profile->pads.size(); ++i)
            if (profile->pads[i].key == key.code)
            {
                description = "Associato al pad " + juce::String(padNumber(static_cast<int>(i))) + ": " + profile->pads[i].name + ". " + description;
                break;
            }
    const auto title = "Tastiera grafica: " + key.label;
    const bool changed = getTitle() != title || getDescription() != description;
    setTitle(title);
    setDescription(description);
    if (changed)
        if (auto* handler = getAccessibilityHandler()) handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
        for (const auto& hitbox : hitboxes) if (hitbox.second == key.code)
        {
            const auto visible = viewport->getViewArea();
            const int x = hitbox.first.getX() < visible.getX() ? static_cast<int>(hitbox.first.getX())
                : hitbox.first.getRight() > visible.getRight() ? static_cast<int>(std::ceil(hitbox.first.getRight())) - visible.getWidth() : visible.getX();
            viewport->setViewPosition(x, visible.getY());
        }
    repaint();
}
bool KeyboardView::keyPressed(const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto& rows = keyboardLayout();
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey
        || code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
        {
            const int direction = code == juce::KeyPress::leftKey ? -1 : 1;
            const auto& row = rows[static_cast<std::size_t>(focusedRow)];
            for (int next = focusedColumn + direction; next >= 0 && next < static_cast<int>(row.size()); next += direction)
                if (row[static_cast<std::size_t>(next)].code.isNotEmpty()) { focusedColumn = next; break; }
        }
        else
        {
            float target = 0;
            const auto& oldRow = rows[static_cast<std::size_t>(focusedRow)];
            for (int i = 0; i < focusedColumn; ++i) target += oldRow[static_cast<std::size_t>(i)].width;
            target += oldRow[static_cast<std::size_t>(focusedColumn)].width * 0.5f;
            focusedRow = juce::jlimit(0, static_cast<int>(rows.size()) - 1, focusedRow + (code == juce::KeyPress::upKey ? -1 : 1));
            float x = 0, distance = std::numeric_limits<float>::max();
            const auto& row = rows[static_cast<std::size_t>(focusedRow)];
            for (std::size_t i = 0; i < row.size(); ++i)
            {
                const float next = std::abs(x + row[i].width * 0.5f - target);
                if (row[i].code.isNotEmpty() && next < distance) { focusedColumn = static_cast<int>(i); distance = next; }
                x += row[i].width;
            }
        }
        updateAccessibleKey();
        return true;
    }
    if (code == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())
    {
        if (assign) assign(focusedCode());
        return true;
    }
    if (code != juce::KeyPress::returnKey) return false;
    if (!keyboardHeld)
    {
        keyboardHeld = true;
        keyboardHeldCode = focusedCode();
        if (press) press(keyboardHeldCode, true);
        repaint();
    }
    return true;
}
bool KeyboardView::keyStateChanged(bool)
{
    if (!keyboardHeld || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::returnKey)) return false;
    keyboardHeld = false;
    if (press) press(keyboardHeldCode, false);
    keyboardHeldCode.clear();
    repaint();
    return true;
}
void KeyboardView::focusLost(FocusChangeType)
{
    if (keyboardHeld && press) press(keyboardHeldCode, false);
    keyboardHeld = false;
    keyboardHeldCode.clear();
    if (held.isNotEmpty() && press) press(held, false);
    held.clear(); repaint();
}
std::unique_ptr<juce::AccessibilityHandler> KeyboardView::createAccessibilityHandler()
{
    juce::AccessibilityActions actions;
    actions.addAction(juce::AccessibilityActionType::press, [this]
    {
        if (press) { press(focusedCode(), true); press(focusedCode(), false); }
    });
    actions.addAction(juce::AccessibilityActionType::showMenu, [this]
    {
        if (assign) assign(focusedCode());
    });
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::group, std::move(actions));
}
void KeyboardView::paint(juce::Graphics& g)
{
    hitboxes.clear();
    const auto& rows = keyboardLayout();
    const float rowHeight = static_cast<float>(getHeight()) / static_cast<float>(rows.size());
    float total = 0;
    for (const auto& row : rows) { float width = 0; for (const auto& key : row) width += key.width; total = std::max(total, width); }
    const float unit = static_cast<float>(getWidth()) / total;
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        float x = 0;
        for (const auto& key : rows[row])
        {
            if (key.code.isEmpty()) { x += unit * key.width; continue; }
            auto box = juce::Rectangle<float>(x, row * rowHeight, unit * key.width, rowHeight * key.height).reduced(2);
            const Pad* pad = nullptr;
            if (profile) for (const auto& p : profile->pads) if (p.key == key.code) { pad = &p; break; }
            g.setColour(held == key.code ? uiColour(*this, uiColours::accent).withAlpha(0.3f) : pad ? uiColour(*this, uiColours::tile) : uiColour(*this, uiColours::panel));
            g.fillRoundedRectangle(box, uiRadius(*this, 6));
            g.setColour(hasKeyboardFocus(false) && key.code == focusedCode() ? uiColour(*this, uiColours::blue)
                : key.code == selectedKey ? uiColour(*this, uiColours::accent) : uiColour(*this, uiColours::line));
            g.drawRoundedRectangle(box, uiRadius(*this, 6), 1);
            g.setFont(uiFont(*this, 12, true)); g.setColour(pad ? uiColour(*this, uiColours::accent) : uiColour(*this, uiColours::muted));
            g.drawFittedText(key.label.startsWith("Num ") ? key.label.substring(4) : key.label,
                box.reduced(4).removeFromTop(20).toNearestInt(), juce::Justification::centredLeft, 1);
            if (pad)
            {
                g.setFont(uiFont(*this, 10)); g.setColour(uiColour(*this, uiColours::text));
                g.drawFittedText("#" + juce::String(padNumber(static_cast<int>(pad - profile->pads.data()))) + " " + pad->name, box.reduced(6).withTrimmedTop(25).toNearestInt(), juce::Justification::topLeft, 2);
            }
            hitboxes.push_back({box, key.code}); x += unit * key.width;
        }
    }
}
void KeyboardView::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    for (const auto& item : hitboxes) if (item.first.contains(e.position))
    {
        const auto& rows = keyboardLayout();
        for (std::size_t row = 0; row < rows.size(); ++row)
            for (std::size_t column = 0; column < rows[row].size(); ++column)
                if (rows[row][column].code == item.second)
                { focusedRow = static_cast<int>(row); focusedColumn = static_cast<int>(column); }
        updateAccessibleKey();
        if (e.mods.isRightButtonDown()) { if (assign) assign(item.second); }
        else { held = item.second; if (press) press(held, true); }
        repaint(); break;
    }
}
void KeyboardView::mouseUp(const juce::MouseEvent&)
{
    if (held.isNotEmpty() && press) press(held, false);
    held.clear(); repaint();
}
void MainComponent::label(juce::Label& item, const juce::String& text, float size)
{
    addAndMakeVisible(item); item.setText(text, juce::dontSendNotification); item.setFont(font(size));
}
MainComponent::MainComponent(bool smoke, bool uiEditor) : smokeTest(smoke || uiEditor)
{
#if !defined(DUALDECK_UI_EDITOR)
    juce::ignoreUnused(uiEditor);
#endif
    setLookAndFeel(&look); setWantsKeyboardFocus(true); addKeyListener(this);
    projectRoot = findProjectRoot(); libraryFile = projectRoot.getChildFile("data/library.json");
    juce::Result loaded = juce::Result::ok();
    if (libraryFile.existsAsFile()) { loaded = library.load(libraryFile); if (loaded.failed()) writable = false; }
    else
    {
        library.ensureProfile();
        if (!smokeTest && loaded.wasOk())
        {
            loaded = library.save(libraryFile);
            if (loaded.failed()) dirty = true;
        }
    }
    library.ensureProfile();
    if (!smokeTest && loaded.wasOk() && library.repairedDuplicateFiles)
    {
        const auto repairSave = library.save(libraryFile);
        if (repairSave.failed())
        {
            dirty = true;
            juce::Logger::writeToLog("Rimozione dei pad duplicati dalla libreria non salvata: " + repairSave.getErrorMessage());
        }
    }
    label(title, "Soundboard", 27); title.setColour(juce::Label::textColourId, palette::text);
    title.setFont(font(27, true));
    label(profileLabel, "Profilo", 13); label(modeLabel, "Riproduzione", 13);
    label(pageLabel, ""); label(statusLabel, "Pronto");
    label(padHeading, "Dettagli suono", 14); label(padFile, "Seleziona un suono");
    label(keyInfo, "Nessun tasto assegnato", 12);
    label(volumeLabel, "Volume del suono"); label(speedLabel, utf8("Velocità"));
    label(fadeLabel, "Dissolvenza  /  entrata e uscita (s)");
    label(padModeLabel, utf8("Modalità del suono"));
    label(outputLabel, "Uscita principale", 15); label(monitorLabel, "Cuffie", 15);
    label(microphoneLabel, "Microfono", 15);
    label(microphoneGainLabel, "Livello del microfono", 13);
    microphoneToggle.setTooltip("Unisce la voce ai suoni nell'uscita principale.");
    microphoneMonitorToggle.setTooltip("Usa le cuffie per evitare ritorni audio dal microfono.");
    label(outputMasterLabel, "Volume uscita", 13); label(monitorMasterLabel, "Volume cuffie", 13);
    label(inputStatus, "", 11); inputStatus.setJustificationType(juce::Justification::topLeft);
    label(folderSummary, "", 11); folderSummary.setJustificationType(juce::Justification::topLeft);
    label(padFolderHeading, "", 12);
    label(keyboardHeading, "Dispositivo", 15);
    label(keyboardStatus, "Scegli una tastiera oppure usa F12.");
    label(lastKey, "");
    label(keyboardHelp, utf8("Quando è attiva, questa tastiera controlla DualDeck e non scrive nelle altre app."), 13);
    keyboardHelp.setJustificationType(juce::Justification::topLeft);
    identifyKeyboard.setTooltip("Premi e rilascia F12 sulla tastiera da dedicare ai suoni.");
    captureKeyboard.setTooltip("Rilascia tutti i tasti prima di attivare. Disattiva per tornare a usare la tastiera normalmente.");
    for (auto* heading : {&profileLabel, &modeLabel, &padHeading, &outputLabel, &monitorLabel, &microphoneLabel, &keyboardHeading})
    {
        heading->setFont(font(heading->getFont().getHeight(), true));
        heading->setColour(juce::Label::textColourId, palette::text);
    }
    for (auto* primary : {&importButton, &applyButton})
    {
        primary->setColour(juce::TextButton::buttonColourId, palette::accent);
        primary->setColour(juce::TextButton::textColourOffId, palette::background);
    }
    for (juce::Component* component : std::initializer_list<juce::Component*>{&profiles,&globalMode,&outputDevice,&monitorDevice,&inputDevice,&padMode,&search,&padName,
        &boardTab,&audioTab,&importButton,&newProfile,&renameProfile,&stopButton,&djButton,&previous,&next,&tilesButton,&keyboardButton,
        &bindButton,&unbindButton,&applyButton,&scanButton,&favoriteButton,&favoritesOnly,&monitorToggle,&keyboardViewport,
        &keyboardTab,&keyboardDevice,&identifyKeyboard,&scanKeyboards,&captureKeyboard,&replaceFile,&editorButton,
        &deleteProfile,&sortBy,&sortDirection,&refreshFolders,&volumeOverride,&globalPitchToggle,&padFolderFilter,
        &microphoneToggle,&microphoneMonitorToggle
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
        ,&soundInspectorToggle
#endif
        }) addAndMakeVisible(component);
    keyboardViewport.setViewedComponent(&keyboard, false);
    keyboardViewport.setScrollBarsShown(false, true);
    keyboardViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    editorViewport.setViewedComponent(&editorPanel, false);
    editorViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(editorViewport); editorViewport.setVisible(false);
    editorPanel.onClose = [this]
    {
        stopEditorPreview(); editorViewport.setVisible(false); editingPadId.clear(); editingProfileId.clear();
    };
    editorPanel.onPreviewLoop = [this](bool enabled, double start, double end)
    { return previewEditorLoop(enabled, start, end); };
    editorPanel.onPreviewScrub = [this](double seconds) { previewEditorScrub(seconds); };
    editorPanel.onScrubStart = [this] { beginEditorScrub(); };
    editorPanel.onScrubEnd = [this](double seconds) { endEditorScrub(seconds); };
    editorPanel.onPlay = [this] { return previewEditorPlay(0.0); };
    editorPanel.onStop = [this] { stopEditorPreview(); };
    editorPanel.onSave = [this](const VoiceSettings& changed, std::optional<bool> loopChange)
    {
        auto* profile = library.findProfile(editingProfileId);
        if (!profile) return;
        for (auto& pad : profile->pads) if (pad.id == editingPadId)
        {
            pad.settings = changed;
            if (loopChange.has_value())
            {
                pad.useGlobalMode = false;
                pad.settings.mode = *loopChange ? PlayMode::toggleLoop : PlayMode::restart;
            }
            save(); selectPad(selection);
            stopEditorPreview();
            editorViewport.setVisible(false); editingPadId.clear(); editingProfileId.clear();
            setStatus("A-B ed effetti salvati per il suono.");
            return;
        }
    };
    for (auto* slider : {&globalPitch,&outputVolume,&monitorVolume,&microphoneVolume,&padVolume,&padSpeed,&fadeIn,&fadeOut})
    {
        addAndMakeVisible(slider); slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
        slider->setRange(0, 100, 1);
    }
    for (auto* slider : {&outputVolume,&monitorVolume,&microphoneVolume,&padVolume}) slider->setTextValueSuffix(" %");
    globalPitch.setRange(-12, 12, 0.1); globalPitch.setTextValueSuffix(" st");
    globalPitch.setDoubleClickReturnValue(true, 0.0);
    globalPitch.setTooltip(utf8("Pitch globale: da -12 a +12 semitoni. Agisce su entrambe le uscite e cambia anche velocità e durata. Doppio clic per tornare a 0."));
    globalPitch.setValue(library.globalPitch, juce::dontSendNotification);
    volumeOverride.setTooltip("Per questo suono il volume del pad sostituisce entrambi i master, anche a 0%.");
    globalPitchToggle.setToggleState(library.globalPitchEnabled, juce::dontSendNotification);
    volumeOverride.onClick = [this] { changePad(); };
    globalPitchToggle.onClick = [this]
    {
        library.globalPitchEnabled = globalPitchToggle.getToggleState(); updateAudioControls(); save();
    };
    globalPitch.onValueChange = [this]
    {
        library.globalPitch = globalPitch.getValue(); updateAudioControls(); dirty = true; dirtyTicks = 0;
    };
    padSpeed.setRange(0.5, 2, 0.01); padSpeed.setTextValueSuffix(" x");
    fadeIn.setRange(0, 10, 0.01); fadeOut.setRange(0, 10, 0.01);
    outputVolume.setValue(library.outputVolume * 100, juce::dontSendNotification);
    monitorVolume.setValue(library.monitorVolume * 100, juce::dontSendNotification);
    microphoneVolume.setValue(library.microphoneVolume * 100, juce::dontSendNotification);
    outputVolume.setTooltip("Volume dei suoni nell'uscita principale");
    monitorVolume.setTooltip("Volume dei suoni nelle cuffie");
    outputVolume.onValueChange = [this] { library.outputVolume = static_cast<float>(outputVolume.getValue() / 100); audio.main.core.setVolume(library.outputVolume); dirty = true; dirtyTicks = 0; };
    monitorVolume.onValueChange = [this] { library.monitorVolume = static_cast<float>(monitorVolume.getValue() / 100); audio.monitor.core.setVolume(library.monitorVolume); dirty = true; dirtyTicks = 0; };
    microphoneVolume.onValueChange = [this]
    {
        library.microphoneVolume = static_cast<float>(microphoneVolume.getValue() / 100.0);
        audio.setMicrophoneGain(library.microphoneVolume); dirty = true; dirtyTicks = 0;
    };
    globalMode.addItemList({"Normale (riavvia)", "Spam (sovrapponi)", "Loop on/off", "Loop premuto"}, 1);
    padMode.addItem(utf8("Usa modalità generale"), 1);
    padMode.addItemList({"Normale (riavvia)", "Spam (sovrapponi)", "Loop on/off", "Loop premuto"}, 2);
    globalMode.setSelectedId(static_cast<int>(library.defaultMode) + 1, juce::dontSendNotification);
    globalMode.onChange = [this] { stopAll(); library.defaultMode = static_cast<PlayMode>(globalMode.getSelectedId() - 1); save(); rebuildTiles(); };
    profiles.onChange = [this]
    {
        if (updating || profiles.getSelectedId() <= 0) return;
        stopAll(); editorViewport.setVisible(false); editingPadId.clear(); editingProfileId.clear();
        library.activeProfile = profiles.getSelectedId() - 1; page = 0; selection = -1; refresh(); save();
        if (!library.current().folders.empty()) scanProfileFolders(library.current().id);
    };
    newProfile.onClick = [this]
    {
        stopAll(); library.profiles.push_back({juce::Uuid().toString(), "Profilo " + juce::String(library.profiles.size() + 1), {}});
        library.activeProfile = static_cast<int>(library.profiles.size()) - 1; selection = -1; page = 0; refresh(); save();
    };
    renameProfile.onClick = [this] { renameCurrentProfile(); };
    renameProfile.setTooltip("Rinomina il profilo conservando suoni e associazioni.");
    deleteProfile.onClick = [this] { removeCurrentProfile(); };
    deleteProfile.setTooltip("Rimuove il profilo e le sue associazioni. I file audio restano sul disco.");
    sortBy.addItemList({"Ordine di aggiunta", "Nome", "Data modifica", "Dimensione", "Tipo di file"}, 1);
    sortBy.setTooltip("Ordina i suoni e aggiorna la numerazione dei pad secondo il criterio selezionato.");
    sortBy.onChange = [this]
    {
        if (updating || sortBy.getSelectedId() < 1) return;
        library.current().sortField = static_cast<SortField>(sortBy.getSelectedId() - 1);
        page = 0; rebuildTiles(); save();
    };
    sortDirection.onClick = [this]
    {
        library.current().ascending = !library.current().ascending;
        sortDirection.setButtonText(library.current().ascending ? "Crescente" : "Decrescente");
        page = 0; rebuildTiles(); save();
    };
    refreshFolders.onClick = [this]
    {
        if (scanningFolders)
        {
            if (scanCancelled) scanCancelled->store(true);
            refreshFolders.setButtonText("Annullamento..."); refreshFolders.setEnabled(false); return;
        }
        scanProfileFolders(library.current().id);
    };
    importButton.setTooltip("Collega una cartella di file audio (anche con sottocartelle). I suoni vengono letti dalla posizione originale.");
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    soundInspectorToggle.setClickingTogglesState(true);
    soundInspectorToggle.setTooltip("Mostra o nasconde i controlli del suono selezionato.");
    soundInspectorToggle.setTitle("Dettagli del suono");
    soundInspectorToggle.getProperties().set("inspector-toggle-icon", true);
#endif
    search.setTextToShowWhenEmpty("Cerca nella libreria...", palette::muted); search.setFont(font(14));
    search.onTextChange = [this]
    {
        page = 0;
        if (library.current().pads.size() > 5000)
        { searchChangedAt = juce::Time::getMillisecondCounter(); searchPending = true; }
        else rebuildTiles();
    };
    favoritesOnly.onClick = [this] { page = 0; rebuildTiles(); };
    padFolderFilter.setTooltip("Scegli una cartella. Anche in Tutte le cartelle ogni pagina contiene suoni di una sola cartella.");
    padFolderFilter.onChange = [this] { page = 0; rebuildTiles(); };
    boardTab.onClick = [this] { setView(0); }; audioTab.onClick = [this] { setView(1); };
    keyboardTab.onClick = [this] { setView(2); };
    replaceFile.onClick = [this] { replaceAudioFile(); };
    editorButton.onClick = [this] { openPadEditor(); };
    keyboardDevice.onChange = [this]
    {
        if (updating) return;
        stopAll(); rawKeyboard.enable(false); rawKeyboard.identify(false);
        captureKeyboard.setToggleState(false, juce::dontSendNotification);
        const auto index = keyboardDevice.getSelectedId() - 1;
        if (index >= 0 && index < static_cast<int>(keyboardDevices.size()))
        {
            library.keyboardDevice = keyboardDevices[index].id;
            const auto result = rawKeyboard.select(library.keyboardDevice);
            if (result.failed()) setStatus(result.getErrorMessage(), true);
            save();
        }
        updateKeyboardStatus();
    };
    scanKeyboards.onClick = [this]
    {
        stopAll(); captureKeyboard.setToggleState(false, juce::dontSendNotification); refreshKeyboards();
    };
    identifyKeyboard.onClick = [this]
    {
        if (rawKeyboard.identifying()) { rawKeyboard.identify(false); updateKeyboardStatus(); return; }
        stopAll(); heldKeys.clear(); rawKeyboard.identify(true); captureKeyboard.setToggleState(false, juce::dontSendNotification);
        updateKeyboardStatus();
        keyboardStatus.setText("Premi e rilascia F12 sulla tastiera da dedicare ai suoni.", juce::dontSendNotification);
    };
    captureKeyboard.onClick = [this]
    {
        stopAll(); heldKeys.clear();
        rawKeyboard.identify(false); rawKeyboard.enable(captureKeyboard.getToggleState());
        captureKeyboard.setToggleState(rawKeyboard.enabled() || rawKeyboard.activating(), juce::dontSendNotification);
        updateKeyboardStatus(); grabKeyboardFocus();
    };
    rawKeyboard.onLocalKey = [this](const juce::String& key, bool down) { handleLocalKey(key, down); };
    rawKeyboard.onKey = [this](const juce::String& key, bool down)
    {
        if (hasKeyboardFocus(true) && textEditorFocused() && !binding) { if (!down) handleKey(key, false); return; }
        if (down) lastKey.setText("Ultimo tasto: " + keyLabel(key), juce::dontSendNotification);
        handleKey(key, down);
    };
    rawKeyboard.onCaptureStateChanged = [this]
    {
        const bool requested = rawKeyboard.enabled() || rawKeyboard.activating();
        captureKeyboard.setToggleState(requested, juce::dontSendNotification);
        if (!requested) { stopAll(); heldKeys.clear(); }
        updateKeyboardStatus();
        if (!requested && rawKeyboard.error().isNotEmpty()) setStatus(rawKeyboard.error(), true);
    };
    rawKeyboard.onIdentify = [this](const juce::String& id)
    {
        library.keyboardDevice = id; refreshKeyboards(); save();
        keyboardStatus.setText("Tastiera riconosciuta. Rilascia i tasti e attiva l'interruttore.", juce::dontSendNotification);
    };
    rawKeyboard.onDevicesChanged = [this]
    {
        stopAll(); heldKeys.clear(); captureKeyboard.setToggleState(false, juce::dontSendNotification); refreshKeyboards();
        keyboardStatus.setText("Dispositivi cambiati: verifica la selezione e riattiva la tastiera.", juce::dontSendNotification);
    };
    importButton.onClick = [this] { chooseAudioFolder(); };
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    soundInspectorToggle.onClick = [this]
    {
        if (mainScreenPanel) mainScreenPanel->setInspectorOpen(soundInspectorToggle.getToggleState());
    };
#endif
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff472c38));
    stopButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffb7c6));
    stopButton.onClick = [this] { stopAll(); setStatus("Tutti i suoni fermati."); };
    djButton.onStateChange = [this]
    {
        djMouseDown = djButton.isDown(); audio.pause(djMouseDown || djKeyDown);
        djButton.setButtonText(djMouseDown || djKeyDown ? "DJ IN PAUSA" : "Pausa DJ");
    };
    djButton.setTooltip("Tieni premuto per congelare la riproduzione; rilascia per riprendere.");
    previous.onClick = [this] { --page; rebuildTiles(); }; next.onClick = [this] { ++page; rebuildTiles(); };
    tilesButton.onClick = [this] { keyboardMode = false; setView(0); };
    keyboardButton.onClick = [this] { keyboardMode = true; setView(0); };
    bindButton.onClick = [this] { if (selectedPad()) { binding = true; grabKeyboardFocus(); setStatus("Premi il tasto da associare, oppure fai clic destro su un tasto della tastiera grafica."); } };
    unbindButton.onClick = [this] { if (auto* p = selectedPad()) { keyIndex.erase(p->key); p->key.clear(); p->keyLabel.clear(); save(); selectPad(selection); rebuildTiles(); } };
    for (auto* slider : {&padVolume,&padSpeed,&fadeIn,&fadeOut}) slider->onValueChange = [this] { changePad(); };
    padMode.onChange = [this] { changePad(); }; favoriteButton.onClick = [this] { changePad(); };
    padName.onReturnKey = [this] { changePad(); grabKeyboardFocus(); };
    padName.onFocusLost = [this] { changePad(); };
    for (auto& tile : tiles)
    {
        tile = std::make_unique<PadTile>(); addAndMakeVisible(*tile); tile->setWantsKeyboardFocus(true);
        tile->select = [this](int index) { selectPad(index); };
        tile->press = [this](int index, bool down) { triggerPad(index, down); };
        tile->contextMenu = [this](int index) { showPadMenu(index); };
    }
    keyboard.setWantsKeyboardFocus(true);
    keyboard.press = [this](const juce::String& key, bool down) { handleKey(key, down); };
    keyboard.assign = [this](const juce::String& key) { bindKey(key); };
    applyButton.onClick = [this] { if (applyAudio()) save(); };
    scanButton.onClick = [this] { refreshDevices(); setStatus("Elenco dispositivi aggiornato."); };
    monitorToggle.setToggleState(library.monitorEnabled, juce::dontSendNotification);
    microphoneToggle.setToggleState(library.microphoneEnabled, juce::dontSendNotification);
    microphoneMonitorToggle.setToggleState(library.microphoneMonitor, juce::dontSendNotification);
    updateAudioControls();
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    audioSettingsPanel = std::make_unique<AudioSettingsPanel>(std::initializer_list<AudioSettingsPanel::Control>{
        {"outputHeading", outputLabel}, {"outputDevice", outputDevice},
        {"monitorHeading", monitorLabel}, {"monitorEnabled", monitorToggle}, {"monitorDevice", monitorDevice},
        {"microphoneHeading", microphoneLabel}, {"microphoneEnabled", microphoneToggle}, {"inputDevice", inputDevice},
        {"microphoneMonitor", microphoneMonitorToggle}, {"microphoneGainHeading", microphoneGainLabel},
        {"microphoneGain", microphoneVolume}, {"applyAudio", applyButton}, {"refreshDevices", scanButton}},
        smokeTest ? juce::File() : projectRoot.getChildFile("data/foleys-audio-ui.xml"));
    addAndMakeVisible(*audioSettingsPanel);
#endif
#if defined(DUALDECK_FOLEYS_DESIGNER)
    editAppearanceButton.setComponentID("editAppearance");
    saveAppearanceButton.setComponentID("saveAppearance");
    resetAppearanceButton.setComponentID("resetAppearance");
    for (auto* action : {&editAppearanceButton, &saveAppearanceButton, &resetAppearanceButton}) addAndMakeVisible(action);
    editAppearanceButton.onClick = [this]
    {
        if (audioSettingsPanel->isDesignerVisible())
        {
            audioSettingsPanel->hideDesigner();
            editAppearanceButton.setButtonText("Modifica aspetto");
        }
        else showAudioUiDesigner();
    };
    saveAppearanceButton.onClick = [this]
    {
        const auto result = audioSettingsPanel->saveAppearance();
        setStatus(result.wasOk() ? "Aspetto salvato." : result.getErrorMessage(), result.failed());
    };
    resetAppearanceButton.onClick = [this]
    {
        const auto result = audioSettingsPanel->resetAppearance();
        setStatus(result.wasOk() ? "Aspetto iniziale ripristinato." : result.getErrorMessage(), result.failed());
    };
    editAppearanceButton.setTooltip("Apri il designer visuale per modificare la pagina audio.");
    saveAppearanceButton.setTooltip("Conserva l'aspetto e ricaricalo ai prossimi avvii.");
    resetAppearanceButton.setTooltip("Ripristina e salva l'aspetto iniziale della pagina audio.");
#endif
    refreshDevices(); refreshKeyboards(); refresh(); setView(0); setSize(1380, 880);
    if (!smokeTest)
    {
        const auto keyboardResult = rawKeyboard.initialise();
        rawKeyboardReady = keyboardResult.wasOk(); updateKeyboardStatus();
        if (keyboardResult.failed()) { captureKeyboard.setEnabled(false); identifyKeyboard.setEnabled(false); keyboardStatus.setText(keyboardResult.getErrorMessage(), juce::dontSendNotification); }
    }
    if (!smokeTest) applyAudio();
    if (loaded.failed()) setStatus(loaded.getErrorMessage(), true);
    else if (smokeTest || audio.main.isOpen()) setStatus("Pronto");
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (!statusIsError && audioSettingsPanel->getAppearanceLoadResult().failed())
        setStatus(audioSettingsPanel->getAppearanceLoadResult().getErrorMessage(), true);
#endif
    initialiseProgramAppearance();
    startTimerHz(20);
    if (!smokeTest && !library.current().folders.empty()) scanProfileFolders(library.current().id);
#if defined(DUALDECK_UI_EDITOR)
    if (uiEditor)
    {
        layoutEditor = std::make_unique<UiLayoutEditor>(*this, projectRoot.getChildFile("data/ui-debug-layout.json"),
            [this](int section)
            {
                setView(section == 3 ? 0 : section);
                if (section == 3)
                {
                    editorPanel.setPad("Anteprima editor", VoiceSettings {});
                    editorViewport.setVisible(true);
                    editorViewport.toFront(false);
                }
                if (layoutEditor) layoutEditor->toFront(false);
            });
        addAndMakeVisible(*layoutEditor);
        layoutEditor->setBounds(getLocalBounds());
        layoutEditor->load();
        layoutEditor->applyOverrides();
        layoutEditor->toFront(false);
    }
#endif
}
MainComponent::~MainComponent()
{
    rawKeyboard.onCaptureStateChanged = {};
    rawKeyboard.onLocalKey = {}; rawKeyboard.enable(false); rawKeyboard.onKey = {}; rawKeyboard.onIdentify = {}; rawKeyboard.onDevicesChanged = {};
    if (scanCancelled) scanCancelled->store(true);
    folderLoader.removeAllJobs(true, -1);
    stopTimer(); ++playGeneration; ++preloadGeneration;
    playbackLoader.removeAllJobs(true, -1); preloadLoader.removeAllJobs(true, -1);
    waveformLoader.removeAllJobs(true, -1);
    saveLoader.removeAllJobs(false, -1);
    if (!smokeTest && writable && (dirty || savedGeneration.load() < saveGeneration.load()))
    {
        const auto result = library.save(libraryFile);
        if (result.failed()) juce::Logger::writeToLog("Salvataggio finale DualDeck non riuscito: " + result.getErrorMessage());
    }
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    mainScreenPanel.reset();
    programAppearance.reset();
    audioSettingsPanel.reset();
#endif
    audio.stop(); removeKeyListener(this); setLookAndFeel(nullptr);
}
void MainComponent::showAudioUiDesigner()
{
#if defined(DUALDECK_FOLEYS_DESIGNER)
    if (mainScreenPanel) mainScreenPanel->hideDesigner();
    if (programAppearance) programAppearance->hideDesigner();
    setView(1);
    audioSettingsPanel->showDesigner();
    editAppearanceButton.setButtonText("Fine modifica");
#endif
}
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(uiColour(*this, uiColours::background));
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (mainScreenPanel) return;
#endif
    g.setColour(uiColour(*this, uiColours::panel)); g.fillRect(0, 0, 196, getHeight());
    g.setColour(uiColour(*this, uiColours::line)); g.drawVerticalLine(195, 0, static_cast<float>(getHeight()));
    g.setColour(uiColour(*this, uiColours::accent)); g.fillRoundedRectangle(22, 25, 32, 32, uiRadius(*this, 8));
    g.setColour(uiColour(*this, uiColours::background)); g.setFont(uiFont(*this, 19, true)); g.drawText("D", 22, 25, 32, 32, juce::Justification::centred);
    g.setColour(uiColour(*this, uiColours::text)); g.setFont(uiFont(*this, 21, true)); g.drawText("DualDeck", 63, 25, 126, 25, juce::Justification::centredLeft);
    g.setColour(uiColour(*this, uiColours::line)); g.drawHorizontalLine(getHeight() - 76, 196, static_cast<float>(getWidth()));
    if (view == 0)
    {
        g.setColour(uiColour(*this, uiColours::panel)); g.fillRoundedRectangle(inspector.toFloat(), uiRadius(*this, 12));
        if (filtered.empty())
        {
            g.setColour(uiColour(*this, uiColours::muted)); g.setFont(uiFont(*this, 19));
            g.drawFittedText(library.current().pads.empty() ? "Nessun suono qui.\nTrascina una cartella o usa Collega cartella."
                : "Nessun suono corrisponde ai filtri.\nControlla cartella, ricerca e preferiti.", centre.withTrimmedTop(160), juce::Justification::centred, 3);
        }
    }
    else
    {
        g.setColour(uiColour(*this, uiColours::panel)); g.fillRoundedRectangle(centre.toFloat(), uiRadius(*this, 12));
    }
    const float meter = audio.main.core.meter();
    auto meterBox = juce::Rectangle<float>(static_cast<float>(getWidth() - 212), static_cast<float>(getHeight() - 29), 184, 5);
    g.setColour(uiColour(*this, uiColours::line)); g.fillRoundedRectangle(meterBox, 2.5f);
    g.setColour(meter > 0.9f ? uiColour(*this, uiColours::warning) : uiColour(*this, uiColours::accent)); g.fillRoundedRectangle(meterBox.withWidth(meterBox.getWidth() * juce::jlimit(0.0f, 1.0f, meter)), 2.5f);
}
void MainComponent::paintOverChildren(juce::Graphics& g)
{
    if (editorViewport.isVisible())
    {
        g.setColour(uiColour(*this, uiColours::accent));
        g.drawRoundedRectangle(editorViewport.getBounds().toFloat().reduced(1.0f), uiRadius(*this, 12.0f), 2.0f);
    }
}
int MainComponent::preferredHeight(int width) const
{
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (view != 0 || keyboardMode) return 800;
    const int columns = juce::jlimit(1, 6, (width - 546 + 9) / 129);
    const int rows = (24 + columns - 1) / columns;
    return juce::jmax(800, 26 + (width < 1200 ? 136 : 78) + 44 + 38 + 68 + rows * 100 + (rows - 1) * 9 + 76);
#else
    if (view != 0 || keyboardMode) return 880;
    const int boardWidth = width - 546;
    const int columns = juce::jlimit(2, 6, (boardWidth + 9) / 129);
    const int rows = (24 + columns - 1) / columns;
    return juce::jmax(880, 346 + (width < 1200 ? 54 : 0) + rows * 100 + (rows - 1) * 9 + 100);
#endif
}
void MainComponent::updateEditorViewportBounds()
{
    const int editorWidth = juce::jmin(820, getWidth() - 40);
    const auto* outerViewport = findParentComponentOfClass<juce::Viewport>();
    const int visibleHeight = outerViewport != nullptr ? outerViewport->getHeight() : getHeight();
    const int editorHeight = juce::jmax(280, juce::jmin(getHeight(), visibleHeight) - 80);
    editorViewport.setBounds((getWidth() - editorWidth) / 2, 40, editorWidth, editorHeight);
    editorPanel.setSize(editorWidth - editorViewport.getScrollBarThickness(), 1410);
}
void MainComponent::parentSizeChanged() { updateEditorViewportBounds(); }
void MainComponent::resized()
{
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (mainScreenPanel)
    {
        mainScreenPanel->setBounds(getLocalBounds()); mainScreenPanel->resized();
        updateEditorViewportBounds();
        if (editorViewport.isVisible()) editorViewport.toFront(false);
        return;
    }
#endif
    const int rightWidth = 282, gap = 22;
    const int compactShift = getWidth() < 1200 ? 54 : 0;
    const int boardTopShift = view == 0 ? 72 : 0;
    const int inspectorDetailShift = view == 0 ? 18 : 0;
    auto area = getLocalBounds().withTrimmedLeft(218).withTrimmedRight(24);
    title.setBounds(area.getX(), 25, area.getWidth() - 230, 36);
    importButton.setBounds(getWidth() - 210, 32, 186, 40);
#if defined(DUALDECK_FOLEYS_DESIGNER)
    editAppearanceButton.setBounds(area.getX(), 72, 168, 30);
    saveAppearanceButton.setBounds(area.getX() + 176, 72, 136, 30);
    resetAppearanceButton.setBounds(area.getX() + 320, 72, 120, 30);
#endif
    boardTab.setBounds(16, 139, 164, 42); audioTab.setBounds(16, 190, 164, 42);
    keyboardTab.setBounds(16, 241, 164, 42);
    outputMasterLabel.setBounds(20, 307, 160, 22); outputVolume.setBounds(12, 335, 174, 30);
    monitorMasterLabel.setBounds(20, 382, 160, 22); monitorVolume.setBounds(12, 410, 174, 30);
    globalPitchToggle.setBounds(20, 466, 160, 28);
    globalPitch.setBounds(12, 500, 174, 30);
    folderSummary.setBounds(20, 565, 160, 54); refreshFolders.setBounds(16, 625, 164, 36);
    inputStatus.setBounds(20, getHeight() - 208, 158, 105);
    stopButton.setBounds(16, getHeight() - 94, 164, 38);
    djButton.setBounds(16, getHeight() - 49, 164, 32);
    profileLabel.setBounds(area.getX(), 112 - boardTopShift, 170, 17);
    profiles.setBounds(area.getX(), 135 - boardTopShift, 168, 34); newProfile.setBounds(area.getX() + 176, 135 - boardTopShift, 30, 34);
    renameProfile.setBounds(area.getX() + 214, 135 - boardTopShift, 80, 34); deleteProfile.setBounds(area.getX() + 302, 135 - boardTopShift, 80, 34);
    modeLabel.setBounds(area.getX() + (compactShift ? 0 : 398), 112 + compactShift + (compactShift ? 14 : 0) - boardTopShift, 220, 17);
    globalMode.setBounds(area.getX() + (compactShift ? 0 : 398), 135 + compactShift + (compactShift ? 12 : 0) - boardTopShift, 210, 34);
    centre = {area.getX(), 194 + compactShift - boardTopShift, area.getWidth() - rightWidth - gap, getHeight() - 294 - compactShift + boardTopShift};
    inspector = {getWidth() - 24 - rightWidth, 112 - boardTopShift, rightWidth, getHeight() - 212 + boardTopShift};
    if (view == 0) importButton.setBounds(inspector.getRight() - 203, inspector.getY() + 17, 186, 40);
    search.setBounds(centre.getX(), 194 + compactShift - boardTopShift, juce::jmax(120, centre.getWidth() - 178), 34);
    tilesButton.setBounds(centre.getRight() - 164, 194 + compactShift - boardTopShift, 67, 34); keyboardButton.setBounds(centre.getRight() - 89, 194 + compactShift - boardTopShift, 89, 34);
    favoritesOnly.setBounds(centre.getX(), 239 + compactShift - boardTopShift, 130, 25);
    sortBy.setBounds(centre.getX() + 135, 237 + compactShift - boardTopShift, 160, 29);
    sortDirection.setBounds(centre.getX() + 302, 237 + compactShift - boardTopShift, 96, 29);
    padFolderFilter.setBounds(centre.getX(), 278 + compactShift - boardTopShift, juce::jmax(180, centre.getWidth() - 310), 30);
    pageLabel.setBounds(padFolderFilter.getRight() + 8, 280 + compactShift - boardTopShift, centre.getRight() - padFolderFilter.getRight() - 104, 25);
    previous.setBounds(centre.getRight() - 84, 278 + compactShift - boardTopShift, 36, 29); next.setBounds(centre.getRight() - 39, 278 + compactShift - boardTopShift, 36, 29);
    padFolderHeading.setBounds(centre.getX(), 316 + compactShift - boardTopShift, centre.getWidth(), 22);
    auto grid = centre.withTop((keyboardMode ? 282 : 346) + compactShift - boardTopShift);
    const int columns = juce::jlimit(2, 6, (grid.getWidth() + 9) / 129);
    const int rows = (24 + columns - 1) / columns, spacing = 9;
    const int width = (grid.getWidth() - (columns - 1) * spacing) / columns;
    const int height = (grid.getHeight() - (rows - 1) * spacing) / rows;
    for (int i = 0; i < 24; ++i) tiles[i]->setBounds(grid.getX() + (i % columns) * (width + spacing), grid.getY() + (i / columns) * (height + spacing), width, height);
    keyboardViewport.setBounds(grid);
    keyboard.setSize(juce::jmax(800, grid.getWidth()), juce::jmax(360, grid.getHeight() - keyboardViewport.getScrollBarThickness()));
    auto detail = inspector.reduced(17);
    const int x = detail.getX(), w = detail.getWidth();
    padHeading.setBounds(x, 129 - inspectorDetailShift, w, 22); padName.setBounds(x, 162 - inspectorDetailShift, w, 34);
    padFile.setBounds(x, 202 - inspectorDetailShift, w - 88, 24); replaceFile.setBounds(x + w - 84, 201 - inspectorDetailShift, 84, 26); keyInfo.setBounds(x, 239 - inspectorDetailShift, w, 24);
    bindButton.setBounds(x, 273 - inspectorDetailShift, w - 65, 33); unbindButton.setBounds(x + w - 59, 273 - inspectorDetailShift, 59, 33);
    favoriteButton.setBounds(x, 319 - inspectorDetailShift, 88, 25);
    editorButton.setBounds(x + 92, 317 - inspectorDetailShift, w - 92, 31);
    volumeOverride.setBounds(x, 360 - inspectorDetailShift, w, 28);
    volumeLabel.setBounds(x, 395 - inspectorDetailShift, w, 23); padVolume.setBounds(x, 421 - inspectorDetailShift, w, 28);
    speedLabel.setBounds(x, 462 - inspectorDetailShift, w, 23); padSpeed.setBounds(x, 488 - inspectorDetailShift, w, 28);
    padModeLabel.setBounds(x, 530 - inspectorDetailShift, w, 23); padMode.setBounds(x, 559 - inspectorDetailShift, w, 33);
    fadeLabel.setBounds(x, 611 - inspectorDetailShift, w, 23); fadeIn.setBounds(x, 638 - inspectorDetailShift, w, 27); fadeOut.setBounds(x, 669 - inspectorDetailShift, w, 27);
    statusLabel.setBounds(218, getHeight() - 66, getWidth() - 460, 52);
    updateEditorViewportBounds();
    if (view == 1)
    {
        centre = {218, 112, getWidth() - 242, getHeight() - 212};
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
        if (audioSettingsPanel) audioSettingsPanel->setBounds(centre);
#else
        const int ax = centre.getX() + 28, aw = juce::jmin(650, centre.getWidth() - 56);
        outputLabel.setBounds(ax, 140, aw, 24); outputDevice.setBounds(ax, 172, aw, 36);
        monitorLabel.setBounds(ax, 240, aw, 24); monitorToggle.setBounds(ax, 272, aw, 28);
        monitorDevice.setBounds(ax, 308, aw, 36);
        microphoneLabel.setBounds(ax, 384, aw, 24);
        microphoneToggle.setBounds(ax, 416, aw, 28);
        inputDevice.setBounds(ax, 452, aw, 36);
        microphoneMonitorToggle.setBounds(ax, 504, aw, 28);
        microphoneGainLabel.setBounds(ax, 548, 190, 26);
        microphoneVolume.setBounds(ax + 190, 548, aw - 190, 30);
        applyButton.setBounds(ax, 618, 180, 38); scanButton.setBounds(ax + 192, 618, 210, 38);
#endif
    }
    if (view == 2)
    {
        centre = {218, 112, getWidth() - 242, getHeight() - 212};
        const int ax = centre.getX() + 28, aw = juce::jmin(840, centre.getWidth() - 56);
        keyboardHeading.setBounds(ax, 140, aw, 24);
        keyboardDevice.setBounds(ax, 182, aw, 38);
        identifyKeyboard.setBounds(ax, 238, 190, 38); scanKeyboards.setBounds(ax + 202, 238, 190, 38);
        captureKeyboard.setBounds(ax, 303, aw, 32);
        keyboardStatus.setBounds(ax, 350, aw, 48);
        lastKey.setBounds(ax, 406, aw, 26);
        keyboardHelp.setBounds(ax, 459, aw, 48);
    }
#if defined(DUALDECK_UI_EDITOR)
    if (layoutEditor)
    {
        layoutEditor->setBounds(getLocalBounds());
        layoutEditor->applyOverrides();
        layoutEditor->toFront(false);
    }
#endif
}
void MainComponent::setView(int section)
{
    if (section != view) hideAppearanceEditors();
    if (section != 0)
    {
        stopEditorPreview(); editorViewport.setVisible(false); editingPadId.clear(); editingProfileId.clear();
    }
    view = section;
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (audioSettingsPanel) audioSettingsPanel->setVisible(view == 1);
#endif
#if defined(DUALDECK_FOLEYS_DESIGNER)
    for (auto* action : {&editAppearanceButton, &saveAppearanceButton, &resetAppearanceButton}) action->setVisible(view == 1);
    if (view != 1 && audioSettingsPanel)
    {
        audioSettingsPanel->hideDesigner();
        editAppearanceButton.setButtonText("Modifica aspetto");
    }
#endif
    boardTab.setToggleState(view == 0, juce::dontSendNotification); audioTab.setToggleState(view == 1, juce::dontSendNotification);
    keyboardTab.setToggleState(view == 2, juce::dontSendNotification);
    tilesButton.setToggleState(!keyboardMode, juce::dontSendNotification); keyboardButton.setToggleState(keyboardMode, juce::dontSendNotification);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&profileLabel,&profiles,&newProfile,&renameProfile,&deleteProfile,&sortBy,&sortDirection,&modeLabel,&globalMode,&search,&favoritesOnly,&pageLabel,&previous,&next,&tilesButton,&keyboardButton,
        &padHeading,&padName,&padFile,&replaceFile,&editorButton,&keyInfo,&bindButton,&unbindButton,&favoriteButton,&volumeOverride,&volumeLabel,&speedLabel,&fadeLabel,&padModeLabel,&padVolume,&padSpeed,&fadeIn,&fadeOut,&padMode,&globalPitchToggle,&globalPitch}) c->setVisible(view == 0);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&outputLabel,&outputDevice,&monitorLabel,&monitorToggle,&monitorDevice,&applyButton,&scanButton,&microphoneLabel,&microphoneToggle,&inputDevice,&microphoneMonitorToggle,&microphoneGainLabel,&microphoneVolume}) c->setVisible(view == 1);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&keyboardHeading,&keyboardDevice,&identifyKeyboard,&scanKeyboards,&captureKeyboard,&keyboardStatus,&lastKey,&keyboardHelp}) c->setVisible(view == 2);
    title.setText(view == 0 ? "Soundboard" : view == 1 ? "Audio e uscite" : "Tastiera dedicata", juce::dontSendNotification);
    title.setVisible(view != 0);
    if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
    {
        const int width = juce::jmax(900, viewport->getWidth() - 20);
        setSize(width, juce::jmax(preferredHeight(width), viewport->getViewHeight()));
    }
    rebuildTiles(); resized(); repaint();
}
void MainComponent::refresh()
{
    missingFiles = static_cast<int>(std::count_if(library.current().pads.begin(), library.current().pads.end(), [](const Pad& p) { return p.fileMissing; }));
    keyIndex.clear();
    for (std::size_t i = 0; i < library.current().pads.size(); ++i)
        if (library.current().pads[i].key.isNotEmpty()) keyIndex.emplace(library.current().pads[i].key, static_cast<int>(i));
    updating = true; profiles.clear(juce::dontSendNotification);
    for (std::size_t i = 0; i < library.profiles.size(); ++i) profiles.addItem(library.profiles[i].name, static_cast<int>(i) + 1);
    profiles.setSelectedId(library.activeProfile + 1, juce::dontSendNotification);
    globalMode.setSelectedId(static_cast<int>(library.defaultMode) + 1, juce::dontSendNotification);
    sortBy.setSelectedId(static_cast<int>(library.current().sortField) + 1, juce::dontSendNotification);
    sortDirection.setButtonText(library.current().ascending ? "Crescente" : "Decrescente");
    const auto& folders = library.current().folders;
    folderSummary.setText(juce::String(folders.size()) + (folders.size() == 1 ? " cartella collegata" : " cartelle collegate"), juce::dontSendNotification);
    juce::StringArray paths; for (const auto& folder : folders) paths.add(folder.getFullPathName());
    folderSummary.setTooltip(paths.joinIntoString("\n"));
    updating = false; rebuildTiles();
    if (selection < 0 && !library.current().pads.empty()) selection = 0;
    selectPad(selection);
}
void MainComponent::rebuildTiles()
{
    const auto& pads = library.current().pads;
    padNumbers = padDisplayNumbers(library.current());
    const int folderIndex = padFolderFilter.getSelectedId() - 2;
    auto selectedFolder = folderIndex >= 0 && folderIndex < static_cast<int>(padFolders.size())
        ? padFolders[static_cast<std::size_t>(folderIndex)] : juce::File();
    if (padFolderProfile != library.current().id)
    {
        selectedFolder = {}; page = 0; padFolderProfile = library.current().id;
    }
    std::set<juce::String> folderPaths;
    for (const auto& pad : pads) folderPaths.insert(pad.file.getParentDirectory().getFullPathName().toLowerCase());
    std::vector<juce::File> folders;
    for (const auto& pad : pads)
    {
        const auto parent = pad.file.getParentDirectory();
        if (folderPaths.erase(parent.getFullPathName().toLowerCase()) != 0) folders.push_back(parent);
    }
    std::sort(folders.begin(), folders.end(), [](const auto& a, const auto& b)
    { return a.getFullPathName().compareNatural(b.getFullPathName()) < 0; });
    padFolders = std::move(folders);
    padFolderFilter.clear(juce::dontSendNotification);
    padFolderFilter.addItem("Tutte le cartelle", 1);
    int selectedFolderId = 1;
    for (std::size_t i = 0; i < padFolders.size(); ++i)
    {
        const int id = static_cast<int>(i) + 2;
        padFolderFilter.addItem(padFolders[i].getFileName() + "  |  " + padFolders[i].getParentDirectory().getFullPathName(), id);
        if (padFolders[i] == selectedFolder) selectedFolderId = id;
    }
    if (selectedFolderId == 1) selectedFolder = {};
    padFolderFilter.setSelectedId(selectedFolderId, juce::dontSendNotification);
    filtered = orderedPads(library.current(), search.getText(), favoritesOnly.getToggleState());
    const auto folderPages = folderPadPages(library.current(), filtered, keyboardMode ? juce::File() : selectedFolder);
    if (!keyboardMode)
    {
        filtered.clear();
        for (const auto& group : folderPages) filtered.insert(filtered.end(), group.indices.begin(), group.indices.end());
    }
    const int pages = juce::jmax(1, static_cast<int>(folderPages.size())); page = juce::jlimit(0, pages - 1, page);
    const auto* currentPage = folderPages.empty() ? nullptr : &folderPages[static_cast<std::size_t>(page)];
    padFolderHeading.setText(currentPage ? currentPage->folder.getFileName() + "  /  "
        + juce::String(currentPage->indices.size()) + " suoni" : "", juce::dontSendNotification);
    padFolderHeading.setTooltip(currentPage ? currentPage->folder.getFullPathName() : juce::String());
    padFolderHeading.setVisible(view == 0 && !keyboardMode);
    padFolderFilter.setVisible(view == 0 && !keyboardMode);
    pageLabel.setVisible(view == 0 && !keyboardMode);
    previous.setVisible(view == 0 && !keyboardMode); next.setVisible(view == 0 && !keyboardMode);
    const auto missing = missingFiles;
    pageLabel.setText(juce::String(filtered.size()) + " suoni  /  " + juce::String(page + 1) + " di " + juce::String(pages) + (missing > 0 ? "  /  " + juce::String(static_cast<int>(missing)) + " mancanti" : ""), juce::dontSendNotification);
    semanticTextColour(pageLabel, missing > 0 ? uiColours::warning : uiColours::muted);
    pageLabel.setTooltip(pageLabel.getText());
    previous.setEnabled(page > 0 && !keyboardMode); next.setEnabled(page + 1 < pages && !keyboardMode);
    for (int i = 0; i < 24; ++i)
    {
        auto& tile = *tiles[i];
        const bool present = currentPage != nullptr && i < static_cast<int>(currentPage->indices.size()); tile.setVisible(view == 0 && !keyboardMode && present);
        if (!present) continue;
        tile.index = currentPage->indices[static_cast<std::size_t>(i)]; const auto& pad = pads[tile.index];
        tile.displayNumber = padNumber(tile.index);
        tile.title = pad.name; tile.binding = pad.keyLabel; tile.favorite = pad.favorite;
        tile.missing = pad.fileMissing; tile.selected = tile.index == selection;
        tile.subtitle = tile.missing ? "File non trovato" : modeName(pad.useGlobalMode ? library.defaultMode : pad.settings.mode) + (pad.settings.overrideMasterVolume ? " / " + juce::String(juce::roundToInt(pad.settings.gain * 100)) + "%" : " / Master");
        tile.setTitle("Pad " + juce::String(tile.displayNumber) + ": " + pad.name);
        tile.setDescription(tile.subtitle + (tile.binding.isEmpty() ? "" : ", tasto " + tile.binding)
            + (tile.missing ? ". Usa Ricollega file audio." : ". Invio per riprodurre; menu contestuale per altre azioni."));
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
        if (mainScreenPanel) mainScreenPanel->refreshControl("padTile" + juce::String(i + 1), true);
#endif
        tile.repaint();
    }
    if (selectedPad()) padHeading.setText("Suono #" + juce::String(padNumber(selection)), juce::dontSendNotification);
    keyboard.profile = &library.current(); keyboard.padNumbers = padNumbers;
    keyboardViewport.setVisible(view == 0 && keyboardMode); keyboard.updateAccessibleKey(); repaint();
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    if (mainScreenPanel)
    {
        emptyState.setText(library.current().pads.empty() ? "Nessun suono qui.\nTrascina una cartella o usa Collega cartella."
            : "Nessun suono corrisponde ai filtri.\nControlla cartella, ricerca e preferiti.", juce::dontSendNotification);
        mainScreenPanel->setScreen(view, keyboardMode, currentPage ? static_cast<int>(currentPage->indices.size()) : 0, filtered.empty());
    }
#endif
    preloadProfile(view == 0 && !keyboardMode && currentPage != nullptr ? currentPage->indices : std::vector<int>{});
}
Pad* MainComponent::selectedPad()
{
    auto& pads = library.current().pads;
    return selection >= 0 && selection < static_cast<int>(pads.size()) ? &pads[selection] : nullptr;
}
int MainComponent::padNumber(int index) const
{
    return index >= 0 && index < static_cast<int>(padNumbers.size()) ? padNumbers[static_cast<std::size_t>(index)] : 0;
}
void MainComponent::selectPad(int index)
{
    selection = index; updating = true; binding = false;
    auto* pad = selectedPad();
    if (editorViewport.isVisible() && (!pad || pad->id != editingPadId || library.current().id != editingProfileId))
    { stopEditorPreview(); editorViewport.setVisible(false); editingPadId.clear(); editingProfileId.clear(); }
    for (juce::Component* c : std::initializer_list<juce::Component*>{&padName,&replaceFile,&editorButton,&bindButton,&unbindButton,&favoriteButton,&volumeOverride,&padVolume,&padSpeed,&fadeIn,&fadeOut,&padMode}) c->setEnabled(pad != nullptr);
    if (pad)
    {
        padHeading.setText("Suono #" + juce::String(padNumber(index)), juce::dontSendNotification);
        replaceFile.setButtonText(pad->fileMissing ? "Ricollega" : "Cambia file");
        padName.setText(pad->name, false); padFile.setText(pad->file.getFileName(), juce::dontSendNotification); padFile.setTooltip(pad->file.getFullPathName());
        keyInfo.setText(pad->key.isEmpty() ? "Nessun tasto assegnato" : "Tasto  " + pad->keyLabel, juce::dontSendNotification);
        favoriteButton.setToggleState(pad->favorite, juce::dontSendNotification);
        volumeOverride.setToggleState(pad->settings.overrideMasterVolume, juce::dontSendNotification);
        padVolume.setValue(pad->settings.gain * 100, juce::dontSendNotification); padSpeed.setValue(pad->settings.speed, juce::dontSendNotification);
        fadeIn.setValue(pad->settings.fadeIn, juce::dontSendNotification); fadeOut.setValue(pad->settings.fadeOut, juce::dontSendNotification);
        padMode.setSelectedId(pad->useGlobalMode ? 1 : static_cast<int>(pad->settings.mode) + 2, juce::dontSendNotification);
        keyboard.selectedKey = pad->key;
    }
    else { padHeading.setText("Dettagli suono", juce::dontSendNotification); padName.clear(); padFile.setText("Seleziona un suono", juce::dontSendNotification); keyInfo.setText("", juce::dontSendNotification); volumeOverride.setToggleState(false, juce::dontSendNotification); keyboard.selectedKey.clear(); }
    padVolume.setEnabled(pad != nullptr && pad->settings.overrideMasterVolume);
    padVolume.setTooltip(pad && pad->settings.overrideMasterVolume ? "Sostituisce i master di entrambe le uscite." : "Attiva 'Usa il volume del suono' per regolare questo pad.");
    updating = false;
    for (auto& tile : tiles) if (tile->isVisible())
    {
        const bool selected = tile->index == selection;
        if (tile->selected != selected) { tile->selected = selected; tile->repaint(); }
    }
}
void MainComponent::changePad()
{
    if (updating) return;
    if (auto* pad = selectedPad())
    {
        const auto oldName = pad->name;
        const bool oldFavorite = pad->favorite;
        const bool oldOverride = pad->settings.overrideMasterVolume;
        if (padName.getText().trim().isNotEmpty()) pad->name = padName.getText().trim();
        pad->favorite = favoriteButton.getToggleState(); pad->settings.gain = static_cast<float>(padVolume.getValue() / 100.0);
        pad->settings.overrideMasterVolume = volumeOverride.getToggleState();
        padVolume.setEnabled(pad->settings.overrideMasterVolume);
        padVolume.setTooltip(pad->settings.overrideMasterVolume ? "Sostituisce i master di entrambe le uscite." : "Attiva 'Usa il volume del suono' per regolare questo pad.");
        pad->settings.speed = padSpeed.getValue();
        pad->settings.fadeIn = fadeIn.getValue(); pad->settings.fadeOut = fadeOut.getValue();
        pad->useGlobalMode = padMode.getSelectedId() == 1;
        if (!pad->useGlobalMode) pad->settings.mode = static_cast<PlayMode>(padMode.getSelectedId() - 2);
        dirty = true; dirtyTicks = 0;
        if (oldName != pad->name || oldFavorite != pad->favorite || oldOverride != pad->settings.overrideMasterVolume) rebuildTiles();
        else for (auto& tile : tiles) if (tile->isVisible() && tile->index == selection)
        {
            tile->subtitle = modeName(pad->useGlobalMode ? library.defaultMode : pad->settings.mode)
                + (pad->settings.overrideMasterVolume ? " / " + juce::String(juce::roundToInt(pad->settings.gain * 100)) + "%" : " / Master");
            tile->repaint();
        }
    }
}
void MainComponent::bindKey(const juce::String& key)
{
    auto* pad = selectedPad(); if (!pad) return;
    if (key == "0001:0" || key == "0058:0" || key == library.djStopKey) { setStatus("Tasto riservato a stop, pausa DJ o riconoscimento.", true); return; }
    if (const auto found = keyIndex.find(key); found != keyIndex.end() && found->second != selection)
    { setStatus(utf8("Tasto già assegnato a ") + library.current().pads[found->second].name + ". Liberalo prima.", true); return; }
    keyIndex.erase(pad->key);
    pad->key = key; pad->keyLabel = keyLabel(key); binding = false;
    keyIndex[key] = selection;
    lastPreloadPlan.clear(); preloadProfile({});
    save(); selectPad(selection); setStatus("Tasto associato.");
}
void MainComponent::setStatus(const juce::String& text, bool error)
{
    statusMessage = text; statusIsError = error;
    displayedPreloadRemaining = preloadRemaining.load();
    const auto displayed = text + (displayedPreloadRemaining > 0
        ? "  /  Preparazione suoni: " + juce::String(displayedPreloadRemaining) : juce::String());
    semanticTextColour(statusLabel, error ? uiColours::warning : uiColours::muted);
    statusLabel.setText(displayed, juce::dontSendNotification); statusLabel.setTooltip(displayed);
}
void MainComponent::refreshDevices()
{
    const auto names = audio.main.deviceNames();
    const auto inputs = audio.main.inputDeviceNames();
    outputDevice.clear(juce::dontSendNotification); monitorDevice.clear(juce::dontSendNotification);
    inputDevice.clear(juce::dontSendNotification);
    outputDevice.addItem("Predefinita di Windows", 1); outputDevice.addItemList(names, 2); monitorDevice.addItemList(names, 1);
    const auto mainIndex = names.indexOf(library.outputDevice);
    outputDevice.setSelectedId(library.outputDevice.isEmpty() ? 1 : mainIndex < 0 ? 0 : mainIndex + 2, juce::dontSendNotification);
    if (library.outputDevice.isNotEmpty() && mainIndex < 0)
        outputDevice.setTextWhenNothingSelected("Uscita salvata non disponibile: " + library.outputDevice);
    const auto monitorIndex = names.indexOf(library.monitorDevice);
    monitorDevice.setSelectedId(monitorIndex < 0 ? 0 : monitorIndex + 1, juce::dontSendNotification);
    monitorDevice.setTextWhenNothingSelected(library.monitorDevice.isNotEmpty() && monitorIndex < 0
        ? "Cuffie salvate non disponibili: " + library.monitorDevice : "Scegli le cuffie");
    inputDevice.addItemList(inputs, 1);
    const auto inputIndex = inputs.indexOf(library.inputDevice);
    inputDevice.setSelectedId(inputIndex < 0 ? 0 : inputIndex + 1, juce::dontSendNotification);
    inputDevice.setTextWhenNothingSelected(library.inputDevice.isNotEmpty() && inputIndex < 0
        ? "Microfono salvato non disponibile: " + library.inputDevice : "Scegli il microfono fisico");
}
void MainComponent::refreshKeyboards()
{
    rawKeyboard.enable(false); rawKeyboard.identify(false);
    heldKeys.clear();
    keyboardDevices = RawKeyboard::devices();
    keyboardDevice.clear(juce::dontSendNotification);
    int selected = 0;
    for (std::size_t i = 0; i < keyboardDevices.size(); ++i)
    {
        keyboardDevice.addItem(keyboardDevices[i].name, static_cast<int>(i) + 1);
        if (keyboardDevices[i].id == library.keyboardDevice) selected = static_cast<int>(i) + 1;
    }
    keyboardDevice.setSelectedId(selected, juce::dontSendNotification);
    keyboardDevice.setTextWhenNothingSelected(library.keyboardDevice.isEmpty() ? "Scegli la tastiera dedicata" : "Tastiera salvata non collegata");
    rawKeyboard.select(library.keyboardDevice);
    captureKeyboard.setToggleState(false, juce::dontSendNotification);
    updateKeyboardStatus();
}
void MainComponent::updateKeyboardStatus()
{
    const bool active = rawKeyboard.enabled();
    const bool pending = rawKeyboard.activating();
    const bool selected = keyboardDevice.getSelectedId() > 0;
    identifyKeyboard.setButtonText(rawKeyboard.identifying() ? "Annulla riconoscimento" : "Riconosci con F12");
    captureKeyboard.setEnabled(rawKeyboardReady && selected);
    inputStatus.setText(active ? "Tastiera dedicata attiva" : pending ? "Collegamento tastiera..." : "Tasti locali\nSolo in primo piano", juce::dontSendNotification);
    stopButton.setTooltip("Ferma tutti i suoni. Scorciatoia: Esc.");
    djButton.setTooltip("Tieni premuto per mettere in pausa; rilascia per riprendere. Scorciatoia: " + keyLabel(library.djStopKey) + ".");
    semanticTextColour(inputStatus, active ? uiColours::accent : uiColours::muted);
    keyboardStatus.setText(active ? "Attiva: i tasti selezionati controllano DualDeck e sono bloccati nelle altre app."
        : pending ? "Collegamento in corso. Mantieni rilasciati i tasti; puoi annullare con l'interruttore."
        : rawKeyboard.error().isNotEmpty() ? rawKeyboard.error()
        : selected ? "Tastiera selezionata. Rilascia tutti i tasti e attivala per bloccarli nelle altre app."
        : "Scegli una tastiera oppure usa Riconosci con F12.", juce::dontSendNotification);
    semanticTextColour(keyboardStatus, active ? uiColours::accent : rawKeyboard.error().isNotEmpty() ? uiColours::warning : uiColours::muted);
}
void MainComponent::replaceAudioFile()
{
    const auto* pad = selectedPad(); if (!pad) return;
    const auto padId = pad->id, profileId = library.current().id;
    chooser = std::make_unique<juce::FileChooser>("Ricollega il file del suono", projectRoot.getChildFile("sounds"), "*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aac;*.wma;*.aif;*.aiff");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<MainComponent>(this), padId, profileId](const juce::FileChooser& choice)
        {
            if (!safe || !choice.getResult().existsAsFile()) return;
            for (auto& profile : safe->library.profiles) if (profile.id == profileId)
                for (auto& target : profile.pads) if (target.id == padId)
                {
                    safe->stopAll(); target.file = choice.getResult(); target.refreshFileInfo(); safe->refresh(); safe->save();
                    safe->setStatus("File ricollegato. Nome, tasto e impostazioni del suono conservati."); return;
                }
        });
}
void MainComponent::openPadEditor()
{
    hideAppearanceEditors();
    const auto* pad = selectedPad(); if (!pad) return;
    stopEditorPreview();
    editingPadId = pad->id; editingProfileId = library.current().id;
    editingClipDuration = 0.0;
    editorPanel.setPad(pad->name, pad->settings,
        (pad->useGlobalMode ? library.defaultMode : pad->settings.mode) == PlayMode::toggleLoop);
    editorViewport.setViewPosition(0, 0);
    editorViewport.setVisible(true); editorViewport.toFront(true); editorPanel.grabKeyboardFocus();
    if (pad->fileMissing) { editorPanel.setMessage("File mancante: ricollegalo per visualizzare la forma d'onda."); return; }
    const auto id = pad->id, profileId = library.current().id;
    const auto file = pad->file;
    const auto revision = juce::String(pad->modifiedMs) + ":" + juce::String(pad->fileSizeBytes);
    juce::Component::SafePointer<MainComponent> safe(this);
    waveformLoader.addJob(new LoadJob([this, safe, id, profileId, file, revision]
    {
        const ClipData* clip = nullptr;
        const auto result = audio.loadClip(file, clip, revision, true);
        std::vector<float> peaks;
        double duration = 0;
        if (result.wasOk() && clip)
        {
            duration = clip->duration();
            const auto frames = static_cast<std::size_t>(clip->frames());
            const auto bins = std::min<std::size_t>(32768, frames);
            peaks.resize(bins);
            for (std::size_t bin = 0; bin < bins; ++bin)
            {
                const auto begin = bin * frames / bins;
                const auto end = (bin + 1) * frames / bins;
                float peak = 0;
                for (auto frame = begin; frame < end; ++frame)
                    peak = juce::jmax(peak, juce::jmax(std::abs(clip->samples[frame * 2]), std::abs(clip->samples[frame * 2 + 1])));
                peaks[bin] = peak;
            }
            audio.releaseReservation(file, revision);
        }
        juce::MessageManager::callAsync([safe, id, profileId, result, duration, peaks = std::move(peaks)]() mutable
        {
            if (!safe || safe->editingPadId != id || safe->editingProfileId != profileId) return;
            if (result.failed()) safe->editorPanel.setMessage(result.getErrorMessage());
            else
            {
                safe->editingClipDuration = duration;
                safe->editorPanel.setWaveform(std::move(peaks), duration);
            }
        });
    }), true);
}
void MainComponent::stopEditorPreview()
{
    editorPanel.resetPreview();
    editorPlaybackRequested = false;
    editorPlaybackSeen = false;
    editorLoopPreviewRequested = false;
    editorScrubbing = false;
    editorScrubVoiceActive = false;
    audio.main.core.trackVoice(0);
    if (!audio.main.isOpen()) return;
    audio.release(editorPlayId);
    audio.release(scrubPreviewId);
    audio.release(loopPreviewId);
}
bool MainComponent::previewEditorLoop(bool enabled, double start, double end)
{
    if (!enabled)
    {
        editorLoopPreviewRequested = false;
        if (!editorPlaybackRequested) audio.main.core.trackVoice(0);
        if (audio.main.isOpen()) audio.release(loopPreviewId);
        return true;
    }
    if (editorPlaybackRequested)
    {
        editorPlaybackRequested = false;
        if (audio.main.isOpen()) audio.release(editorPlayId);
    }
    const auto* profile = library.findProfile(editingProfileId);
    if (!profile || editingClipDuration <= 0) return false;
    for (const auto& pad : profile->pads) if (pad.id == editingPadId && !pad.fileMissing)
    {
        auto settings = editorPanel.currentSettings();
        settings.mode = PlayMode::holdLoop;
        settings.trimStart = start;
        settings.trimEnd = end;
        settings.fadeIn = 0.005;
        settings.fadeOut = 0.02;
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        audio.main.core.trackVoice(loopPreviewId);
        editorLoopPreviewRequested = audio.playCached(loopPreviewId, pad.file, revision, settings) == AudioEngine::PlayResult::queued;
        if (editorLoopPreviewRequested)
        {
            editorPanel.setPlaybackPosition(start);
        }
        else audio.main.core.trackVoice(0);
        return editorLoopPreviewRequested;
    }
    return false;
}
bool MainComponent::previewEditorPlay(double seconds)
{
    const auto* profile = library.findProfile(editingProfileId);
    if (!profile || editingClipDuration <= 0) { editorPlaybackRequested = false; return false; }
    for (const auto& pad : profile->pads) if (pad.id == editingPadId && !pad.fileMissing)
    {
        auto settings = editorPanel.currentSettings();
        settings.mode = PlayMode::restart;
        settings.trimStart = std::clamp(seconds, 0.0, std::max(0.0, editingClipDuration - 0.005));
        settings.trimEnd = 0.0;
        settings.fadeIn = 0.008;
        settings.fadeOut = 0.02;
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        audio.main.core.trackVoice(editorPlayId);
        editorPlaybackRequested = audio.playCached(editorPlayId, pad.file, revision, settings) == AudioEngine::PlayResult::queued;
        editorPlaybackSeen = false;
        if (editorPlaybackRequested)
        {
            editorPanel.setPlaybackPosition(settings.trimStart);
        }
        else audio.main.core.trackVoice(0);
        return editorPlaybackRequested;
    }
    editorPlaybackRequested = false;
    return false;
}
void MainComponent::beginEditorScrub()
{
    editorScrubbing = true;
    editorScrubVoiceActive = false;
    audio.main.core.trackVoice(0);
    if (editorPlaybackRequested && audio.main.isOpen()) audio.release(editorPlayId);
}
void MainComponent::endEditorScrub(double seconds)
{
    editorScrubbing = false;
    if (editorScrubVoiceActive && audio.main.isOpen()) audio.release(scrubPreviewId);
    editorScrubVoiceActive = false;
    if (!editorPlaybackRequested) return;
    if (!previewEditorPlay(seconds))
        editorPanel.setMessage("Riproduzione non disponibile: controlla file e uscita audio.");
}
void MainComponent::previewEditorScrub(double seconds)
{
    audio.setScrubTarget(std::clamp(seconds, 0.0, editingClipDuration));
    if (editorScrubVoiceActive) return;
    const auto* profile = library.findProfile(editingProfileId);
    if (!profile || editingClipDuration <= 0) return;
    for (const auto& pad : profile->pads) if (pad.id == editingPadId && !pad.fileMissing)
    {
        auto settings = editorPanel.currentSettings();
        settings.mode = PlayMode::scrub;
        settings.trimStart = settings.trimEnd = 0.0;
        settings.fadeIn = seconds < 0.003 ? 0.0 : 0.001;
        settings.fadeOut = 0.02;
        const auto revision = juce::String(pad.modifiedMs) + ":" + juce::String(pad.fileSizeBytes);
        editorScrubVoiceActive = audio.playCached(scrubPreviewId, pad.file, revision, settings)
            == AudioEngine::PlayResult::queued;
        return;
    }
}
bool MainComponent::applyAudio()
{
    if (smokeTest) return true;
    if (outputDevice.getSelectedId() == 0)
    {
        setStatus("Uscita principale salvata non disponibile. Scegli un dispositivo prima di applicare.", true);
        return false;
    }
    if (monitorToggle.getToggleState() && monitorDevice.getSelectedId() == 0)
    {
        setStatus("Cuffie non disponibili. Scegli un dispositivo oppure disattiva l'ascolto aggiuntivo.", true);
        return false;
    }
    if (microphoneToggle.getToggleState() && inputDevice.getSelectedId() == 0)
    {
        setStatus("Microfono non disponibile. Scegli un ingresso prima di applicare.", true);
        return false;
    }
    const auto outputName = outputDevice.getSelectedId() == 1 ? juce::String() : outputDevice.getText();
    const auto monitorName = monitorDevice.getSelectedId() > 0 ? monitorDevice.getText() : juce::String();
    const auto monitorEnabled = monitorToggle.getToggleState();
    const auto inputName = inputDevice.getSelectedId() > 0 ? inputDevice.getText() : juce::String();
    const auto microphoneEnabled = microphoneToggle.getToggleState();
    stopAll();
    updateAudioControls();
    audio.setMicrophoneGain(static_cast<float>(microphoneVolume.getValue() / 100.0));
    audio.setMicrophoneMonitor(microphoneMonitorToggle.getToggleState() && monitorEnabled);
    const auto result = audio.configure(outputName, monitorName, monitorEnabled, inputName, microphoneEnabled);
    outputDevice.setTooltip(audio.main.details());
    monitorDevice.setTooltip(monitorEnabled && !audio.hasMonitor() && result.wasOk() ? "Stesso dispositivo: una sola copia, volume principale." : audio.monitor.details());
    setStatus(result.wasOk() ? "Uscite audio pronte." : result.getErrorMessage(), result.failed());
    if (result.failed()) return false;
    library.outputDevice = outputName;
    library.monitorDevice = monitorName;
    library.monitorEnabled = monitorEnabled;
    library.inputDevice = inputName;
    library.microphoneEnabled = microphoneEnabled;
    library.microphoneMonitor = microphoneMonitorToggle.getToggleState();
    library.microphoneVolume = static_cast<float>(microphoneVolume.getValue() / 100.0);
    return true;
}
void MainComponent::timerCallback()
{
    if (editorViewport.isVisible() && !editorScrubbing)
    {
        const auto tracked = editorPlaybackRequested ? editorPlayId : editorLoopPreviewRequested ? loopPreviewId : 0;
        if (tracked != 0)
        {
            const auto position = audio.main.core.trackedPositionSeconds(tracked);
            if (position >= 0)
            {
                editorPanel.setPlaybackPosition(position);
                if (tracked == editorPlayId) editorPlaybackSeen = true;
            }
            else if (tracked == editorPlayId && (editorPlaybackSeen || audio.main.core.trackedVoiceStarted()))
            {
                editorPanel.setPlaybackPosition(editingClipDuration);
                editorPlaybackRequested = false;
                editorPlaybackSeen = false;
                audio.main.core.trackVoice(0);
            }
        }
    }
    keyStateChanged(false, this);
    if (preloadRemaining.load() != displayedPreloadRemaining) setStatus(statusMessage, statusIsError);
    if (searchPending && juce::Time::getMillisecondCounter() - searchChangedAt >= 150)
    { searchPending = false; rebuildTiles(); }
    if (dirty && ++dirtyTicks >= 20) { dirtyTicks = 0; save(); }
    const auto error = audio.main.takeError(); const auto monitorError = audio.monitor.takeError();
    const auto streamError = audio.takeStreamingError();
    if (streamError.isNotEmpty()) lastPreloadPlan.clear();
    if (error.isNotEmpty() || monitorError.isNotEmpty() || streamError.isNotEmpty())
    { stopAll(); setStatus("Audio: " + error + " " + monitorError + " " + streamError, true); }
    repaint(getWidth() - 220, getHeight() - 66, 212, 60);
#if defined(DUALDECK_FOLEYS_AUDIO_UI)
    outputMeter.repaint();
#endif
}
void MainComponent::chooseAudioFolder()
{
    const auto profileId = library.current().id;
    chooser = std::make_unique<juce::FileChooser>("Collega una cartella di file audio", library.current().folders.empty() ? projectRoot : library.current().folders.back(), "*");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe = juce::Component::SafePointer<MainComponent>(this), profileId](const juce::FileChooser& choice)
        { if (safe && choice.getResult().isDirectory()) safe->scanProfileFolders(profileId, {choice.getResult()}); });
}
void MainComponent::scanProfileFolders(const juce::String& profileId, const std::vector<juce::File>& extraFolders)
{
    if (smokeTest) return;
    if (scanningFolders) { setStatus("Scansione in corso. Attendi oppure annullala prima di collegare un'altra cartella."); return; }
    const auto* profile = library.findProfile(profileId); if (!profile) return;
    auto folders = profile->folders;
    folders.insert(folders.end(), extraFolders.begin(), extraFolders.end());
    std::vector<juce::File> files;
    for (const auto& pad : profile->pads) files.push_back(pad.file);
    scanningFolders = true; scanningProfile = profileId; scanCancelled = std::make_shared<std::atomic<bool>>(false);
    importButton.setEnabled(false); refreshFolders.setEnabled(true); refreshFolders.setButtonText("Annulla scansione");
    setStatus("Lettura delle cartelle in corso...");
    const auto cancelled = scanCancelled;
    const bool requestedFolders = !extraFolders.empty();
    juce::Component::SafePointer<MainComponent> safe(this);
    folderLoader.addJob(new FolderJob([safe, profileId, folders, files, cancelled, requestedFolders](juce::ThreadPoolJob& job)
    {
        auto result = scanAudioFolders(folders, files, [&] { return cancelled->load() || job.shouldExit(); });
        juce::MessageManager::callAsync([safe, profileId, requestedFolders, cancelled, result = std::move(result)]() mutable
        {
            if (!safe) return;
            if (cancelled->load()) result.cancelled = true;
            safe->scanningFolders = false; safe->scanningProfile.clear();
            safe->importButton.setEnabled(true); safe->refreshFolders.setEnabled(true); safe->refreshFolders.setButtonText("Aggiorna cartelle");
            if (result.cancelled) safe->setStatus("Scansione annullata. Nessuna modifica applicata.");
            else if (safe->library.findProfile(profileId))
            {
                const int added = safe->library.mergeFolderScan(profileId, result);
                safe->refresh();
                safe->setStatus(juce::String(added) + " nuovi suoni collegati. "
                    + juce::String(safe->missingFiles) + " file mancanti nel profilo attivo; pad e impostazioni conservati."
                    + (result.warnings.isEmpty() ? "" : " " + result.warnings.joinIntoString(" / ")), !result.warnings.isEmpty());
                if (added > 0 || requestedFolders) safe->save();
            }
            if (safe->library.current().id != profileId && !safe->library.current().folders.empty())
                safe->scanProfileFolders(safe->library.current().id);
        });
    }), true);
}
void MainComponent::updateAudioControls()
{
    audio.main.core.setVolume(library.outputVolume); audio.monitor.core.setVolume(library.monitorVolume);
    for (auto* core : {&audio.main.core, &audio.monitor.core})
    {
        core->setPitch(library.globalPitchEnabled ? library.globalPitch : 0.0);
    }
    outputVolume.setEnabled(true); monitorVolume.setEnabled(true);
    globalPitch.setEnabled(library.globalPitchEnabled);
    const auto* pad = selectedPad();
    padVolume.setEnabled(pad != nullptr && pad->settings.overrideMasterVolume);
    padVolume.setTooltip(pad && pad->settings.overrideMasterVolume ? "Sostituisce i master di entrambe le uscite." : "Attiva 'Usa il volume del suono' per regolare questo pad.");
}
void MainComponent::showPadMenu(int index)
{
    selectPad(index);
    const auto* pad = selectedPad(); if (!pad) return;
    const auto profileId = library.current().id, padId = pad->id;
    juce::PopupMenu menu;
    menu.addItem(1, "Modifica nome e impostazioni");
    menu.addItem(2, pad->favorite ? "Rimuovi dai preferiti" : "Aggiungi ai preferiti");
    menu.addSeparator();
    menu.addItem(3, "Associa un tasto");
    menu.addItem(4, "Libera il tasto", pad->key.isNotEmpty());
    menu.addItem(5, pad->fileMissing ? "Ricollega file audio..." : "Cambia file audio...");
    menu.addItem(6, "Audio ed effetti...");
    menu.addSeparator();
    menu.addItem(7, "Rimuovi pad dalla libreria...", writable);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
        [safe = juce::Component::SafePointer<MainComponent>(this), profileId, padId](int action)
        {
            if (!safe || action == 0 || safe->library.current().id != profileId) return;
            auto& pads = safe->library.current().pads;
            const auto found = std::find_if(pads.begin(), pads.end(), [&](const Pad& p) { return p.id == padId; });
            if (found == pads.end()) return;
            safe->selectPad(static_cast<int>(found - pads.begin()));
            if (action == 1) { safe->padName.grabKeyboardFocus(); safe->padName.selectAll(); }
            else if (action == 2) { found->favorite = !found->favorite; safe->save(); safe->selectPad(safe->selection); }
            else if (action == 3) safe->bindButton.onClick();
            else if (action == 4) safe->unbindButton.onClick();
            else if (action == 5) safe->replaceAudioFile();
            else if (action == 6) safe->openPadEditor();
            else if (action == 7 && safe->writable)
            {
                juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Rimuovere il pad?",
                    "Il pad verrà rimosso dalla libreria e non verrà reimportato dalle cartelle collegate.\nIl file audio resterà sul disco.",
                    "Rimuovi", "Annulla", safe.getComponent(),
                    juce::ModalCallbackFunction::create([safe, profileId, padId](int answer)
                    {
                        if (!safe || answer != 1 || safe->library.current().id != profileId) return;
                        auto& pads = safe->library.current().pads;
                        const auto found = std::find_if(pads.begin(), pads.end(), [&](const Pad& item) { return item.id == padId; });
                        if (found == pads.end()) return;
                        const auto removedIndex = static_cast<int>(found - pads.begin());
                        safe->stopAll();
                        if (!safe->library.removePad(profileId, padId)) return;
                        safe->selection = safe->library.current().pads.empty() ? -1
                            : juce::jmin(removedIndex, static_cast<int>(safe->library.current().pads.size()) - 1);
                        safe->page = 0;
                        safe->refresh();
                        safe->save();
                        safe->setStatus("Pad rimosso dalla libreria. Il file audio resta sul disco.");
                    }));
            }
        });
}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files) if (juce::File(path).isDirectory()) return true;
    return false;
}
void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    std::vector<juce::File> folders;
    for (const auto& path : files) if (juce::File(path).isDirectory()) folders.emplace_back(path);
    if (!folders.empty()) scanProfileFolders(library.current().id, folders);
}
void MainComponent::prepareSnapshot(int section)
{
    keyboardMode = section == 2; setView(section == 3 ? 2 : section == 1 ? 1 : 0);
    if (section == 4 || section == 5)
    {
        openPadEditor();
        if (section == 5) editorViewport.setViewPosition(0, editorPanel.getHeight());
    }
}
}
