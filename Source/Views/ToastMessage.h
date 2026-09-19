#pragma once

#include <JuceHeader.h>

// A short, readable message that appears near the bottom of the window and clears itself after a few
// seconds (or on a click). It replaces the small status label in the header bar for anything that is not
// an error: it is large enough to read and impossible to miss, and it goes away by itself. Errors never
// come here - they get a dialog.
class ToastMessage final : public juce::Component, private juce::Timer
{
public:
    ToastMessage()
    {
        setInterceptsMouseClicks(true, false);
        setVisible(false);
    }

    // How tall the message needs to be to show `text` at `width`.
    static int preferredHeight(const juce::String& text, int width)
    {
        return (int) std::ceil(makeLayout(text, width - 32).getHeight()) + 28;
    }

    void show(const juce::String& text)
    {
        message = text;
        setVisible(true);
        toFront(false);
        repaint();
        startTimer(6000);
    }

    void dismiss()
    {
        stopTimer();
        setVisible(false);
    }

    const juce::String& getMessage() const noexcept { return message; }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xf2161e2b));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colour(0xff3a4a63));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        makeLayout(message, getWidth() - 32).draw(g, juce::Rectangle<float>(16.0f, 14.0f, (float) getWidth() - 32.0f, (float) getHeight() - 28.0f));
    }

    void mouseDown(const juce::MouseEvent&) override { dismiss(); }

private:
    static juce::TextLayout makeLayout(const juce::String& text, int width)
    {
        juce::AttributedString attributed;
        attributed.setJustification(juce::Justification::centred);
        attributed.append(text, juce::Font(juce::FontOptions(15.0f)), juce::Colour(0xffe6edf7));

        juce::TextLayout layout;
        layout.createLayout(attributed, (float) juce::jmax(50, width));
        return layout;
    }

    void timerCallback() override { dismiss(); }

    juce::String message;
};
