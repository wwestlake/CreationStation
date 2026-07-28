#include "ThresholdMeterComponent.h"
#include "CreationStationPluginPalette.h"

namespace cs::plugins
{

ThresholdMeterComponent::ThresholdMeterComponent()
{
    startTimerHz(30);
}

ThresholdMeterComponent::~ThresholdMeterComponent()
{
    stopTimer();
}

void ThresholdMeterComponent::setLevelSource(const std::atomic<float>* newLevelSource)
{
    levelSource = newLevelSource;
}

void ThresholdMeterComponent::setThresholdDb(float db)
{
    auto clamped = juce::jlimit(minDb, maxDb, db);
    if (! juce::approximatelyEqual(clamped, thresholdDb))
    {
        thresholdDb = clamped;
        repaint();
    }
}

float ThresholdMeterComponent::yToDb(int y) const
{
    auto proportion = 1.0f - juce::jlimit(0.0f, 1.0f, (float) y / juce::jmax(1.0f, (float) getHeight()));
    return minDb + proportion * (maxDb - minDb);
}

float ThresholdMeterComponent::dbToY(float db) const
{
    auto proportion = (juce::jlimit(minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return (1.0f - proportion) * (float) getHeight();
}

void ThresholdMeterComponent::timerCallback()
{
    if (levelSource == nullptr)
        return;

    auto db = juce::jlimit(minDb, maxDb, levelSource->load());

    // Peak-style ballistics: jump up instantly, ease back down so strums are readable.
    auto target = db;
    displayedLevelDb = target > displayedLevelDb ? target
                                                 : displayedLevelDb * 0.8f + target * 0.2f;
    repaint();
}

void ThresholdMeterComponent::mouseDown(const juce::MouseEvent& event)
{
    setThresholdDb(yToDb(event.y));
    if (onThresholdChanged)
        onThresholdChanged(thresholdDb);
}

void ThresholdMeterComponent::mouseDrag(const juce::MouseEvent& event)
{
    setThresholdDb(yToDb(event.y));
    if (onThresholdChanged)
        onThresholdChanged(thresholdDb);
}

void ThresholdMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(palette::knobTrack);
    g.fillRoundedRectangle(bounds, 3.0f);

    // dB tick marks (every 12 dB) with labels.
    g.setFont(juce::Font(9.0f));
    for (int db = 0; db >= (int) minDb; db -= 12)
    {
        auto y = dbToY((float) db);
        g.setColour(palette::outline);
        g.drawHorizontalLine((int) y, bounds.getX(), bounds.getRight());
        g.setColour(palette::textSecondary);
        g.drawText(juce::String(db), (int) bounds.getX() + 2, (int) y + 1, 26, 10, juce::Justification::topLeft);
    }

    // Signal fill from the bottom up to the current level.
    auto fillTop = dbToY(displayedLevelDb);
    auto fillBounds = juce::Rectangle<float>(bounds.getX(), fillTop, bounds.getWidth(), bounds.getBottom() - fillTop);
    g.setColour(displayedLevelDb > -6.0f ? palette::meterHot : palette::meterOk);
    g.fillRect(fillBounds.reduced(1.0f, 0.0f));

    // Threshold line + handle.
    auto thresholdY = dbToY(thresholdDb);
    g.setColour(palette::accent);
    g.fillRect(bounds.getX(), thresholdY - 1.0f, bounds.getWidth(), 2.0f);
    g.fillRoundedRectangle(bounds.getRight() - 10.0f, thresholdY - 5.0f, 10.0f, 10.0f, 2.0f);
    g.setColour(palette::textPrimary);
    g.setFont(juce::Font(10.0f).boldened());
    g.drawText(juce::String(juce::roundToInt(thresholdDb)) + " dB",
               (int) bounds.getX() + 2, (int) thresholdY - 14, (int) bounds.getWidth() - 4, 12,
               juce::Justification::centredRight);

    g.setColour(palette::outline);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

}
