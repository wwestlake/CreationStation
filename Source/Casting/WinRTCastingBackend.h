#pragma once

#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include "CastingBackend.h"

namespace cs::casting
{

// The real CastingBackend: Windows' own Windows.Media.Casting API (the "Play To" successor),
// which discovers and casts to both Miracast and DLNA/UPnP receivers through one OS-native
// picker and connection API -- no Google/Apple account, no receiver app registration, no new
// third-party dependency. See the Work Order 3 feasibility-gate findings for why this, rather
// than a Chromecast custom receiver or raw desktop-mirroring Miracast, is the approach that
// fits this project's constraints.
//
// Needs the native window handle of the component that owns the Cast button, both to anchor
// the OS device picker UI near it and because CastingDevicePicker requires IInitializeWithWindow
// interop on a classic Win32/JUCE app (it has no window of its own to anchor to otherwise).
class WinRTCastingBackend : public CastingBackend
{
public:
    explicit WinRTCastingBackend(juce::Component& ownerWindow);
    ~WinRTCastingBackend() override;

    void beginDiscovery(juce::Rectangle<int> anchorScreenBounds) override;
    void cancelDiscovery() override;
    void startCasting(const juce::String& streamUrl) override;
    void stopCasting() override;

    // True only on Windows, and only once construction has confirmed the OS casting API is
    // actually present (not, e.g., an older Windows build lacking Windows.Media.Casting).
    bool isAvailable() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE(WinRTCastingBackend)
};

} // namespace cs::casting
