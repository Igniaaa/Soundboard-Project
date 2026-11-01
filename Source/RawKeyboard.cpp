#include "RawKeyboard.h"
#include "KeyboardBridge.h"
#include "KeyboardScan.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <setupapi.h>
#endif

namespace dualdeck
{
struct RawKeyboard::Impl : juce::Timer, std::enable_shared_from_this<Impl>
{
    explicit Impl(RawKeyboard& owner) : parent(&owner)
    {
        bridge.onKey = [this](const juce::String& code, bool down)
        {
            const auto keepAlive = shared_from_this();
            if (parent == nullptr || !requested || identifying || !bridge.active()) return;
            const auto callback = parent->onKey;
            if (callback) callback(code, down);
        };
        bridge.onStateChanged = [this]
        {
            const auto keepAlive = shared_from_this();
            if (parent == nullptr) return;
            if (bridge.error().isNotEmpty()) lastError = bridge.error();
            if (!bridge.starting() && !bridge.active()) { requested = false; stopTimer(); }
            notifyState();
        };
    }
    RawKeyboard* parent;
    KeyboardBridge bridge;
    juce::String selectedId, selectedHardwareId, lastError;
    bool requested = false;
    bool identifying = false;
    PhysicalKeyboardInput localKeys;
    unsigned mutationDepth = 0;
    bool notifying = false;
    bool statePending = false;
    bool notifiedActive = false, notifiedStarting = false;
    juce::String notifiedError;

    bool active() const { return requested && !identifying && bridge.active(); }
    bool starting() const { return requested && !identifying && bridge.starting(); }

    void localInput(std::uintptr_t device, unsigned scan, unsigned flags, bool down)
    {
        const auto key = localKeys.input(device, scan, flags, down);
        if (key.scan == 0) return;
        if (parent == nullptr || requested || identifying || bridge.active() || bridge.starting()) return;
        const auto code = juce::String::formatted("%04X:%u", key.scan, key.flags);
        const auto callback = parent->onLocalKey;
        if (callback) callback(code, key.down);
        if (key.pulse && parent != nullptr)
        {
            const auto released = parent->onLocalKey;
            if (released) released(code, false);
        }
    }

    void notifyState()
    {
        const auto keepAlive = shared_from_this();
        statePending = true;
        if (parent == nullptr || mutationDepth != 0 || notifying) return;
        notifying = true;
        while (parent != nullptr && statePending)
        {
            statePending = false;
            const auto isActive = active(), isStarting = starting();
            if (notifiedActive == isActive && notifiedStarting == isStarting && notifiedError == lastError) continue;
            notifiedActive = isActive; notifiedStarting = isStarting; notifiedError = lastError;
            const auto callback = parent->onCaptureStateChanged;
            if (callback) callback();
        }
        notifying = false;
    }

    // Hold the implementation across callbacks, including callbacks that delete
    // RawKeyboard. Do not expose transient bridge states during start/stop.
    struct Mutation
    {
        std::shared_ptr<Impl> self;
        explicit Mutation(Impl& value) : self(value.shared_from_this()) { ++self->mutationDepth; }
        ~Mutation() { --self->mutationDepth; self->notifyState(); }
    };

    void stopCapture()
    {
        requested = false;
        stopTimer();
        bridge.stop();
    }

    void timerCallback() override
    {
        const auto keepAlive = shared_from_this();
        if (parent == nullptr || !requested) { stopTimer(); return; }
        bridge.poll();
        if (parent == nullptr) return;
        if (bridge.error().isNotEmpty()) lastError = bridge.error();
        if (!bridge.active() && !bridge.starting()) { requested = false; stopTimer(); }
        notifyState();
    }

    void shutdown()
    {
        parent = nullptr;
        bridge.onKey = nullptr;
        bridge.onStateChanged = nullptr;
        stopCapture();
        identifying = false;
#if JUCE_WINDOWS
        if (window != nullptr)
        {
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            RAWINPUTDEVICE remove {1, 6, RIDEV_REMOVE, nullptr};
            RegisterRawInputDevices(&remove, 1, sizeof(remove));
            DestroyWindow(window);
            window = nullptr;
        }
#endif
    }
#if JUCE_WINDOWS
    HWND window = nullptr;
    static juce::Result hardwareId(const juce::String& rawPath, juce::String& result)
    {
        result.clear();
        const auto instance = rawKeyboardDetail::instanceId(std::wstring(rawPath.toWideCharPointer()));
        if (instance.empty()) return juce::Result::fail("Percorso della tastiera non riconosciuto: seleziona nuovamente il dispositivo.");
        // GUID_DEVCLASS_KEYBOARD, kept local to avoid an extra UUID library.
        static constexpr GUID keyboardClass {0x4d36e96b, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
        const auto info = SetupDiGetClassDevsW(&keyboardClass, nullptr, nullptr, DIGCF_PRESENT);
        if (info == INVALID_HANDLE_VALUE) return juce::Result::fail("Impossibile leggere l'identità PnP della tastiera.");
        struct DeviceList { HDEVINFO value; ~DeviceList() { SetupDiDestroyDeviceInfoList(value); } } cleanup {info};
        unsigned matches = 0;
        for (DWORD index = 0;; ++index)
        {
            SP_DEVINFO_DATA device {}; device.cbSize = sizeof(device);
            if (!SetupDiEnumDeviceInfo(info, index, &device))
            {
                if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
                return juce::Result::fail("Enumerazione PnP della tastiera non riuscita.");
            }
            DWORD chars = 0;
            if (SetupDiGetDeviceInstanceIdW(info, &device, nullptr, 0, &chars) || GetLastError() != ERROR_INSUFFICIENT_BUFFER || chars < 2 || chars > 32768)
                return juce::Result::fail("Identità PnP della tastiera non verificabile.");
            std::vector<wchar_t> deviceInstance(chars, 0);
            if (!SetupDiGetDeviceInstanceIdW(info, &device, deviceInstance.data(), chars, nullptr))
                return juce::Result::fail("Lettura dell'istanza PnP non riuscita.");
            if (!juce::String(deviceInstance.data()).equalsIgnoreCase(juce::String(instance.c_str()))) continue;
            if (++matches != 1) return juce::Result::fail("Identità PnP ambigua: cattura della tastiera disattivata.");
            DWORD bytes = 0, type = 0;
            if (SetupDiGetDeviceRegistryPropertyW(info, &device, SPDRP_HARDWAREID, &type, nullptr, 0, &bytes)
                || GetLastError() != ERROR_INSUFFICIENT_BUFFER || type != REG_MULTI_SZ || bytes < 2 * sizeof(wchar_t) || bytes > 65536 || bytes % sizeof(wchar_t) != 0)
                return juce::Result::fail("HardwareId PnP della tastiera assente o non valido.");
            std::vector<wchar_t> ids(bytes / sizeof(wchar_t), 0);
            DWORD readBytes = 0;
            if (!SetupDiGetDeviceRegistryPropertyW(info, &device, SPDRP_HARDWAREID, &type, reinterpret_cast<PBYTE>(ids.data()), bytes, &readBytes)
                || type != REG_MULTI_SZ || readBytes != bytes || ids[ids.size() - 1] != 0 || ids[ids.size() - 2] != 0)
                return juce::Result::fail("Lettura dell'HardwareId PnP della tastiera non riuscita.");
            const auto end = std::find(ids.begin(), ids.end(), wchar_t{});
            const std::wstring first(ids.begin(), end);
            // MULTI_SZ is ordered by specificity. Never fall back to a generic
            // later ID, or choose an unrelated instance sharing a VID/PID.
            if (!rawKeyboardDetail::specificHardwareId(first))
                return juce::Result::fail("HardwareId PnP troppo generico: impossibile catturare questa tastiera in sicurezza.");
            result = juce::String(first.c_str());
        }
        return matches == 1 ? juce::Result::ok() : juce::Result::fail("L'istanza PnP esatta della tastiera non è presente. Aggiorna la selezione.");
    }
    static juce::String path(HANDLE device)
    {
        UINT count = 0;
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &count) == static_cast<UINT>(-1) || count == 0) return {};
        std::vector<wchar_t> text(static_cast<std::size_t>(count) + 1, 0);
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, text.data(), &count) == static_cast<UINT>(-1)) return {};
        return juce::String(text.data());
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        const auto keepAlive = self != nullptr ? self->shared_from_this() : std::shared_ptr<Impl>{};
        if (self && message == WM_INPUT)
        {
            RAWINPUT input {}; UINT bytes = sizeof(input);
            const auto size = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &input, &bytes, sizeof(RAWINPUTHEADER));
            if (size != static_cast<UINT>(-1) && size >= sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD)
                && input.header.dwType == RIM_TYPEKEYBOARD && input.header.hDevice != nullptr)
            {
                const auto& keyboard = input.data.keyboard;
                unsigned scan = keyboard.MakeCode;
                auto flags = static_cast<unsigned>(keyboard.Flags & (RI_KEY_E0 | RI_KEY_E1));
                if (scan == 0)
                {
                    const auto mapped = MapVirtualKeyW(keyboard.VKey, MAPVK_VK_TO_VSC_EX);
                    scan = mapped & 0xff;
                    if (flags == 0) flags = (mapped & 0xff00) == 0xe000 ? 2u : (mapped & 0xff00) == 0xe100 ? 4u : 0u;
                }
                if (scan != 0 && scan != 0xff && keyboard.VKey < 255)
                {
                    const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
                    self->localInput(reinterpret_cast<std::uintptr_t>(input.header.hDevice), scan, flags, down);
                    // Dedicated pad events still come exclusively from the bridge.
                    if (self->parent != nullptr && self->identifying && !self->requested && scan == 0x58 && flags == 0 && !down)
                    {
                        self->identifying = false;
                        const auto id = path(input.header.hDevice);
                        const auto callback = self->parent->onIdentify;
                        if (callback) callback(id);
                    }
                }
            }
            // DefWindowProc performs the required cleanup for foreground WM_INPUT.
        }
        if (self && message == WM_INPUT_DEVICE_CHANGE)
        {
            {
                Mutation mutation(*self);
                const bool wasCapturing = self->requested || self->bridge.active() || self->bridge.starting();
                self->stopCapture();
                self->localKeys.clear();
                self->identifying = false;
                self->selectedId.clear(); self->selectedHardwareId.clear();
                if (wasCapturing) self->lastError = "Dispositivi tastiera cambiati: seleziona e attiva nuovamente la tastiera dedicata.";
            }
            if (self->parent != nullptr)
            {
                const auto callback = self->parent->onDevicesChanged;
                if (callback) callback();
            }
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
#endif
};
RawKeyboard::RawKeyboard() : impl(std::make_shared<Impl>(*this)) {}
RawKeyboard::~RawKeyboard() { impl->shutdown(); }
juce::Result RawKeyboard::initialise()
{
#if JUCE_WINDOWS
    if (impl->window) return juce::Result::ok();
    const wchar_t* className = L"DualDeckJuceRawKeyboard";
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW klass {}; klass.lpfnWndProc = Impl::procedure; klass.hInstance = instance; klass.lpszClassName = className;
    if (!RegisterClassW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return juce::Result::fail("Impossibile creare il ricevitore della tastiera.");
    impl->window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, impl.get());
    if (!impl->window) return juce::Result::fail("Impossibile aprire il ricevitore della tastiera.");
    RAWINPUTDEVICE registration {1, 6, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, impl->window};
    if (!RegisterRawInputDevices(&registration, 1, sizeof(registration)))
    {
        DestroyWindow(impl->window); impl->window = nullptr;
        return juce::Result::fail("Registrazione Raw Input non riuscita.");
    }
    return juce::Result::ok();
#else
    return juce::Result::fail("La tastiera dedicata richiede Windows.");
#endif
}
std::vector<KeyboardDevice> RawKeyboard::devices()
{
    std::vector<KeyboardDevice> result;
#if JUCE_WINDOWS
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) return result;
    if (count == 0) return result;
    std::vector<RAWINPUTDEVICELIST> items(count);
    const auto read = GetRawInputDeviceList(items.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (read == static_cast<UINT>(-1)) return result;
    for (UINT i = 0; i < read; ++i)
    {
        if (items[i].dwType != RIM_TYPEKEYBOARD) continue;
        const auto id = Impl::path(items[i].hDevice);
        if (id.isEmpty()) continue;
        auto detail = id.fromFirstOccurrenceOf("HID#", false, true).upToFirstOccurrenceOf("#", false, false);
        if (detail.isEmpty()) detail = id.substring(0, 70);
        result.push_back({id, "Tastiera " + juce::String(result.size() + 1) + "  /  " + detail, reinterpret_cast<std::uintptr_t>(items[i].hDevice)});
    }
#endif
    return result;
}
juce::Result RawKeyboard::select(const juce::String& id)
{
    Impl::Mutation mutation(*impl);
    auto& state = *mutation.self;
    state.stopCapture(); state.identifying = false;
    state.selectedId.clear(); state.selectedHardwareId.clear(); state.lastError.clear();
    for (const auto& item : devices()) if (item.id == id)
    {
        state.selectedId = id;
#if JUCE_WINDOWS
        const auto mapped = Impl::hardwareId(id, state.selectedHardwareId);
        if (mapped.failed()) state.lastError = mapped.getErrorMessage();
        return mapped;
#endif
    }
    state.lastError = "Tastiera non collegata. Aggiorna l'elenco o usa Riconosci con F12.";
    return juce::Result::fail(state.lastError);
}
void RawKeyboard::enable(bool value)
{
    Impl::Mutation mutation(*impl);
    auto& state = *mutation.self;
    if (!value) { state.stopCapture(); return; }
    if (state.requested && (state.bridge.active() || state.bridge.starting())) return;
    state.stopCapture(); state.identifying = false; state.lastError.clear();
#if JUCE_WINDOWS
    const auto current = devices();
    const auto present = std::find_if(current.begin(), current.end(), [&](const auto& item) { return item.id == state.selectedId; });
    if (state.selectedId.isEmpty() || present == current.end())
    {
        state.lastError = "Seleziona una tastiera collegata prima di attivare la cattura.";
        return;
    }
    const auto mapped = Impl::hardwareId(state.selectedId, state.selectedHardwareId);
    if (mapped.failed()) { state.lastError = mapped.getErrorMessage(); return; }
    state.requested = true;
    const auto started = state.bridge.start(state.selectedHardwareId);
    if (started.failed())
    {
        state.lastError = started.getErrorMessage();
        state.stopCapture();
        return;
    }
    if (state.requested && (state.bridge.starting() || state.bridge.active())) state.startTimer(50);
    else
    {
        if (state.lastError.isEmpty()) state.lastError = "La cattura della tastiera non è stata avviata.";
        state.stopCapture();
    }
#else
    state.lastError = "La tastiera dedicata richiede Windows.";
#endif
}
bool RawKeyboard::enabled() const { return impl->active(); }
bool RawKeyboard::activating() const { return impl->starting(); }
juce::String RawKeyboard::error() const { return impl->lastError; }
bool RawKeyboard::identifying() const { return impl->identifying; }
void RawKeyboard::identify(bool value)
{
    Impl::Mutation mutation(*impl);
    if (value) mutation.self->stopCapture();
    mutation.self->identifying = value;
}
}
