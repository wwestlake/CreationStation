#pragma once

#include <JuceHeader.h>

#include <functional>

// Station's own message-box parts. Each app owns its AI panel; nothing here is shared with the suite.
namespace station_ui
{
// The round button that sits inside the right edge of the assistant's message box. What it shows
// depends on what pressing it will do, which the host sets as the assistant's state changes: an up
// arrow to send, a square to stop a running request. More icons are added here as the assistant's
// process grows modes.
class SendArrowButton final : public juce::Button
{
public:
    enum class Icon
    {
        send,
        stop
    };

    SendArrowButton() : juce::Button("Send") {}

    void setIcon(Icon newIcon)
    {
        if (icon == newIcon) return;
        icon = newIcon;
        repaint();
    }
    Icon getIcon() const noexcept { return icon; }

    void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const bool live = isEnabled();

        auto fill = live ? juce::Colour(0xff3d7be0) : juce::Colour(0xff3a4658);
        if (live && isButtonDown) fill = fill.darker(0.25f);
        else if (live && isMouseOver) fill = fill.brighter(0.15f);
        g.setColour(fill);
        g.fillEllipse(bounds);

        const auto c = bounds.getCentre();
        const float r = bounds.getWidth() * 0.26f;
        g.setColour(juce::Colours::white.withAlpha(live ? 1.0f : 0.5f));

        if (icon == Icon::stop)
        {
            g.fillRoundedRectangle(c.x - r * 0.8f, c.y - r * 0.8f, r * 1.6f, r * 1.6f, 2.0f);
            return;
        }

        // An up arrow: a stem and a head.
        juce::Path arrow;
        arrow.startNewSubPath(c.x, c.y + r);
        arrow.lineTo(c.x, c.y - r);
        arrow.startNewSubPath(c.x - r * 0.8f, c.y - r * 0.15f);
        arrow.lineTo(c.x, c.y - r);
        arrow.lineTo(c.x + r * 0.8f, c.y - r * 0.15f);
        g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    Icon icon = Icon::send;
};

// A multi-line message box that can send on Enter. When enterSends is on, Enter calls onSend and
// Shift+Enter starts a new line; when it is off, Enter starts a new line and Ctrl+Enter sends.
class PromptEditor final : public juce::TextEditor
{
public:
    bool enterSends = false;
    std::function<void()> onSend;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::returnKey)
        {
            const bool shift = key.getModifiers().isShiftDown();
            const bool ctrl = key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown();
            const bool send = enterSends ? (! shift) : ctrl;
            if (send)
            {
                if (onSend) onSend();
                return true;
            }
            insertTextAtCaret("\n");
            return true;
        }
        return juce::TextEditor::keyPressed(key);
    }
};
}
