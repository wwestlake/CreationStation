#include "GraphPanel.h"

namespace
{
juce::Rectangle<float> makeNodeBounds(int index, float x, float y)
{
    return { x + index * 220.0f, y + (index % 2) * 90.0f, 180.0f, 110.0f };
}
}

GraphPanel::GraphPanel()
{
    headerLabel.setText("Node Graph", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(juce::FontOptions(24.0f)).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    nodeNames.addArray({ "Input", "EQ", "Drive", "Space", "Output" });
}

void GraphPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff13171d));

    auto bounds = getLocalBounds().reduced(20);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    for (int x = bounds.getX(); x < bounds.getRight(); x += 28)
        g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += 28)
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));

    auto originY = static_cast<float>(bounds.getY() + 90);
    auto originX = static_cast<float>(bounds.getX() + 20);

    for (int i = 0; i < nodeNames.size(); ++i)
    {
        auto node = makeNodeBounds(i, originX, originY);
        g.setColour(juce::Colour(0xff243041));
        g.fillRoundedRectangle(node, 14.0f);
        g.setColour(juce::Colour(0xff79c0ff));
        g.drawRoundedRectangle(node, 14.0f, 2.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(20.0f)).boldened());
        g.drawText(nodeNames[i], node.toNearestInt().reduced(12), juce::Justification::topLeft, false);
        g.setFont(juce::Font(juce::FontOptions(15.0f)));
        g.drawText("DSP block", node.toNearestInt().reduced(12, 38), juce::Justification::topLeft, false);

        if (i > 0)
        {
            auto previous = makeNodeBounds(i - 1, originX, originY);
            g.setColour(juce::Colour(0xff9aa4b2).withAlpha(0.65f));
            g.drawLine(previous.getRight(), previous.getCentreY(), node.getX(), node.getCentreY(), 2.0f);
        }
    }
}

void GraphPanel::resized()
{
    headerLabel.setBounds(getLocalBounds().reduced(20).removeFromTop(40));
}
