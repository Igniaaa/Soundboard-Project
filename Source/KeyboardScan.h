#pragma once
#include <cstdint>
#include <map>
#include <set>

namespace dualdeck
{
struct PhysicalKey
{
    unsigned scan = 0, flags = 0;
    bool down = false, pulse = false;
};

// One decoder per physical device; flags use Raw Input's 0 / E0=2 / E1=4.
class KeyboardScanSequence
{
public:
    PhysicalKey input(unsigned scan, unsigned flags, bool down)
    {
        if (scan == 0 || scan >= 255 || (flags != 0 && flags != 2 && flags != 4))
        { pausePrefix = false; return {}; }
        if (pausePrefix)
        {
            pausePrefix = false;
            if (scan == 0x45 && flags == 0)
                return down ? PhysicalKey{0x45, 4, true, true} : PhysicalKey{};
        }
        // Pause may arrive as E1 Ctrl followed by an unextended Num Lock code.
        // It has no dependable physical key-up, so deliver a press/release pulse.
        if (flags == 4 && scan == 0x1d) { pausePrefix = true; return {}; }
        if ((flags == 4 && scan == 0x45) || (flags == 2 && scan == 0x46))
            return down ? PhysicalKey{0x45, 4, true, true} : PhysicalKey{};
        if (flags == 4 || (flags == 2 && (scan == 0x2a || scan == 0x36))) return {};
        if (scan == 0x45 && flags == 2) flags = 0; // Num Lock variants.
        if (scan == 0x54 && flags == 0) { scan = 0x37; flags = 2; } // Alt+Stamp.
        return {scan, flags, down, false};
    }
private:
    bool pausePrefix = false;
};

// Track transitions even while unfocused, preventing autorepeat on focus return.
class PhysicalKeyboardInput
{
public:
    PhysicalKey input(std::uintptr_t device, unsigned scan, unsigned flags, bool down)
    {
        if (device == 0) return {};
        auto& state = devices[device];
        const auto key = state.sequence.input(scan, flags, down);
        if (key.scan == 0 || key.pulse) return key;
        const auto identity = key.scan | (key.flags << 8);
        if (key.down ? !state.held.insert(identity).second : state.held.erase(identity) == 0) return {};
        for (const auto& other : devices)
            if (other.first != device && other.second.held.count(identity) != 0) return {};
        return key;
    }
    void clear() { devices.clear(); }
private:
    struct Device { KeyboardScanSequence sequence; std::set<unsigned> held; };
    std::map<std::uintptr_t, Device> devices;
};
}
