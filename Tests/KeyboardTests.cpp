#include "KeyboardFilter.h"
#include "KeyboardBlockSession.h"
#include "KeyboardDeviceIdentity.h"
#include <cstdlib>
#include <iostream>
#include <type_traits>
using namespace dualdeck;
int checks = 0;
void require(bool condition, const char* name) { if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); } ++checks; }
void identityTests()
{
    using namespace dualdeck::rawKeyboardDetail;
    require(instanceId(LR"(\\?\HID#VID_17EF&PID_608C&MI_00#7&123&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd})") == LR"(HID\VID_17EF&PID_608C&MI_00\7&123&0&0000)", "exact interface becomes exact PnP instance");
    require(instanceId(LR"(\\?\hid#vid_17ef&pid_608c#instance#{884B96C3-56EF-11D1-BC8C-00A0C91405DD})") == LR"(hid\vid_17ef&pid_608c\instance)", "case-insensitive enumerator");
    require(instanceId(LR"(HID#VID_17EF&PID_608C#instance#{884b96c3-56ef-11d1-bc8c-00a0c91405dd})").empty(), "missing prefix");
    require(instanceId(LR"(\\?\HID#VID_17EF&PID_608C##{884b96c3-56ef-11d1-bc8c-00a0c91405dd})").empty(), "missing instance");
    require(instanceId(LR"(\\?\HID#VID_17EF&PID_608C#instance#extra#{884b96c3-56ef-11d1-bc8c-00a0c91405dd})").empty(), "ambiguous separators");
    require(instanceId(LR"(\\?\HID#VID_17EF&PID_608C#instance#not-a-guid)").empty(), "malformed suffix");
    require(instanceId(LR"(\\?\USB#VID_17EF&PID_608C#instance#{884b96c3-56ef-11d1-bc8c-00a0c91405dd})").empty(), "different enumerator");
    require(specificHardwareId(LR"(HID\VID_17EF&PID_608C&REV_0100&MI_00)"), "specific VID/PID accepted");
    require(specificHardwareId(LR"(hid\vid_17ef&pid_608c&mi_00)"), "lowercase accepted");
    require(!specificHardwareId(L"HID_DEVICE_SYSTEM_KEYBOARD"), "generic ID rejected");
    require(!specificHardwareId(LR"(HID\VID_0000&PID_608C)"), "zero vendor rejected");
    require(!specificHardwareId(LR"(HID\VID_17EF&PID_0000)"), "zero product rejected");
    require(!specificHardwareId(LR"(HID\VID_17EG&PID_608C)"), "invalid hex rejected");
    require(!specificHardwareId(LR"(HID\VID_17EF&PID_608C7)"), "substring rejected");
    require(!specificHardwareId(LR"(USB\VID_17EF&PID_608C)"), "non-HID ID rejected");
}
using Session = KeyboardBlockSession;
void ready(Session& session, std::uint64_t now = 100)
{
    session.setBackendAvailable(true);
    session.select(20);
    require(session.confirmAllKeysReleased(), "trusted released confirmation accepted");
    require(session.arm(now), "unique released device arms with available backend");
}
void sessionTests()
{
    Session gate;
    require(!gate.arm(100), "new session cannot capture");
    gate.select(20);
    gate.confirmAllKeysReleased();
    require(!gate.arm(100), "backend required before arming");
    gate.setBackendAvailable(true);
    require(!gate.arm(100), "backend connection invalidates old released confirmation");
    gate.select(0); gate.confirmAllKeysReleased();
    require(!gate.arm(100), "zero selection cannot arm");
    gate.select(20, 2); gate.confirmAllKeysReleased();
    require(!gate.arm(100), "ambiguous device cannot arm");
    gate.select(20, 0); gate.confirmAllKeysReleased();
    require(!gate.arm(100), "absent device cannot arm");
    gate.select(20);
    gate.input(20, 30, 0, true, 100);
    require(!gate.confirmAllKeysReleased(), "trusted confirmation cannot erase observed held key");
    require(!gate.arm(100), "selected held key prevents capture");
    gate.input(20, 30, 0, false, 100);
    require(!gate.arm(100), "release alone cannot replace trusted confirmation");
    require(gate.confirmAllKeysReleased() && gate.arm(100), "released selected key permits explicit arm");

    Session delivery; ready(delivery);
    auto result = delivery.input(99, 30, 0, true, 101);
    require(!result.suppress && result.event == Session::none, "other keyboard always passes without delivery");
    result = delivery.input(20, 30, 0, true, 102);
    require(result.suppress && result.event == Session::pressed, "selected press delivered and suppressed");
    result = delivery.input(20, 30, 0, true, 103, false);
    require(result.suppress && result.event == Session::none && delivery.isArmed(), "repeat suppressed without needing queue space");
    result = delivery.input(99, 30, 0, false, 104);
    require(!result.suppress && result.event == Session::none, "other keyboard cannot release selected key");
    result = delivery.input(20, 30, 0, false, 105);
    require(result.suppress && result.event == Session::released, "selected release delivered");
    result = delivery.input(20, 30, 0, false, 106, false);
    require(result.suppress && result.event == Session::none && delivery.isArmed(), "unmatched release suppressed without queue requirement");
    for (unsigned prefix = 0; prefix != 3; ++prefix)
        require(delivery.input(20, 29, prefix, true, 107).event == Session::pressed, "normal E0 E1 presses independent");
    for (unsigned prefix = 0; prefix != 3; ++prefix)
        require(delivery.input(20, 29, prefix, false, 108).event == Session::released, "normal E0 E1 releases independent");
    result = delivery.input(0, 0, 99, true, 109, false);
    require(!result.suppress && delivery.isArmed(), "malformed nonselected source still passes");
    result = delivery.input(99, 255, 0, true, 110);
    require(!result.suppress && result.event == Session::none && delivery.isArmed(),
            "another keyboard overrun passes without disarming selected capture");

    Session timeout; ready(timeout);
    timeout.poll(1599);
    require(timeout.isArmed(), "heartbeat valid through 1499ms");
    timeout.poll(1600);
    require(!timeout.isArmed() && timeout.stopPadsRequested(), "heartbeat expires at exactly 1500ms and requests stop");
    require(!timeout.heartbeat(1601) && !timeout.isArmed(), "late heartbeat never rearms");
    timeout.confirmAllKeysReleased();
    require(!timeout.arm(1601), "pending pad stop prevents rearm");
    timeout.acknowledgeStopPads();
    require(timeout.arm(1601), "acknowledged stop permits deliberate rearm with fresh confirmation");
    require(timeout.heartbeat(3000), "timely heartbeat renews deadline");
    timeout.poll(4499);
    require(timeout.isArmed(), "renewed heartbeat lasts 1499ms");
    result = timeout.input(20, 32, 0, true, 4500);
    require(!result.suppress && result.event == Session::none && timeout.stopPadsRequested(), "packet at deadline forwards after disarm");

    Session rollback; ready(rollback);
    rollback.poll(200);
    require(!rollback.heartbeat(199) && rollback.stopPadsRequested(), "clock rollback disarms before heartbeat renewal");
    rollback.acknowledgeStopPads();
    require(!rollback.arm(200), "disarm consumes release confirmation");

    Session disabled; ready(disabled);
    disabled.input(20, 30, 0, true, 101);
    disabled.disable();
    require(!disabled.isArmed() && disabled.stopPadsRequested(), "disable requests pad stop");
    result = disabled.input(20, 30, 0, false, 102);
    require(!result.suppress && result.event == Session::none, "disabled selected release forwards");
    disabled.acknowledgeStopPads();
    require(disabled.confirmAllKeysReleased() && disabled.arm(103), "forwarded release clears held state");

    Session backend; ready(backend);
    backend.input(20, 30, 0, true, 101);
    backend.setBackendAvailable(false);
    require(!backend.isArmed() && backend.stopPadsRequested(), "backend loss restores forwarding and requests stop");
    backend.acknowledgeStopPads();
    backend.setBackendAvailable(true);
    require(!backend.arm(102), "reconnected backend requires fresh released confirmation");
    require(backend.confirmAllKeysReleased() && backend.arm(103), "backend reconnection resets obsolete held state");
    backend.setBackendAvailable(true);
    require(backend.isArmed(), "unchanged backend status preserves session");

    Session change; ready(change);
    change.input(20, 30, 0, true, 101);
    change.select(21);
    require(!change.isArmed() && change.stopPadsRequested(), "selection change disarms and requests stop");
    change.acknowledgeStopPads();
    require(!change.arm(102), "new selection requires fresh released confirmation");
    require(change.confirmAllKeysReleased() && change.arm(103), "selection resets old device held state");
    change.disconnect(20);
    require(change.isArmed(), "unrelated disconnect preserves capture");
    change.disconnect(21);
    require(!change.isArmed() && change.stopPadsRequested(), "selected disconnect disarms and requests stop");
    change.acknowledgeStopPads(); change.confirmAllKeysReleased();
    require(!change.arm(104), "disconnected selection cannot rearm");

    for (bool down : {true, false})
    {
        Session queue; ready(queue);
        if (!down) queue.input(20, 30, 0, true, 101);
        result = queue.input(20, 30, 0, down, 102, false);
        require(!result.suppress && result.event == Session::none && !queue.isArmed() && queue.stopPadsRequested(), "failed delivery forwards and requests pad stop");
    }
    for (auto malformed : {0u, 255u, 256u, 999u})
    {
        Session invalid; ready(invalid);
        result = invalid.input(20, malformed, 0, true, 101);
        require(!result.suppress && result.event == Session::none && !invalid.isArmed() && invalid.stopPadsRequested(), "malformed selected scan disarms and forwards");
    }
    Session prefix; ready(prefix);
    result = prefix.input(20, 30, 3, true, 101);
    require(!result.suppress && !prefix.isArmed() && prefix.stopPadsRequested(), "invalid selected prefix disarms and forwards");
    Session idle;
    idle.disable(); idle.setBackendAvailable(false); idle.select(0);
    require(!idle.stopPadsRequested(), "unarmed changes do not spuriously request pad stop");
    require(!std::is_copy_constructible<Session>::value && !std::is_move_constructible<Session>::value,
            "capture session has a single nontransferable owner");
}
int main()
{
    KeyboardFilter input;
    input.enable(true);
    require(!input.isEnabled(), "cannot arm without device");
    input.select(2); input.enable(true);
    require(input.input(1, 30, true) == KeyboardFilter::ignored, "main keyboard does not trigger");
    require(input.input(2, 30, true) == KeyboardFilter::pressed, "selected keyboard press");
    require(input.input(2, 30, true) == KeyboardFilter::ignored, "auto-repeat is ignored");
    require(input.input(1, 30, false) == KeyboardFilter::ignored, "main keyboard cannot release secondary key");
    require(input.input(2, 30, false) == KeyboardFilter::released, "selected keyboard release");
    require(input.input(2, 30, false) == KeyboardFilter::ignored, "unmatched release ignored");
    input.enable(false);
    require(input.input(2, 31, true) == KeyboardFilter::ignored, "disabled press tracked silently");
    input.enable(true);
    require(input.input(2, 31, true) == KeyboardFilter::ignored, "held key cannot restart on re-enable");
    require(input.input(2, 31, false) == KeyboardFilter::released, "held key release clears state");
    require(input.input(2, 31, true) == KeyboardFilter::pressed, "fresh key works after release");
    input.select(3);
    require(!input.isEnabled(), "changing device disarms capture");
    input.enable(true);
    require(input.input(2, 31, true) == KeyboardFilter::ignored, "old device cannot trigger after switch");
    require(input.input(3, 0x1d, true) == KeyboardFilter::pressed, "left modifier");
    require(input.input(3, 0x2001d, true) == KeyboardFilter::pressed, "extended right modifier independent");
    require(input.input(0, 30, true) == KeyboardFilter::ignored, "synthetic unknown source ignored");
    sessionTests();
    identityTests();
    std::cout << "PASS: " << checks << " keyboard checks\n";
}
