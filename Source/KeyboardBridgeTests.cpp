#include "KeyboardBridgeProtocol.h"
#include "KeyboardFilterChannel.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
#include <functional>
using namespace dualdeck::keyboardbridge;
namespace {
int checks = 0;
void require(bool result, const char* label) { ++checks; if (!result) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); } }
void queueTests()
{
    Shared shared; Packet value;
    require(!pop(shared, value), "empty queue");
    require(room(shared), "empty queue has space");
    for (unsigned i = 0; i < capacity; ++i) require(push(shared, {i + 1, i % 3, i % 2}), "queue accepts exact capacity");
    require(!push(shared, {1, 0, 1}) && !room(shared), "full queue refuses overwrite");
    for (unsigned i = 0; i < capacity; ++i) { require(pop(shared, value), "queue yields each packet"); require(value.scan == i + 1 && value.prefix == i % 3 && value.down == i % 2, "queue preserves ordering and fields"); }
    require(!pop(shared, value), "drained queue empty");
    InterlockedExchange(&shared.read, static_cast<LONG>(0xfffffff0u)); InterlockedExchange(&shared.write, static_cast<LONG>(0xfffffff0u));
    for (unsigned i = 0; i < capacity * 3; ++i) { require(push(shared, {i, 0, 0}), "queue works across sequence wrap"); require(pop(shared, value) && value.scan == i, "wrapped queue keeps ordering"); }
}
void identityTests()
{
    std::array<std::wstring, 10> ids{}; ids[3] = L"HID\\VID_1234&PID_5678&REV_0001&MI_00";
    require(uniqueSlot(ids, ids[3]) == 4, "full ID maps to one keyboard slot");
    require(uniqueSlot(ids, L"hid\\vid_1234&pid_5678&rev_0001&mi_00") == 4, "ID comparison ignores case");
    require(uniqueSlot(ids, L"HID\\VID_1234&PID_5678") == 0, "VID PID prefix never matches");
    require(uniqueSlot(ids, L"") == 0, "empty ID rejected");
    ids[9] = ids[3]; require(uniqueSlot(ids, ids[3]) == 0, "duplicate complete IDs refused");
    ids[9] += L"&COL01"; require(uniqueSlot(ids, ids[3]) == 4, "different collection distinct");
}
void decoderTests()
{
    Packet packet;
    require(decode(30, 0, packet) && packet.scan == 30 && packet.prefix == 0 && packet.down == 1, "ordinary make decoded");
    require(decode(30, 1, packet) && packet.prefix == 0 && packet.down == 0, "ordinary break decoded");
    require(decode(29, 2, packet) && packet.prefix == 1 && packet.down == 1, "E0 make decoded");
    require(decode(29, 3, packet) && packet.prefix == 1 && packet.down == 0, "E0 break decoded");
    require(decode(69, 4, packet) && packet.prefix == 2 && packet.down == 1, "E1 make decoded");
    require(decode(69, 5, packet) && packet.prefix == 2 && packet.down == 0, "E1 break decoded");
    require(!decode(0, 0, packet) && !decode(255, 0, packet) && !decode(256, 0, packet), "invalid scan codes fail open");
    require(!decode(30, 6, packet) && !decode(30, 7, packet), "mutually exclusive E0 E1 flags rejected");
    require(!decode(30, 8, packet) && !decode(30, 16, packet) && !decode(30, 32, packet), "unsupported terminal service flags fail open");
}
void watchdogTests()
{
    require(!expired(1599, 100), "heartbeat valid at 1499ms"); require(expired(1600, 100), "heartbeat expires at 1500ms"); require(expired(99, 100), "backward clock fails open");
    require(valid({30, 0, 1}) && valid({29, 1, 0}) && valid({69, 2, 1}), "normal E0 E1 valid");
    require(!valid({0, 0, 1}) && !valid({255, 0, 0}) && !valid({30, 3, 1}) && !valid({30, 0, 2}), "malformed packets rejected");
    dualdeck::KeyboardBlockSession policy; policy.setBackendAvailable(true); policy.select(4); policy.confirmAllKeysReleased(); require(policy.arm(100), "simulated broker arms");
    require(policy.input(4, 30, 0, true, 100).event == dualdeck::KeyboardBlockSession::pressed, "first press delivered");
    require(policy.input(4, 30, 0, true, 101, false).suppress, "repeat needs no queue entry");
    require(!policy.input(4, 31, 0, true, 102, false).suppress && !policy.isArmed() && policy.stopPadsRequested(), "full queue fails open and stops pads");
    require(!policy.heartbeat(103), "heartbeat never rearms failed session");
}
void heartbeatOrderingTests()
{
    std::uint64_t heartbeat = 100;
    std::vector<int> order;
    const auto concurrent = sampleHeartbeat(
        [&] { order.push_back(1); return heartbeat; },
        [&] { order.push_back(2); heartbeat = 101; return std::uint64_t{100}; });
    require(!concurrent.isExpired(), "concurrent next-tick heartbeat cannot resemble clock rollback");
    require(order == std::vector<int>({1, 2}), "production helper reads heartbeat before sampling local time");
    const auto rollback = sampleHeartbeat([] { return std::uint64_t{101}; }, [] { return std::uint64_t{100}; });
    require(rollback.isExpired(), "actual clock rollback still fails open");
    const auto timely = sampleHeartbeat([] { return std::uint64_t{100}; }, [] { return std::uint64_t{1599}; });
    const auto late = sampleHeartbeat([] { return std::uint64_t{100}; }, [] { return std::uint64_t{1600}; });
    require(!timely.isExpired() && late.isExpired(), "ordered snapshot retains exact watchdog deadline");
    dualdeck::KeyboardBlockSession policy;
    policy.setBackendAvailable(true); policy.select(4); policy.confirmAllKeysReleased();
    require(policy.arm(100) && policy.heartbeat(200), "policy accepts monotonic local clock");
    require(!policy.heartbeat(199) && !policy.isArmed() && policy.stopPadsRequested(), "policy still detects local clock rollback");
}
struct FakeBackend
{
    int filter = 0xffff, clearCalls = 0, received = 0, forwarded = 0, closed = 0;
    int pending = 0, readResult = 1, sendResult = 1;
    bool refuseClear = false;
    std::function<void()> afterClear, duringSend;
    std::vector<int> operations;
    bool clearFilter(int slot) { require(slot == 4, "cleanup touches selected keyboard only"); ++clearCalls; operations.push_back(1); if (!refuseClear) filter = 0; if (afterClear) afterClear(); return !refuseClear; }
    int wait(unsigned long) { return pending > received ? 4 : 0; }
    int receive(int, WireStroke&) { operations.push_back(2); ++received; return readResult; }
    int send(int, const WireStroke&) { operations.push_back(3); ++forwarded; if (duringSend) duringSend(); return sendResult; }
    void close() { ++closed; operations.push_back(4); }
};
void cleanupTests()
{
    FakeBackend backend; backend.pending = 3;
    { FilterOwner<FakeBackend> capture(backend, 4); capture.attempted = true; require(capture.clear(), "filter clear verified"); require(capture.clear(), "cleanup idempotent"); }
    require(backend.clearCalls == 1 && backend.closed == 1 && backend.received == 3 && backend.forwarded == 3, "cleanup removes filter drains forwards closes once");
    require(backend.operations.front() == 1 && backend.operations.back() == 4, "filter cleared before drain and context closed last");
    FakeBackend unused; { FilterOwner<FakeBackend> capture(unused, 4); }
    require(unused.clearCalls == 0 && unused.closed == 1, "early abort closes without setting any filter");
    FakeBackend stuck; stuck.pending = 10000;
    { FilterOwner<FakeBackend> capture(stuck, 4); capture.attempted = true; }
    require(stuck.received == static_cast<int>(capacity) && stuck.closed == 1, "cleanup drain bounded at queue capacity");
    FakeBackend refused; refused.refuseClear = true;
    { FilterOwner<FakeBackend> capture(refused, 4); capture.attempted = true; require(!capture.clear(), "failed filter verification reported"); }
    require(refused.closed == 1, "failed filter verification still destroys context");
    FakeBackend unreadable; unreadable.pending = 10; unreadable.readResult = 0;
    { FilterOwner<FakeBackend> capture(unreadable, 4); capture.attempted = true; }
    require(unreadable.received == 1 && unreadable.forwarded == 0 && unreadable.closed == 1, "cleanup receive error still closes context");
    FakeBackend unsendable; unsendable.pending = 10; unsendable.sendResult = 0;
    { FilterOwner<FakeBackend> capture(unsendable, 4); capture.attempted = true; }
    require(unsendable.received == 1 && unsendable.forwarded == 1 && unsendable.closed == 1, "cleanup send error still closes context");
}
void cleanupDeliveryTests()
{
    for (bool refuseClear : {false, true})
    {
        Shared shared; InterlockedExchange(&shared.state, running);
        require(push(shared, {30, 0, 1}), "queued down before capture retirement");
        FakeBackend backend; backend.refuseClear = refuseClear;
        backend.afterClear = [&]
        {
            Packet packet;
            require(!popForDelivery(shared, packet), "queued down cannot reach pads while filter cleanup is in progress");
            require(read32(shared.state) == stopping, "client sees retirement before cleanup completes");
        };
        FilterOwner<FakeBackend> capture(backend, 4); capture.attempted = true;
        require(retireCapture(shared, capture) == !refuseClear, "retirement preserves cleanup confirmation");
        Packet packet;
        require(!popForDelivery(shared, packet), "retired queue stays unavailable until a new session");
    }
}
void forwardingDeliveryTests()
{
    for (int sendResult : {0, 1})
    {
        Shared shared; InterlockedExchange(&shared.state, running);
        require(push(shared, {30, 0, 1}), "queued down before fail-open forwarding");
        FakeBackend backend; backend.sendResult = sendResult;
        backend.duringSend = [&]
        {
            Packet packet;
            require(!popForDelivery(shared, packet), "queued down cannot reach pads during fail-open forwarding");
        };
        require(forwardRetiring(shared, backend, 4, {}) == sendResult, "retirement preserves forwarding result");
        require(backend.forwarded == 1, "fail-open packet still forwarded exactly once");
    }
}
void filterWriteTests()
{
    struct Entry { HANDLE handle, event; };
    std::array<Entry, 20> context{};
    const auto selected = reinterpret_cast<HANDLE>(std::uintptr_t{1234});
    context[3].handle = selected;
    int calls = 0;
    bool refuse = false;
    unsigned short received = 0;
    auto control = [&](HANDLE handle, DWORD code, void* input, DWORD inputSize, void* output, DWORD outputSize, DWORD* bytes, OVERLAPPED* overlapped) -> BOOL
    {
        ++calls;
        require(handle == selected && code == 0x222010, "filter write addresses only selected pinned context handle");
        require(inputSize == 2 && output == nullptr && outputSize == 0 && overlapped == nullptr, "filter write uses official two-byte synchronous ABI");
        std::memcpy(&received, input, sizeof(received));
        *bytes = 0; // Successful SET returns no output; GET also returns no data.
        SetLastError(refuse ? ERROR_ACCESS_DENIED : ERROR_SUCCESS);
        return refuse ? FALSE : TRUE;
    };
    auto armed = writePinnedKeyboardFilter(context.data(), 4, 0xffff, control);
    require(armed.ok && armed.error == 0 && received == 0xffff && calls == 1, "write acknowledgement arms without unsupported GET_FILTER readback");
    auto cleared = writePinnedKeyboardFilter(context.data(), 4, 0, control);
    require(cleared.ok && received == 0 && calls == 2, "zero filter requires its own acknowledged write");
    refuse = true;
    auto denied = writePinnedKeyboardFilter(context.data(), 4, 0xffff, control);
    require(!denied.ok && denied.error == ERROR_ACCESS_DENIED, "driver rejection remains a failure with Windows error");
    auto clearDenied = writePinnedKeyboardFilter(context.data(), 4, 0, control);
    require(!clearDenied.ok, "failed clear is never reported as confirmed by empty readback");
    const int before = calls;
    for (int slot : {0, -1, 11, 20, 21})
        require(!writePinnedKeyboardFilter(context.data(), slot, 0xffff, control).ok, "invalid and mouse slots rejected before IOCTL");
    require(!writePinnedKeyboardFilter(nullptr, 4, 0xffff, control).ok, "missing context rejected");
    require(!writePinnedKeyboardFilter(context.data(), 5, 0xffff, control).ok, "missing device handle rejected");
    context[4].handle = INVALID_HANDLE_VALUE;
    require(!writePinnedKeyboardFilter(context.data(), 5, 0xffff, control).ok, "invalid device handle rejected");
    require(!writePinnedKeyboardFilter(context.data(), 4, 1, control).ok && calls == before, "unexpected filter mask rejected without device access");
}

}
int main() { queueTests(); identityTests(); decoderTests(); watchdogTests(); heartbeatOrderingTests(); cleanupTests(); cleanupDeliveryTests(); forwardingDeliveryTests(); filterWriteTests(); std::cout << "PASS keyboard bridge: " << checks << " checks\n"; }
