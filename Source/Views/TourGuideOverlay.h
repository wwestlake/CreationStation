#pragma once

#include <JuceHeader.h>

class TourGuideOverlay final : public juce::Component
{
public:
    struct Step
    {
        juce::String title;
        juce::String body;
        std::function<juce::Rectangle<int>()> targetBounds;
        bool advanceOnTargetClick = true;
        bool drawConnector = true;
        juce::String nextButtonText;
        std::function<void()> onStepEntered;
    };

    TourGuideOverlay();

    void setSteps(std::vector<Step> newSteps);
    void start(int stepIndex = 0);
    void stop();
    bool isActive() const noexcept { return active; }
    int getCurrentStepIndex() const noexcept { return currentStepIndex; }

    std::function<void()> onFinished;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void goToStep(int stepIndex);
    void updateButtons();
    juce::Rectangle<int> getTargetBounds() const;
    juce::Rectangle<int> getInfoBounds() const;

    std::vector<Step> steps;
    juce::Label stepLabel;
    juce::Label titleLabel;
    juce::TextEditor bodyEditor;
    juce::TextButton backButton { "Back" };
    juce::TextButton nextButton { "Next" };
    juce::TextButton doneButton { "Done" };
    juce::TextButton skipButton { "Skip Tour" };
    int currentStepIndex = -1;
    bool active = false;
};
