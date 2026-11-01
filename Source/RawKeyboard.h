#pragma once
#include <juce_events/juce_events.h>
#include <cstdint>
#include "KeyboardDeviceIdentity.h"

namespace dualdeck
{
struct KeyboardDevice { juce::String id, name; std::uintptr_t handle = 0; };
class RawKeyboard
{
public:
    // All calls and callbacks belong to the JUCE message thread. Capture state
    // changes require the caller to stop pads started by the previous session.
    RawKeyboard();
    ~RawKeyboard();
    static std::vector<KeyboardDevice> devices();
    juce::Result initialise();
    juce::Result select(const juce::String&);
    void enable(bool);
    bool enabled() const;
    bool activating() const;
    juce::String error() const;
    bool identifying() const;
    void identify(bool);
    std::function<void(const juce::String&, bool)> onKey;
    // Physical foreground keys; caller owns focus/text-entry policy. Never used
    // as a fallback for dedicated capture or delivered during its activation.
    std::function<void(const juce::String&, bool)> onLocalKey;
    std::function<void(const juce::String&)> onIdentify;
    std::function<void()> onDevicesChanged;
    std::function<void()> onCaptureStateChanged;
private:
    struct Impl;
    std::shared_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE(RawKeyboard)
};
}
