#pragma once

#include <JuceHeader.h>

class AuthGateView final : public juce::Component
{
public:
    AuthGateView();

    void setStatusText(const juce::String& text);
    void setAccountText(const juce::String& text);
    void setBusy(bool shouldBeBusy);

    std::function<void()> onSignInRequested;
    std::function<void()> onLogoutRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label accountLabel;
    juce::TextEditor statusEditor;
    juce::TextButton signInButton { "Sign In" };
    juce::TextButton logoutButton { "Clear Session" };
    juce::ProgressBar progress { progressValue };
    double progressValue = 0.0;
};
