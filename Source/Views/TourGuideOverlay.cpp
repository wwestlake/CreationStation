#include "TourGuideOverlay.h"

TourGuideOverlay::TourGuideOverlay()
{
    setInterceptsMouseClicks(true, true);
    setAlwaysOnTop(true);

    stepLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(stepLabel);

    titleLabel.setFont(juce::Font(22.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    bodyEditor.setMultiLine(true);
    bodyEditor.setReadOnly(true);
    bodyEditor.setCaretVisible(false);
    bodyEditor.setPopupMenuEnabled(false);
    bodyEditor.setScrollbarsShown(true);
    bodyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff151b23));
    bodyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2c394c));
    bodyEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd7deea));
    addAndMakeVisible(bodyEditor);

    backButton.onClick = [this]
    {
        goToStep(currentStepIndex - 1);
    };
    addAndMakeVisible(backButton);

    nextButton.onClick = [this]
    {
        goToStep(currentStepIndex + 1);
    };
    addAndMakeVisible(nextButton);

    doneButton.onClick = [this]
    {
        stop();
    };
    addAndMakeVisible(doneButton);

    skipButton.onClick = [this]
    {
        stop();
    };
    addAndMakeVisible(skipButton);

    setVisible(false);
}

void TourGuideOverlay::setSteps(std::vector<Step> newSteps)
{
    steps = std::move(newSteps);
    if (active && ! steps.empty())
        goToStep(juce::jlimit(0, (int) steps.size() - 1, currentStepIndex));
}

void TourGuideOverlay::start(int stepIndex)
{
    if (steps.empty())
        return;

    active = true;
    setVisible(true);
    goToStep(stepIndex);
    toFront(true);
    repaint();
}

void TourGuideOverlay::stop()
{
    active = false;
    currentStepIndex = -1;
    setVisible(false);
    repaint();

    if (onFinished)
        onFinished();
}

void TourGuideOverlay::goToStep(int stepIndex)
{
    if (steps.empty())
        return;

    currentStepIndex = juce::jlimit(0, (int) steps.size() - 1, stepIndex);
    const auto& step = steps[(size_t) currentStepIndex];
    stepLabel.setText("Step " + juce::String(currentStepIndex + 1) + " of " + juce::String((int) steps.size()), juce::dontSendNotification);
    titleLabel.setText(step.title, juce::dontSendNotification);
    bodyEditor.setText(step.body, juce::dontSendNotification);
    nextButton.setButtonText(step.nextButtonText.isNotEmpty() ? step.nextButtonText : "Next");
    updateButtons();
    if (step.onStepEntered)
        step.onStepEntered();
    repaint();
}

void TourGuideOverlay::updateButtons()
{
    backButton.setEnabled(currentStepIndex > 0);
    nextButton.setVisible(currentStepIndex < (int) steps.size() - 1);
    doneButton.setVisible(currentStepIndex >= (int) steps.size() - 1);
}

juce::Rectangle<int> TourGuideOverlay::getTargetBounds() const
{
    if (! active || ! juce::isPositiveAndBelow(currentStepIndex, (int) steps.size()))
        return {};

    if (steps[(size_t) currentStepIndex].targetBounds)
        return steps[(size_t) currentStepIndex].targetBounds();

    return {};
}

juce::Rectangle<int> TourGuideOverlay::getInfoBounds() const
{
    return getLocalBounds().withSizeKeepingCentre(620, 200).translated(0, 180);
}

void TourGuideOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xaa05070a));

    auto target = getTargetBounds().expanded(8);
    if (target.isEmpty())
        target = getLocalBounds().withSizeKeepingCentre(520, 320).translated(0, -40);

    g.setColour(juce::Colour(0x405f93ff));
    g.fillRoundedRectangle(target.toFloat(), 18.0f);
    g.setColour(juce::Colour(0xff8fd3ff));
    g.drawRoundedRectangle(target.toFloat(), 18.0f, 3.0f);

    auto infoArea = getInfoBounds();
    g.setColour(juce::Colour(0xff121821));
    g.fillRoundedRectangle(infoArea.toFloat(), 20.0f);
    g.setColour(juce::Colour(0xff2d3949));
    g.drawRoundedRectangle(infoArea.toFloat(), 20.0f, 1.0f);

    if (active && juce::isPositiveAndBelow(currentStepIndex, (int) steps.size()))
    {
        const auto& step = steps[(size_t) currentStepIndex];
        if (step.drawConnector)
        {
            juce::Point<float> source((float) infoArea.getCentreX(), (float) infoArea.getY());
            juce::Point<float> destination((float) target.getCentreX(), (float) target.getBottom());
            auto distance = std::abs(source.y - destination.y);

            juce::Path connector;
            connector.startNewSubPath(source);
            connector.cubicTo(source.x, source.y - distance * 0.22f,
                              destination.x, destination.y + distance * 0.22f,
                              destination.x, destination.y);

            g.setColour(juce::Colour(0xff8fd3ff).withAlpha(0.95f));
            g.strokePath(connector, juce::PathStrokeType(3.0f));
            g.drawLine(destination.x - 8.0f, destination.y - 10.0f, destination.x, destination.y, 3.0f);
            g.drawLine(destination.x + 8.0f, destination.y - 10.0f, destination.x, destination.y, 3.0f);
        }
    }
}

void TourGuideOverlay::resized()
{
    auto infoArea = getInfoBounds().reduced(18);
    stepLabel.setBounds(infoArea.removeFromTop(20));
    titleLabel.setBounds(infoArea.removeFromTop(34));
    bodyEditor.setBounds(infoArea.removeFromTop(88));

    auto buttonRow = infoArea.removeFromBottom(32);
    skipButton.setBounds(buttonRow.removeFromRight(108));
    doneButton.setBounds(buttonRow.removeFromRight(88));
    nextButton.setBounds(buttonRow.removeFromRight(88));
    backButton.setBounds(buttonRow.removeFromRight(88));
}

void TourGuideOverlay::mouseDown(const juce::MouseEvent& event)
{
    auto target = getTargetBounds().expanded(8);
    if (target.contains(event.getPosition()))
    {
        const auto& step = steps[(size_t) currentStepIndex];
        if (step.advanceOnTargetClick)
            goToStep(currentStepIndex + 1);
        return;
    }
}
