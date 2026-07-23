#include "LevelMeterComponent.h"
#include "CreationStationPluginPalette.h"

namespace cs::plugins
{

LevelMeterComponent::LevelMeterComponent()
{
    startTimerHz(30);
}

LevelMeterComponent::~LevelMeterComponent()
{
    stopTimer();
}

void LevelMeterComponent::setLevelSource(const std::atomic<float>* newLevelSource, float minDb, float maxDb)
{
    levelSource = newLevelSource;
    minDecibels = minDb;
    maxDecibels = maxDb;
}

void LevelMeterComponent::timerCallback()
{
    if (levelSource == nullptr)
        return;

    auto db = juce::jlimit(minDecibels, maxDecibels, levelSource->load());
    auto normalised = (db - minDecibels) / juce::jmax(0.001f, maxDecibels - minDecibels);

    if (! juce::approximatelyEqual(normalised, displayedLevel))
    {
        displayedLevel = juce::jlimit(0.0f, 1.0f, normalised);
        repaint();
    }
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(palette::knobTrack);
    g.fillRoundedRectangle(bounds, 3.0f);

    auto fillBounds = bounds.removeFromBottom(bounds.getHeight() * displayedLevel);
    g.setColour(displayedLevel > 0.85f ? palette::meterHot : palette::meterOk);
    g.fillRoundedRectangle(fillBounds, 3.0f);

    g.setColour(palette::outline);
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.0f, 1.0f);
}

}
