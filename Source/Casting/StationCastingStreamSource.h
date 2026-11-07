#pragma once

#include <atomic>
#include <thread>
#include <juce_gui_basics/juce_gui_basics.h>
#include "CastingMp4Sink.h"
#include "CastingStreamServer.h"
#include "CastingStreamSource.h"
#include "StationWindowCapture.h"

namespace cs::casting
{

// The real CastingStreamSource: captures the given window (Station's own top-level window),
// encodes it live, and serves it over a local HTTP endpoint -- wiring StationWindowCapture,
// CastingMp4Sink, and CastingStreamServer together behind the one start()/stop() pair
// CastingSessionController needs. Owns a single dedicated encode thread; never touches the
// audio thread or blocks the UI thread.
class StationCastingStreamSource : public CastingStreamSource
{
public:
    // windowToCapture must outlive this object, and must already be showing on screen (have a
    // native peer) by the time start() is called.
    explicit StationCastingStreamSource(juce::Component& windowToCapture);
    ~StationCastingStreamSource() override;

    juce::String start() override;
    void stop() override;

    size_t getDroppedCaptureFrameCount() const { return capture.getDroppedFrameCount(); }

private:
    void encodeLoop();

    juce::Component& window;
    StationWindowCapture capture;
    CastingMp4Sink encoder;
    CastingStreamServer server;
    std::thread encodeThread;
    std::atomic<bool> stopRequested { false };
    bool running = false;

    JUCE_DECLARE_NON_COPYABLE(StationCastingStreamSource)
};

} // namespace cs::casting
