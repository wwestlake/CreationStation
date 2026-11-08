#include "CastingTypes.h"

namespace cs::casting
{

juce::String describeCastError(const CastError& error)
{
    juce::String base;
    switch (error.code)
    {
        case CastErrorCode::none:                 return {};
        case CastErrorCode::noReceiverSelected:    base = "No device was selected."; break;
        case CastErrorCode::castingApiUnavailable: base = "Wireless casting isn't available on this PC."; break;
        case CastErrorCode::captureUnavailable:    base = "Couldn't capture the Station window."; break;
        case CastErrorCode::encoderUnavailable:    base = "Couldn't start a video encoder for casting."; break;
        case CastErrorCode::streamServerFailed:    base = "Couldn't open a local network stream for casting."; break;
        case CastErrorCode::connectionRejected:    base = "The device declined the casting connection."; break;
        case CastErrorCode::receiverStalled:       base = "The device stopped responding and was disconnected."; break;
        case CastErrorCode::internalError:         base = "Casting stopped because of an internal error."; break;
    }
    if (error.detail.isNotEmpty())
        return base + " (" + error.detail + ")";
    return base;
}

} // namespace cs::casting
