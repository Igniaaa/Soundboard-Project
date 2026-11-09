#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace dualdeck
{
namespace uiColours
{
enum { background = 0x2dde000, panel, tile, line, text, muted, accent, blue, warning };
}
struct UiTheme
{
    std::array<juce::Colour, 9> colours {{juce::Colour(0xff0b111b), juce::Colour(0xff121c29), juce::Colour(0xff182536),
        juce::Colour(0xff28384b), juce::Colour(0xffedf3f9), juce::Colour(0xff8c9eb3), juce::Colour(0xff63dfc4),
        juce::Colour(0xff7aa8ff), juce::Colour(0xffffc579)}};
    float fontScale = 1.0f, cornerRadius = 10.0f;
};
struct UiThemeProvider
{
    virtual ~UiThemeProvider() = default;
    virtual const UiTheme& getUiTheme() const = 0;
};
inline juce::Colour uiColour(const juce::Component& component, int id)
{
    return component.findColour(id, true);
}
inline float uiFontScale(const juce::Component& component)
{
    auto* provider = dynamic_cast<const UiThemeProvider*>(&component.getLookAndFeel());
    float local = 1.0f;
    for (auto* c = &component; c; c = c->getParentComponent())
        if (c->getProperties().contains("appearance-font-scale")) { local = static_cast<float>(c->getProperties()["appearance-font-scale"]); break; }
    return juce::jlimit(0.6f, 2.0f, (provider ? provider->getUiTheme().fontScale : 1.0f) * local);
}
inline float uiFontHeight(const juce::Component& component, float fallback)
{
    for (auto* c = &component; c; c = c->getParentComponent())
        if (c->getProperties().contains("appearance-font-size"))
        {
            const auto size = static_cast<float>(c->getProperties()["appearance-font-size"]);
            return size > 0.0f ? size : fallback;
        }
    return fallback;
}
inline juce::Font uiFont(const juce::Component& component, float size, bool bold = false)
{
    const auto requestedScale = uiFontHeight(component, 14.0f) / 14.0f;
    return juce::Font(juce::FontOptions(size * requestedScale * uiFontScale(component), bold ? juce::Font::bold : juce::Font::plain));
}
inline float uiRadius(const juce::Component& component, float radius)
{
    auto* provider = dynamic_cast<const UiThemeProvider*>(&component.getLookAndFeel());
    return radius * (provider ? provider->getUiTheme().cornerRadius * 0.1f : 1.0f);
}
}
