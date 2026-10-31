#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dualdeck
{
// OFFLINE policy only: these are capture DECISIONS, not physical key blocking.
// Windows suppression requires a future real driver/broker. The caller MUST
// release its driver filter whenever this policy disarms, including on poll().
// One owner/thread must serialize every call and poll regularly even when idle.
// Device tokens and uniqueness, backend health, trusted released confirmation,
// timestamps (monotonic milliseconds), and delivery capacity come from caller.
class KeyboardBlockSession
{
public:
    using Device = std::uintptr_t;
    enum Event { none, pressed, released };
    struct Decision { bool suppress = false; Event event = none; };
    KeyboardBlockSession() = default;
    KeyboardBlockSession(const KeyboardBlockSession&) = delete;
    KeyboardBlockSession& operator=(const KeyboardBlockSession&) = delete;
    KeyboardBlockSession(KeyboardBlockSession&&) = delete;
    KeyboardBlockSession& operator=(KeyboardBlockSession&&) = delete;

    void setBackendAvailable(bool available)
    {
        if (backend == available) return;
        disarm();
        backend = available;
        if (available) resetKeys();
    }
    // Re-selection, even of the same token, starts a new device-state epoch.
    void select(Device token, std::size_t matchingDevices = 1)
    {
        disarm();
        device = token;
        unique = token != 0 && matchingDevices == 1;
        resetKeys();
    }
    void disconnect(Device token)
    {
        if (token != 0 && token == device) select(0, 0);
    }
    bool confirmAllKeysReleased()
    {
        releasedConfirmed = !anyHeld();
        return releasedConfirmed;
    }
    bool arm(std::uint64_t now)
    {
        if (armed) { poll(now); return armed; }
        if (!backend || !unique || !releasedConfirmed || anyHeld() || stopPending) return false;
        armed = true;
        releasedConfirmed = false;
        lastHeartbeat = lastObserved = now;
        return true;
    }
    bool isArmed() const { return armed; }
    void disable() { disarm(); }
    void poll(std::uint64_t now)
    {
        if (!armed) return;
        if (now < lastObserved || now - lastHeartbeat >= 1500) disarm();
        else lastObserved = now;
    }
    bool heartbeat(std::uint64_t now)
    {
        poll(now);
        if (!armed) return false;
        lastHeartbeat = now;
        return true;
    }
    bool stopPadsRequested() const { return stopPending; }
    // Acknowledge only AFTER the consumer has stopped the session's pads.
    void acknowledgeStopPads() { stopPending = false; }

    // queueAvailable promises delivery capacity for the returned event. Repeat
    // and unmatched-release suppression need no delivery slot. No I/O occurs.
    Decision input(Device source, unsigned scan, unsigned prefix, bool down,
                   std::uint64_t now, bool queueAvailable = true)
    {
        poll(now);
        if (source == 0 || source != device) return {};
        // 0xFF is KEYBOARD_OVERRUN_MAKE_CODE, not a physical key.
        if (scan == 0 || scan >= 255 || prefix > 2)
        {
            disarm();
            return {};
        }
        auto& key = held[prefix * 256 + scan];
        const Event event = key == down ? none : (down ? pressed : released);
        key = down;
        if (!armed) return {};
        if (event != none && !queueAvailable)
        {
            disarm();
            return {};
        }
        return {true, event};
    }

private:
    bool anyHeld() const
    {
        for (bool key : held) if (key) return true;
        return false;
    }
    void resetKeys()
    {
        held.fill(false);
        releasedConfirmed = false;
    }
    void disarm()
    {
        if (armed) stopPending = true;
        armed = false;
        releasedConfirmed = false;
    }
    std::array<bool, 3 * 256> held{};
    Device device = 0;
    std::uint64_t lastHeartbeat = 0, lastObserved = 0;
    bool backend = false, unique = false, releasedConfirmed = false;
    bool armed = false, stopPending = false;
};
}
