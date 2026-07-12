#pragma once

#include <JuceHeader.h>

namespace branding
{
juce::Image createCreationStationLogoImage(int size);
juce::Image createCreationStationSplashImage();
juce::Image createPatreonBadgeImage(const juce::String& tierId, int size);
juce::String getPatreonTierDisplayName(const juce::String& tierId);
juce::String getBestPatreonTierId(const juce::StringArray& entitlements);
}
