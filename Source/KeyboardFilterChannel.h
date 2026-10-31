#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>

namespace dualdeck::keyboardbridge
{
struct FilterWriteResult { bool ok; DWORD error; };
// Only for the SHA-256-pinned official v1.0.1 x64 DLL. Its library/interception.c
// context is an array of 20 { HANDLE handle; HANDLE unempty; } entries.
// The installed driver implements SET_FILTER but not GET_FILTER. Calling the
// void DLL setter loses the IOCTL acknowledgement, so retain that result here.
template<class Control> FilterWriteResult writePinnedKeyboardFilter(void* context, int slot, unsigned short filter, Control control)
{
    static_assert(sizeof(HANDLE) == 8, "Pinned Interception runtime requires x64");
    if (!context || slot < 1 || slot > 10 || (filter != 0 && filter != 0xffff))
        return {false, ERROR_INVALID_PARAMETER};
    HANDLE handle = nullptr;
    const auto* entry = static_cast<const unsigned char*>(context) + (slot - 1) * 2 * sizeof(HANDLE);
    std::memcpy(&handle, entry, sizeof(handle));
    if (!handle || handle == INVALID_HANDLE_VALUE) return {false, ERROR_INVALID_HANDLE};
    DWORD bytes = 0;
    if (!control(handle, 0x222010, &filter, sizeof(filter), nullptr, 0, &bytes, nullptr))
    {
        const auto error = GetLastError();
        return {false, error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE};
    }
    return {true, ERROR_SUCCESS};
}
inline FilterWriteResult writePinnedKeyboardFilter(void* context, int slot, unsigned short filter)
{
    return writePinnedKeyboardFilter(context, slot, filter,
        [](HANDLE handle, DWORD code, void* input, DWORD inputSize, void* output, DWORD outputSize, DWORD* bytes, OVERLAPPED* overlapped)
        { return DeviceIoControl(handle, code, input, inputSize, output, outputSize, bytes, overlapped); });
}
}
