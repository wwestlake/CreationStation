#include "RouterEndpointRegistry.h"

RouterEndpointRegistry::RouterEndpointRegistry()
{
    endpoints.add({ EndpointRole::capture, djehuti::router::contract::captureEndpoint, "DjeRoute Capture", "OBS and recording apps" });
    endpoints.add({ EndpointRole::monitor, djehuti::router::contract::monitorEndpoint, "DjeRoute Monitor", "Speakers and headphones" });
    endpoints.add({ EndpointRole::mic, djehuti::router::contract::micEndpoint, "DjeRoute Mic", "Microphone workflows" });
    endpoints.add({ EndpointRole::instrument, djehuti::router::contract::instrumentEndpoint, "DjeRoute Instrument", "Guitar and line input workflows" });
}

juce::String RouterEndpointRegistry::getDisplayName(EndpointRole role) const
{
    for (const auto& endpoint : endpoints)
        if (endpoint.role == role)
            return endpoint.name;

    return {};
}

juce::String RouterEndpointRegistry::getShortName(EndpointRole role) const
{
    for (const auto& endpoint : endpoints)
        if (endpoint.role == role)
            return endpoint.shortName;

    return {};
}

juce::String RouterEndpointRegistry::getPurpose(EndpointRole role) const
{
    for (const auto& endpoint : endpoints)
        if (endpoint.role == role)
            return endpoint.purpose;

    return {};
}

juce::StringArray RouterEndpointRegistry::getRoleNames() const
{
    juce::StringArray names;

    for (const auto& endpoint : endpoints)
        names.add(endpoint.name);

    return names;
}
