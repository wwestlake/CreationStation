#include "PatinaIr.h"

namespace cw::patina::ir
{
juce::String toString(Domain domain)
{
    switch (domain)
    {
        case Domain::audio: return "audio";
        case Domain::control: return "control";
        case Domain::event: return "event";
        case Domain::worker: return "worker";
    }

    return "unknown";
}

juce::String describeValueRef(const ValueRef& valueRef)
{
    switch (valueRef.kind)
    {
        case ValueRef::Kind::literal:
            return valueRef.literalValue.toString();
        case ValueRef::Kind::graphParameter:
            return "$param(" + valueRef.referenceName + ")";
        case ValueRef::Kind::localConstant:
            return "$let(" + valueRef.referenceName + ")";
        case ValueRef::Kind::symbolic:
            return valueRef.referenceName;
    }

    return {};
}
} // namespace cw::patina::ir
