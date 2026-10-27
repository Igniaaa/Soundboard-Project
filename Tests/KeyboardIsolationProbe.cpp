#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <iostream>
#include <vector>

namespace
{
std::atomic<unsigned> rawEvents {0}, hookEvents {0};
LRESULT CALLBACK lowLevelHook(int code, WPARAM event, LPARAM data)
{
    if (code == HC_ACTION && (event == WM_KEYDOWN || event == WM_KEYUP || event == WM_SYSKEYDOWN || event == WM_SYSKEYUP))
    {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        ++hookEvents;
        std::wcout << L"HOOK scan=" << key->scanCode << L" flags=" << key->flags
                   << L" injected=" << ((key->flags & LLKHF_INJECTED) != 0) << L" device=<not provided>\n";
    }
    return CallNextHookEx(nullptr, code, event, data); // This probe never suppresses a key.
}
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_INPUT)
    {
        UINT bytes = 0;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &bytes,
                            sizeof(RAWINPUTHEADER)) == 0 && bytes >= sizeof(RAWINPUTHEADER))
        {
            std::vector<BYTE> buffer(bytes);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, buffer.data(), &bytes,
                                sizeof(RAWINPUTHEADER)) == bytes)
            {
                const auto* input = reinterpret_cast<const RAWINPUT*>(buffer.data());
                if (input->header.dwType == RIM_TYPEKEYBOARD)
                {
                    ++rawEvents;
                    std::wcout << L"RAW scan=" << input->data.keyboard.MakeCode
                               << L" device=" << input->header.hDevice << L"\n";
                }
            }
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
int wmain(int argc, wchar_t** argv)
{
    const int duration = argc > 1 ? _wtoi(argv[1]) : 3000;
    std::wcout << L"Keyboard isolation feasibility probe; no key is blocked.\n";
    UINT deviceCount = 0;
    if (GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST)) == 0 && deviceCount > 0)
    {
        std::vector<RAWINPUTDEVICELIST> devices(deviceCount);
        const auto read = GetRawInputDeviceList(devices.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST));
        unsigned keyboardCount = 0;
        if (read != static_cast<UINT>(-1))
            for (UINT i = 0; i < read; ++i) if (devices[i].dwType == RIM_TYPEKEYBOARD) ++keyboardCount;
        std::wcout << L"Raw keyboard collections: " << keyboardCount << L"\n";
    }
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW klass {};
    klass.lpfnWndProc = windowProc; klass.hInstance = instance; klass.lpszClassName = L"DualDeckKeyboardIsolationProbe";
    if (!RegisterClassW(&klass)) { std::wcerr << L"Window class registration failed.\n"; return 1; }
    const auto window = CreateWindowExW(0, klass.lpszClassName, L"", 0, 0, 0, 0, 0,
                                        HWND_MESSAGE, nullptr, instance, nullptr);
    if (!window) { std::wcerr << L"Message window creation failed.\n"; return 1; }
    RAWINPUTDEVICE registration {1, 6, RIDEV_INPUTSINK, window};
    if (!RegisterRawInputDevices(&registration, 1, sizeof(registration)))
    { std::wcerr << L"Raw Input registration failed.\n"; DestroyWindow(window); return 1; }
    const auto hook = SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelHook, instance, 0);
    if (!hook)
    { std::wcerr << L"Low-level hook registration failed.\n"; DestroyWindow(window); return 1; }
    const auto end = GetTickCount64() + static_cast<ULONGLONG>(duration > 0 ? duration : 3000);
    while (GetTickCount64() < end)
    {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        { TranslateMessage(&message); DispatchMessageW(&message); }
        Sleep(5);
    }
    UnhookWindowsHookEx(hook);
    RAWINPUTDEVICE remove {1, 6, RIDEV_REMOVE, nullptr};
    RegisterRawInputDevices(&remove, 1, sizeof(remove));
    DestroyWindow(window);
    std::wcout << L"Raw events: " << rawEvents.load() << L"; hook events: " << hookEvents.load() << L"\n";
    std::wcout << L"The hook event has no keyboard device identifier; Raw Input identifies the device but cannot cancel delivery to other apps.\n";
    return 0;
}
