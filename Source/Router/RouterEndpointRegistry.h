#pragma once

#include <JuceHeader.h>
#include "RouterDriverContract.h"

class RouterEndpointRegistry final
{
public:
    enum class EndpointRole
    {
        capture,
        monitor,
        mic,
        instrument
    };

    struct EndpointDescriptor
    {
        EndpointRole role = EndpointRole::capture;
        juce::String name;
        juce::String shortName;
        juce::String purpose;
    };

    RouterEndpointRegistry();

    const juce::Array<EndpointDescriptor>& getEndpoints() const noexcept { return endpoints; }
    juce::String getDisplayName(EndpointRole role) const;
    juce::String getShortName(EndpointRole role) const;
    juce::String getPurpose(EndpointRole role) const;
    juce::StringArray getRoleNames() const;

private:
    juce::Array<EndpointDescriptor> endpoints;
};
