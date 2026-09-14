#pragma once

#include <JuceHeader.h>

// Content component for the "Send Feedback" popup window (MainComponent owns
// the actual juce::DocumentWindow, matching how every other popup panel in
// this app works -- see showTrackFxStackWindow/showMidiEditorWindow). This
// component only presents the two opt-in toggles and the feedback form; it
// never talks to the network itself -- MainComponent does the actual POST
// (on a background thread) in response to onSubmitRequested, then calls
// setSubmitResult() to report back.
class FeedbackDialog final : public juce::Component
{
public:
    FeedbackDialog();

    void setOptIns(bool feedbackOptIn, bool metricsOptIn);
    void setSubmitInProgress(bool inProgress);
    void setSubmitResult(bool success, const juce::String& message);

    std::function<void(bool feedbackOptIn, bool metricsOptIn)> onOptInsChanged;
    std::function<void(const juce::String& message, const juce::String& category)> onSubmitRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateSubmitEnablement();

    juce::Label titleLabel;
    juce::Label privacyIntroLabel;
    juce::ToggleButton feedbackOptInToggle { "Send written feedback (opt-in)" };
    juce::ToggleButton metricsOptInToggle { "Share anonymous usage metrics (opt-in)" };
    juce::Label metricsHintLabel;

    juce::Label categoryLabel;
    juce::ComboBox categoryCombo;
    juce::Label messageLabel;
    juce::TextEditor messageEditor;
    juce::TextButton submitButton { "Send Feedback" };
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeedbackDialog)
};
