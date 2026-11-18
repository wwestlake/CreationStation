#include "CastingSessionController.h"

namespace cs::casting
{

CastingSessionController::CastingSessionController(CastingBackend& backendIn, CastingStreamSource& streamSourceIn)
    : backend(backendIn), streamSource(streamSourceIn)
{
    backend.onDeviceSelected = [this](const CastDevice& device) { handleDeviceSelected(device); };
    backend.onStateChanged = [this](CastState newState) { handleBackendStateChanged(newState); };
    backend.onError = [this](const CastError& error) { handleBackendError(error); };
}

CastingSessionController::~CastingSessionController()
{
    // Always tear down fully on destruction, regardless of what state we were in -- a crash or
    // an app-shutdown path must never leave capture/encoder/server resources running.
    backend.onDeviceSelected = nullptr;
    backend.onStateChanged = nullptr;
    backend.onError = nullptr;
    backend.stopCasting();
    releaseStreamSourceIfRunning();
}

void CastingSessionController::beginCast(juce::Rectangle<int> anchorScreenBounds)
{
    if (state != CastState::idle && state != CastState::disconnected && state != CastState::failed)
        return; // Already searching/connecting/connected -- ignore a confused repeat click.

    connectedDevice = {};
    backend.beginDiscovery(anchorScreenBounds);
}

void CastingSessionController::disconnect()
{
    if (state == CastState::idle)
        return;
    backend.stopCasting();
}

void CastingSessionController::handleDeviceSelected(const CastDevice& device)
{
    connectedDevice = device;

    const auto streamUrl = streamSource.start();
    if (streamUrl.isEmpty())
    {
        // The stream source itself failed (capture/encoder/server couldn't start) before we
        // ever asked the backend to connect to anything -- nothing to disconnect there.
        connectedDevice = {};
        handleBackendError({ CastErrorCode::captureUnavailable, "Couldn't start the local video stream." });
        handleBackendStateChanged(CastState::failed);
        return;
    }

    streamSourceRunning = true;
    backend.startCasting(streamUrl);
}

void CastingSessionController::handleBackendStateChanged(CastState backendState)
{
    state = backendState;

    if (backendState == CastState::disconnected || backendState == CastState::failed)
    {
        releaseStreamSourceIfRunning();
        connectedDevice = {};
    }

    if (onStateChanged)
        onStateChanged(state);
}

void CastingSessionController::handleBackendError(const CastError& error)
{
    if (onError)
        onError(error);
}

void CastingSessionController::releaseStreamSourceIfRunning()
{
    if (! streamSourceRunning)
        return;
    streamSourceRunning = false;
    streamSource.stop();
}

} // namespace cs::casting
