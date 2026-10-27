#pragma once
#include "AudioCore.h"
#include <map>

namespace dualdeck
{
// Records the mode at key-down. The pad's settings may change before key-up.
class PadPressState
{
public:
    void press(int index, PlayMode mode) { modes.try_emplace(index, mode); }
    bool releaseNeedsStop(int index)
    {
        const auto found = modes.find(index);
        if (found == modes.end()) return false;
        const bool stop = found->second == PlayMode::holdLoop;
        modes.erase(found);
        return stop;
    }
    bool isHeld(int index) const { return modes.count(index) != 0; }
    void clear() { modes.clear(); }
private:
    std::map<int, PlayMode> modes;
};
}
