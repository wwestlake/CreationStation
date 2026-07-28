#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <functional>

namespace cs::plugins
{
// A vertical dBFS input meter with a draggable threshold line on the same scale, so the user
// sets the compressor threshold visually against the live signal (classic compressor workflow).
class ThresholdMeterComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    ThresholdMeterComponent();
    ~ThresholdMeterComponent() override;

    void setLevelSource(const std::atomic<float>* newLevelSource);
    void setThresholdDb(float db);                 // sync the line from the parameter (no callback)
    float getThresholdDb() const noexcept { return thresholdDb; }

    std::function<void(float db)> onThresholdChanged; // fired while the user drags the line

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    float yToDb(int y) const;
    float dbToY(float db) const;

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;

    const std::atomic<float>* levelSource = nullptr;
    float displayedLevelDb = minDb;
    float thresholdDb = -18.0f;
};
}
