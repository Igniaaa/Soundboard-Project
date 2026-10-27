#include "RawKeyboard.h"
#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace dualdeck
{
struct RawKeyboard::Impl
{
    explicit Impl(RawKeyboard& owner) : parent(owner) {}
    RawKeyboard& parent;
    KeyboardFilter filter;
    bool identifying = false;
#if JUCE_WINDOWS
    HWND window = nullptr;
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
        if (self && message == WM_INPUT)
        {
            RAWINPUT input {}; UINT bytes = sizeof(input);
            const auto size = GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &input, &bytes, sizeof(RAWINPUTHEADER));
            if (size != static_cast<UINT>(-1) && size >= sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD)
                && input.header.dwType == RIM_TYPEKEYBOARD && input.header.hDevice != nullptr)
            {
                const auto& keyboard = input.data.keyboard;
                unsigned scan = keyboard.MakeCode;
                if (scan == 0) scan = MapVirtualKeyW(keyboard.VKey, MAPVK_VK_TO_VSC_EX) & 0xff;
                if (scan != 0 && scan != 0xff && keyboard.VKey < 255)
                {
                    const auto flags = keyboard.Flags & (RI_KEY_E0 | RI_KEY_E1);
                    const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
                    const auto source = reinterpret_cast<std::uintptr_t>(input.header.hDevice);
                    const auto event = self->filter.input(source, scan | (flags << 16), down);
                    if (self->identifying && scan == 0x58 && flags == 0 && !down)
                    {
                        self->identifying = false;
                        if (self->parent.onIdentify) self->parent.onIdentify(path(input.header.hDevice));
                    }
                    else if (event != KeyboardFilter::ignored && self->parent.onKey)
                        self->parent.onKey(juce::String::formatted("%04X:%u", scan, static_cast<unsigned>(flags)), event == KeyboardFilter::pressed);
                }
            }
            // DefWindowProc performs the required cleanup for foreground WM_INPUT.
        }
        if (self && message == WM_INPUT_DEVICE_CHANGE)
        {
            self->filter.enable(false);
            self->identifying = false;
            if (self->parent.onDevicesChanged) self->parent.onDevicesChanged();
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
#endif
};
RawKeyboard::RawKeyboard() : impl(std::make_unique<Impl>(*this)) {}
RawKeyboard::~RawKeyboard()
{
#if JUCE_WINDOWS
    if (impl->window)
    {
        RAWINPUTDEVICE remove {1, 6, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&remove, 1, sizeof(remove));
        DestroyWindow(impl->window);
    }
#endif
}
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
    impl->filter.select(0);
    for (const auto& item : devices()) if (item.id == id) { impl->filter.select(item.handle); return juce::Result::ok(); }
    return juce::Result::fail("Tastiera non collegata. Aggiorna l'elenco o usa Riconosci con F12.");
}
void RawKeyboard::enable(bool value) { impl->filter.enable(value); }
bool RawKeyboard::enabled() const { return impl->filter.isEnabled(); }
bool RawKeyboard::identifying() const { return impl->identifying; }
void RawKeyboard::identify(bool value) { impl->identifying = value; if (value) enable(false); }
}
