#pragma once
#include <cstdint>
#include <set>

namespace dualdeck
{
// State lives exclusively on the UI thread. No hooks and no synthetic key events.
class KeyboardFilter
{
public:
    enum Event { ignored, pressed, released };
    void select(std::uintptr_t value) { device = value; held.clear(); enabled = false; }
    void enable(bool value) { enabled = value && device != 0; }
    bool isEnabled() const { return enabled; }
    Event input(std::uintptr_t source, unsigned code, bool down)
    {
        if (source == 0 || source != device) return ignored;
        if (down) return held.insert(code).second && enabled ? pressed : ignored;
        return held.erase(code) != 0 && enabled ? released : ignored;
    }
private:
    std::uintptr_t device = 0;
    bool enabled = false;
    std::set<unsigned> held;
};
}
