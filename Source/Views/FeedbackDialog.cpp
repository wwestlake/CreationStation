#include "FeedbackDialog.h"

FeedbackDialog::FeedbackDialog()
{
    titleLabel.setText("Feedback & Privacy", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    privacyIntroLabel.setText("Both of these are opt-in and off by default. Feedback you write is sent when you press Send; "
                              "usage metrics (feature usage, crashes, performance) are only collected in the background "
                              "while that toggle is on, and you can turn either off at any time.",
                              juce::dontSendNotification);
    privacyIntroLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ba1bc));
    privacyIntroLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(privacyIntroLabel);

    feedbackOptInToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    feedbackOptInToggle.onClick = [this]
    {
        updateSubmitEnablement();
        if (onOptInsChanged != nullptr)
            onOptInsChanged(feedbackOptInToggle.getToggleState(), metricsOptInToggle.getToggleState());
    };
    addAndMakeVisible(feedbackOptInToggle);

    metricsOptInToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    metricsOptInToggle.onClick = [this]
    {
        if (onOptInsChanged != nullptr)
            onOptInsChanged(feedbackOptInToggle.getToggleState(), metricsOptInToggle.getToggleState());
    };
    addAndMakeVisible(metricsOptInToggle);

    metricsHintLabel.setText("Anonymous by default -- an install identifier, app version, and OS, never your files or content.",
                             juce::dontSendNotification);
    metricsHintLabel.setFont(juce::Font(13.0f));
    metricsHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff677b93));
    addAndMakeVisible(metricsHintLabel);

    categoryLabel.setText("Category", juce::dontSendNotification);
    categoryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(categoryLabel);

    categoryCombo.addItem("General", 1);
    categoryCombo.addItem("Bug report", 2);
    categoryCombo.addItem("Suggestion", 3);
    categoryCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(categoryCombo);

    messageLabel.setText("Message", juce::dontSendNotification);
    messageLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(messageLabel);

    messageEditor.setMultiLine(true);
    messageEditor.setReturnKeyStartsNewLine(true);
    messageEditor.setTextToShowWhenEmpty("What's working, what isn't, what you'd like to see...",
                                         juce::Colour(0xff677b93));
    messageEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff16202c));
    messageEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2d3e54));
    messageEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    messageEditor.onTextChange = [this] { updateSubmitEnablement(); };
    addAndMakeVisible(messageEditor);

    submitButton.onClick = [this]
    {
        if (onSubmitRequested != nullptr)
            onSubmitRequested(messageEditor.getText(), categoryCombo.getText());
    };
    addAndMakeVisible(submitButton);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ba1bc));
    addAndMakeVisible(statusLabel);

    updateSubmitEnablement();
}

void FeedbackDialog::setOptIns(bool feedbackOptIn, bool metricsOptIn)
{
    feedbackOptInToggle.setToggleState(feedbackOptIn, juce::dontSendNotification);
    metricsOptInToggle.setToggleState(metricsOptIn, juce::dontSendNotification);
    updateSubmitEnablement();
}

void FeedbackDialog::setSubmitInProgress(bool inProgress)
{
    submitButton.setEnabled(! inProgress);
    if (inProgress)
        statusLabel.setText("Sending...", juce::dontSendNotification);
}

void FeedbackDialog::setSubmitResult(bool success, const juce::String& message)
{
    statusLabel.setColour(juce::Label::textColourId, success ? juce::Colour(0xff7fe0a0) : juce::Colour(0xffff8f8f));
    statusLabel.setText(message, juce::dontSendNotification);
    if (success)
        messageEditor.clear();
    updateSubmitEnablement();
}

void FeedbackDialog::updateSubmitEnablement()
{
    submitButton.setEnabled(feedbackOptInToggle.getToggleState() && messageEditor.getText().trim().isNotEmpty());
}

void FeedbackDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11151c));
}

void FeedbackDialog::resized()
{
    auto area = getLocalBounds().reduced(20);

    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(6);
    privacyIntroLabel.setBounds(area.removeFromTop(48));
    area.removeFromTop(14);

    feedbackOptInToggle.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);
    metricsOptInToggle.setBounds(area.removeFromTop(26));
    metricsHintLabel.setBounds(area.removeFromTop(20).withTrimmedLeft(24));
    area.removeFromTop(16);

    auto categoryRow = area.removeFromTop(26);
    categoryLabel.setBounds(categoryRow.removeFromLeft(80));
    categoryCombo.setBounds(categoryRow.removeFromLeft(200));
    area.removeFromTop(10);

    messageLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);

    auto bottomRow = area.removeFromBottom(26);
    statusLabel.setBounds(bottomRow.removeFromLeft(bottomRow.getWidth() - 140));
    submitButton.setBounds(bottomRow.removeFromRight(130));
    area.removeFromBottom(10);

    messageEditor.setBounds(area);
}
