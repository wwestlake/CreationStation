#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>

namespace cs::plugins
{
class LevelMeterComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override;

    // The source is read each timer tick and mapped from [minDb, maxDb] to a 0..1 fill,
    // higher value = more fill. For a gain-reduction meter, pass a source that reports
    // reduction as a positive dB magnitude (0 = no reduction).
    void setLevelSource(const std::atomic<float>* newLevelSource, float minDb, float maxDb);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    const std::atomic<float>* levelSource = nullptr;
    float minDecibels = -24.0f;
    float maxDecibels = 0.0f;
    float displayedLevel = 0.0f;
};
}
