#pragma once
#include <juce_events/juce_events.h>
#include <cstdint>
#include <memory>

namespace dualdeck
{
// All client calls/callbacks belong to the message thread. The child process
// owns the driver context, applies only one keyboard filter, and releases it
// on stop, parent exit, or a missing GUI heartbeat.
class KeyboardBridge
{
public:
    KeyboardBridge();
    ~KeyboardBridge();
    juce::Result start(const juce::String& exactHardwareId);
    void stop();
    void poll();
    bool active() const;
    bool starting() const;
    juce::String error() const;
    static juce::File runtimeLibrary();
    std::function<void(const juce::String&, bool)> onKey;
    std::function<void()> onStateChanged;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE(KeyboardBridge)
};

int runKeyboardBroker(std::uintptr_t mappingHandle, std::uintptr_t parentHandle,
                      const juce::File& library);
// Passive probe: inspect driver keyboard slots through the official API.
// Never enable filters, receive keys, or inject input.
juce::Result writeKeyboardDriverStatus(const juce::File& library, const juce::File& report);
}
