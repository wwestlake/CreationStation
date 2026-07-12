#include "AuthGateView.h"

AuthGateView::AuthGateView()
{
    titleLabel.setText("Sign in to Creation Station", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f)).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Use your LagDaemon account to unlock the app.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    accountLabel.setText("Not signed in yet.", juce::dontSendNotification);
    accountLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(accountLabel);

    statusEditor.setMultiLine(true);
    statusEditor.setReadOnly(true);
    statusEditor.setScrollbarsShown(false);
    statusEditor.setCaretVisible(false);
    statusEditor.setPopupMenuEnabled(false);
    statusEditor.setText("Waiting for you to start the sign-in flow.");
    statusEditor.setFont(juce::Font(juce::FontOptions(15.0f)));
    statusEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffaab6c9));
    statusEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    statusEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    statusEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(statusEditor);

    signInButton.onClick = [this]
    {
        if (onSignInRequested != nullptr)
            onSignInRequested();
    };
    addAndMakeVisible(signInButton);

    logoutButton.onClick = [this]
    {
        if (onLogoutRequested != nullptr)
            onLogoutRequested();
    };
    addAndMakeVisible(logoutButton);

    addAndMakeVisible(progress);
}

void AuthGateView::setStatusText(const juce::String& text)
{
    statusEditor.setText(text, juce::dontSendNotification);
}

void AuthGateView::setAccountText(const juce::String& text)
{
    accountLabel.setText(text, juce::dontSendNotification);
}

void AuthGateView::setBusy(bool shouldBeBusy)
{
    progressValue = shouldBeBusy ? 0.55 : 0.0;
    signInButton.setEnabled(! shouldBeBusy);
}

void AuthGateView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));

    auto card = getLocalBounds().toFloat().reduced(120.0f, 100.0f);
    g.setColour(juce::Colour(0xff171d27));
    g.fillRoundedRectangle(card, 20.0f);
    g.setColour(juce::Colour(0xff2d384a));
    g.drawRoundedRectangle(card, 20.0f, 1.0f);
}

void AuthGateView::resized()
{
    auto area = getLocalBounds().reduced(150, 130);
    auto card = area.reduced(40, 40);

    titleLabel.setBounds(card.removeFromTop(46));
    subtitleLabel.setBounds(card.removeFromTop(28));
    card.removeFromTop(16);
    accountLabel.setBounds(card.removeFromTop(26));
    auto statusArea = card.removeFromTop(54);
    statusEditor.setBounds(statusArea);
    card.removeFromTop(12);
    progress.setBounds(card.removeFromTop(18));
    card.removeFromTop(18);
    signInButton.setBounds(card.removeFromLeft(150));
    card.removeFromLeft(12);
    logoutButton.setBounds(card.removeFromLeft(150));
}
