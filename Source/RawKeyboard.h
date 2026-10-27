#pragma once
#include <juce_events/juce_events.h>
#include "KeyboardFilter.h"

namespace dualdeck
{
struct KeyboardDevice { juce::String id, name; std::uintptr_t handle = 0; };
class RawKeyboard
{
public:
    RawKeyboard();
    ~RawKeyboard();
    static std::vector<KeyboardDevice> devices();
    juce::Result initialise();
    juce::Result select(const juce::String&);
    void enable(bool);
    bool enabled() const;
    bool identifying() const;
    void identify(bool);
    std::function<void(const juce::String&, bool)> onKey;
    std::function<void(const juce::String&)> onIdentify;
    std::function<void()> onDevicesChanged;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE(RawKeyboard)
};
}
