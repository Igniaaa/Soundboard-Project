#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstdint>
#include <string>
#include "KeyboardBlockSession.h"
#include "KeyboardScan.h"
namespace dualdeck::keyboardbridge
{
constexpr unsigned capacity = 256;
constexpr std::uint64_t timeoutMs = 1500;
constexpr std::uint32_t magic = 0x44444b42, version = 1;
enum State : LONG { pending, running, stopping, stopped, failed };
struct Packet { unsigned scan = 0, prefix = 0, down = 0; };
// Anonymous mapping: one producer (broker), one consumer (message thread).
// All publication uses Windows interlocked operations, including 64-bit clocks.
struct alignas(8) Shared
{
    std::uint32_t signature = magic, protocol = version;
    volatile LONG state = pending, stop = 0;
    alignas(8) volatile LONG64 uiHeartbeat = 0, brokerHeartbeat = 0;
    volatile LONG read = 0, write = 0;
    wchar_t hardwareId[1024]{};
    wchar_t error[512]{};
    Packet packets[capacity]{};
};
inline LONG read32(volatile LONG& value) { return InterlockedCompareExchange(&value, 0, 0); }
inline LONG64 read64(volatile LONG64& value) { return InterlockedCompareExchange64(&value, 0, 0); }
inline std::uint32_t queued(Shared& value) { return static_cast<std::uint32_t>(read32(value.write)) - static_cast<std::uint32_t>(read32(value.read)); }
inline bool room(Shared& value) { return queued(value) < capacity; }
inline bool push(Shared& value, Packet packet)
{
    if (!room(value)) return false;
    const auto position = static_cast<std::uint32_t>(read32(value.write));
    value.packets[position % capacity] = packet;
    InterlockedExchange(&value.write, static_cast<LONG>(position + 1));
    return true;
}
inline bool pop(Shared& value, Packet& packet)
{
    const auto size = queued(value);
    if (size == 0 || size > capacity) return false;
    const auto position = static_cast<std::uint32_t>(read32(value.read));
    packet = value.packets[position % capacity];
    InterlockedExchange(&value.read, static_cast<LONG>(position + 1));
    return true;
}
inline void discard(Shared& value) { InterlockedExchange(&value.read, read32(value.write)); }
inline bool popForDelivery(Shared& value, Packet& packet)
{
    return read32(value.state) == running && pop(value, packet) && read32(value.state) == running;
}
inline bool expired(std::uint64_t now, std::uint64_t heartbeat) { return now < heartbeat || now - heartbeat >= timeoutMs; }
struct HeartbeatSample
{
    std::uint64_t heartbeat, time;
    bool isExpired() const { return expired(time, heartbeat); }
};
template<class Reader, class Clock> HeartbeatSample sampleHeartbeat(Reader readHeartbeat, Clock clock)
{
    const auto heartbeat = readHeartbeat();
    const auto time = clock();
    return {heartbeat, time};
}
inline HeartbeatSample sampleHeartbeat(volatile LONG64& heartbeat)
{
    return sampleHeartbeat([&heartbeat] { return static_cast<std::uint64_t>(read64(heartbeat)); },
                           [] { return static_cast<std::uint64_t>(GetTickCount64()); });
}
inline bool valid(Packet packet) { return packet.scan > 0 && packet.scan < 255 && packet.prefix < 3 && packet.down < 2; }
inline bool decode(unsigned scan, unsigned state, Packet& packet)
{
    const unsigned extended = state & 6u;
    if ((state & ~7u) != 0 || extended == 6) return false;
    packet = {scan, extended == 2 ? 1u : (extended == 4 ? 2u : 0u), (state & 1u) == 0 ? 1u : 0u};
    return valid(packet);
}
// Deliver physical keys after normalising multi-packet keys. Pulse releases are
// internal pad events only; no keyboard input is synthesised for Windows.
inline bool deliverPhysicalKey(Shared& shared, KeyboardBlockSession& policy, KeyboardScanSequence& sequence,
                               std::uintptr_t device, Packet raw, std::uint64_t time)
{
    if (!valid(raw)) return false;
    const auto key = sequence.input(raw.scan, raw.prefix == 1 ? 2u : raw.prefix == 2 ? 4u : 0u, raw.down != 0);
    if (key.scan == 0) return true;
    Packet packet{key.scan, key.flags == 2 ? 1u : key.flags == 4 ? 2u : 0u, key.down ? 1u : 0u};
    const auto deliver = [&]()
    {
        const auto decision = policy.input(device, packet.scan, packet.prefix, packet.down != 0, time, room(shared));
        return decision.suppress && (decision.event == KeyboardBlockSession::none || push(shared, packet));
    };
    if (!deliver()) return false;
    if (key.pulse) { packet.down = 0; return deliver(); }
    return true;
}
inline bool exactId(const std::wstring& left, const std::wstring& right)
{
    return !left.empty() && left.size() == right.size()
        && CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}
inline int uniqueSlot(const std::array<std::wstring, 10>& ids, const std::wstring& wanted)
{
    int result = 0;
    for (unsigned index = 0; index < ids.size(); ++index)
        if (exactId(ids[index], wanted)) { if (result != 0) return 0; result = static_cast<int>(index + 1); }
    return result;
}
// Revoke delivery before any driver call: cleanup may block while the GUI polls.
template<class Capture> bool retireCapture(Shared& shared, Capture& capture)
{
    InterlockedExchange(&shared.state, stopping);
    return capture.clear();
}
using WireStroke = std::array<unsigned char, 20>;
template<class Backend> int forwardRetiring(Shared& shared, Backend& backend, int device, const WireStroke& stroke)
{
    InterlockedExchange(&shared.state, stopping);
    return backend.send(device, stroke);
}
// The backend owns its context. FilterOwner is independently testable with an
// offline transport; production uses the pinned DLL and acknowledged filter IOCTL.
template<class Backend> struct FilterOwner
{
    Backend& backend;
    int slot;
    bool attempted = false;
    FilterOwner(Backend& value, int selected) : backend(value), slot(selected) {}
    FilterOwner(const FilterOwner&) = delete;
    FilterOwner& operator=(const FilterOwner&) = delete;
    bool closed = false, verified = true;
    bool clear()
    {
        if (closed) return verified;
        closed = true;
        if (attempted)
        {
            verified = backend.clearFilter(slot);
            const auto deadline = GetTickCount64() + 100;
            for (unsigned count = 0; count < capacity && GetTickCount64() < deadline; ++count)
            {
                const auto device = backend.wait(0);
                if (device < 1 || device > 20) break;
                WireStroke stroke{};
                if (backend.receive(device, stroke) != 1) break;
                if (backend.send(device, stroke) != 1) break;
            }
        }
        backend.close();
        return verified;
    }
    ~FilterOwner() { clear(); }
};

}
