#pragma once

#include <wdm.h>

namespace djehuti::router::driver_contract
{
inline constexpr ULONG sampleRate = 48000;
inline constexpr ULONG blockSize = 512;

inline constexpr const wchar_t* productName = L"Djehuti Router";
inline constexpr const wchar_t* shortName = L"DjeRoute";

inline constexpr const wchar_t* captureEndpoint = L"Djehuti Router Capture";
inline constexpr const wchar_t* monitorEndpoint = L"Djehuti Router Monitor";
inline constexpr const wchar_t* micEndpoint = L"Djehuti Router Mic";
inline constexpr const wchar_t* instrumentEndpoint = L"Djehuti Router Instrument";
}
