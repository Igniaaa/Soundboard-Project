#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace dualdeck
{
namespace rawKeyboardDetail
{
inline std::wstring upper(std::wstring_view value)
{
    std::wstring result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t c) { return c >= L'a' && c <= L'z' ? static_cast<wchar_t>(c - L'a' + L'A') : c; });
    return result;
}
// An interface path identifies one instance; a VID/PID search does not.
inline std::wstring instanceId(std::wstring_view path)
{
    if (path.size() < 4 || path.substr(0, 4) != L"\\\\?\\") return {};
    path.remove_prefix(4);
    const auto first = path.find(L'#');
    if (first == std::wstring_view::npos || upper(path.substr(0, first)) != L"HID") return {};
    const auto second = path.find(L'#', first + 1);
    if (second == std::wstring_view::npos || second == first + 1) return {};
    const auto third = path.find(L'#', second + 1);
    if (third == std::wstring_view::npos || third == second + 1) return {};
    const auto suffix = path.substr(third + 1);
    if (suffix.size() != 38 || suffix.front() != L'{' || suffix.back() != L'}') return {};
    for (std::size_t i = 1; i + 1 < suffix.size(); ++i)
    {
        if (i == 9 || i == 14 || i == 19 || i == 24) { if (suffix[i] != L'-') return {}; }
        else if (! ((suffix[i] >= L'0' && suffix[i] <= L'9') || (suffix[i] >= L'a' && suffix[i] <= L'f') || (suffix[i] >= L'A' && suffix[i] <= L'F'))) return {};
    }
    std::wstring result(path.substr(0, third));
    if (result.find_first_of(L"\\/") != std::wstring::npos || result.find(L'\0') != std::wstring::npos) return {};
    std::replace(result.begin(), result.end(), L'#', L'\\');
    return result;
}
inline bool specificHardwareId(std::wstring_view value)
{
    const auto id = upper(value);
    if (id.size() < 21 || id.substr(0, 8) != L"HID\\VID_" || id.substr(12, 5) != L"&PID_") return false;
    for (const auto begin : {8u, 17u})
    {
        bool nonzero = false;
        for (unsigned i = begin; i < begin + 4; ++i)
        {
            const auto c = id[i];
            if (! ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F'))) return false;
            nonzero = nonzero || c != L'0';
        }
        if (!nonzero) return false;
    }
    return (id.size() == 21 || id[21] == L'&') && id.find_first_of(L"\\/#", 4) == std::wstring::npos && id.find(L'\0') == std::wstring::npos;
}
}
}
