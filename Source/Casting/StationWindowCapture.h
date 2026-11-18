#pragma once

#include <memory>
#include <juce_graphics/juce_graphics.h>

namespace cs::casting
{

// One captured frame: a CPU-side BGRA snapshot of Station's own main window, plus the time (in
// seconds since this capture session started) it was taken.
struct CapturedFrame
{
    juce::Image image;
    double captureTimeSeconds = 0.0;
};

// Captures Station's own top-level window -- never the whole desktop, and never any other
// app's window -- using Windows Graphics Capture (the same WinRT API Djehuti Movie already
// uses for its general-purpose window/screen capture; see Movie's MovieVisualCapture.cpp,
// inspected but not modified or linked against here). This is a small, Station-local
// implementation of just the one case Station needs -- one fixed, always-valid window -- rather
// than a reuse of Movie's general multi-source capture subsystem, which is built for an open
// catalog of external windows/screens/devices that Station's Cast feature does not need.
//
// Runs its own capture thread with its own WinRT apartment; never touches the audio thread.
// Produces frames into a small bounded queue (see BoundedFrameQueue) so a slow consumer causes
// controlled frame dropping, never unbounded memory growth.
class StationWindowCapture
{
public:
    StationWindowCapture();
    ~StationWindowCapture();

    // Starts capturing the given native top-level window handle (an HWND on Windows, passed as
    // void* so this header doesn't need <windows.h>). Returns false, with nothing left running,
    // if Windows Graphics Capture isn't available on this machine/OS version -- callers must
    // treat that as "capture unavailable" and not proceed to encoding.
    bool start(void* nativeWindowHandle);

    // Stops capture and releases every OS/WinRT resource start() may have acquired. Safe to
    // call even if start() was never called or already failed/stopped.
    void stop();

    bool isRunning() const noexcept;

    // Pops the oldest captured frame not yet consumed, if any. Intended to be polled from a
    // dedicated encoder thread, never from the UI or audio thread.
    bool tryPopFrame(CapturedFrame& out);

    size_t getDroppedFrameCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE(StationWindowCapture)
};

} // namespace cs::casting
