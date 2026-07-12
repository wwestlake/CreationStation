#include "GraphPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff13171d); }
juce::Colour cardColour() { return juce::Colour(0xff202838); }
juce::Colour accentColour() { return juce::Colour(0xff79c0ff); }

void drawSourceIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    g.fillRoundedRectangle(area.reduced(3.0f), 6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawLine(area.getX() + 5.0f, area.getCentreY(), area.getRight() - 8.0f, area.getCentreY(), 2.0f);
    g.drawEllipse(area.getCentreX() - 5.0f, area.getCentreY() - 5.0f, 10.0f, 10.0f, 2.0f);
}

void drawEffectIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    g.drawRoundedRectangle(area.reduced(4.0f), 10.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    auto midY = area.getCentreY();
    g.drawLine(area.getX() + 7.0f, midY, area.getRight() - 7.0f, midY, 2.0f);
    g.drawLine(area.getCentreX(), area.getY() + 7.0f, area.getCentreX(), area.getBottom() - 7.0f, 2.0f);
}

void drawSinkIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    g.fillRoundedRectangle(area.reduced(4.0f), 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawLine(area.getX() + 8.0f, area.getY() + 10.0f, area.getRight() - 8.0f, area.getY() + 10.0f, 2.0f);
    g.drawLine(area.getX() + 8.0f, area.getY() + 18.0f, area.getRight() - 14.0f, area.getY() + 18.0f, 2.0f);
}
}

GraphPanel::NodeCard::NodeCard(const cw::NodeTemplate& templateData)
    : nodeTemplate(templateData)
{
    title.setText(templateData.name, juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    subtitle.setText(templateData.description, juce::dontSendNotification);
    subtitle.setJustificationType(juce::Justification::centredLeft);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitle);

    categoryLabel.setText(cw::nodeCategoryName(templateData.category), juce::dontSendNotification);
    categoryLabel.setJustificationType(juce::Justification::centredRight);
    categoryLabel.setColour(juce::Label::textColourId, cw::nodeCategoryColour(templateData.category));
    addAndMakeVisible(categoryLabel);

    amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    amountSlider.setRange(0.0, 1.0, 0.001);
    amountSlider.setValue(templateData.defaultValue);
    amountSlider.onValueChange = [this]
    {
        setAmount(static_cast<float>(amountSlider.getValue()));
    };
    addAndMakeVisible(amountSlider);

    enableToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(enableToggle);
}

void GraphPanel::NodeCard::setAmount(float newAmount)
{
    amount = juce::jlimit(0.0f, 1.0f, newAmount);
    amountSlider.setValue(amount, juce::dontSendNotification);
}

void GraphPanel::NodeCard::setEnabled(bool shouldEnable)
{
    enabled = shouldEnable;
    enableToggle.setToggleState(enabled, juce::dontSendNotification);
}

void GraphPanel::NodeCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto accent = cw::nodeCategoryColour(nodeTemplate.category);
    g.setColour(cardColour().withAlpha(enabled ? 0.95f : 0.6f));
    g.fillRoundedRectangle(bounds.reduced(3.0f), 14.0f);
    g.setColour(enabled ? accent : juce::Colour(0xff4d5a6d));
    g.drawRoundedRectangle(bounds.reduced(3.0f), 14.0f, 2.0f);

    auto iconArea = getLocalBounds().reduced(14).removeFromTop(26).removeFromLeft(26);
    switch (nodeTemplate.category)
    {
        case cw::NodeCategory::source: drawSourceIcon(g, iconArea.toFloat(), accent); break;
        case cw::NodeCategory::effect: drawEffectIcon(g, iconArea.toFloat(), accent); break;
        case cw::NodeCategory::sink: drawSinkIcon(g, iconArea.toFloat(), accent); break;
    }

    if (! enabled)
    {
        g.setColour(juce::Colour(0xff9aa4b2).withAlpha(0.5f));
        g.drawText("Bypassed", getLocalBounds().reduced(16), juce::Justification::bottomLeft, false);
    }
}

void GraphPanel::NodeCard::resized()
{
    auto area = getLocalBounds().reduced(14);
    enableToggle.setBounds(area.removeFromTop(24).removeFromRight(70));
    categoryLabel.setBounds(area.removeFromTop(20));
    title.setBounds(area.removeFromTop(24));
    subtitle.setBounds(area.removeFromTop(20));
    amountSlider.setBounds(area.removeFromBottom(44));
}

GraphPanel::GraphPanel()
{
    setName("Node Graph");
    headerLabel.setText("Sources → Effects → Sinks", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    subtitleLabel.setText("Build sound by patching live inputs, generators, processors, and outputs.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    graphEnabledToggle.setToggleState(true, juce::dontSendNotification);
    graphEnabledToggle.onClick = [this]
    {
        if (onEnabledChanged)
            onEnabledChanged(graphEnabledToggle.getToggleState());
    };
    addAndMakeVisible(graphEnabledToggle);

    auto templates = cw::getStarterNodeTemplates();

    for (const auto& node : templates)
    {
        auto* card = nodeCards.add(new NodeCard(node));
        addAndMakeVisible(card);
    }

    if (nodeCards.size() >= 4)
        nodeCards[3]->amountSlider.onValueChange = [this]
        {
            if (onInputChanged)
                onInputChanged(static_cast<float>(nodeCards[3]->amountSlider.getValue()));
        };

    if (nodeCards.size() >= 5)
        nodeCards[4]->amountSlider.onValueChange = [this]
        {
            if (onDriveChanged)
                onDriveChanged(static_cast<float>(nodeCards[4]->amountSlider.getValue()));
        };

    if (nodeCards.size() >= 6)
        nodeCards[5]->amountSlider.onValueChange = [this]
        {
            if (onToneChanged)
                onToneChanged(static_cast<float>(nodeCards[5]->amountSlider.getValue()));
        };

    if (nodeCards.size() >= 7)
        nodeCards[6]->amountSlider.onValueChange = [this]
        {
            if (onEchoChanged)
                onEchoChanged(static_cast<float>(nodeCards[6]->amountSlider.getValue()));
        };

    if (nodeCards.size() >= 8)
        nodeCards[7]->amountSlider.onValueChange = [this]
        {
            if (onWidthChanged)
                onWidthChanged(static_cast<float>(nodeCards[7]->amountSlider.getValue()));
        };
}

void GraphPanel::setInput(float amount)
{
    if (nodeCards.size() >= 4)
        nodeCards[3]->setAmount(amount);
}

void GraphPanel::setDrive(float amount)
{
    if (nodeCards.size() >= 5)
        nodeCards[4]->setAmount(amount);
}

void GraphPanel::setTone(float amount)
{
    if (nodeCards.size() >= 6)
        nodeCards[5]->setAmount(amount);
}

void GraphPanel::setEcho(float amount)
{
    if (nodeCards.size() >= 7)
        nodeCards[6]->setAmount(amount);
}

void GraphPanel::setWidth(float amount)
{
    if (nodeCards.size() >= 8)
        nodeCards[7]->setAmount(amount);
}

void GraphPanel::setEnabled(bool shouldEnable)
{
    graphEnabledToggle.setToggleState(shouldEnable, juce::dontSendNotification);
    for (auto* card : nodeCards)
        card->setEnabled(shouldEnable);
}

void GraphPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());

    auto bounds = getLocalBounds().reduced(20).toFloat();
    auto columnWidth = juce::jmin(230.0f, (bounds.getWidth() - 40.0f) / 3.0f);
    auto gap = 20.0f;
    auto sourceX = bounds.getX();
    auto effectX = sourceX + columnWidth + gap;
    auto sinkX = effectX + columnWidth + gap;

    auto drawBus = [&](float fromX, float toX, float y, juce::Colour colour)
    {
        g.setColour(colour.withAlpha(0.55f));
        g.drawLine(fromX, y, toX, y, 2.0f);
        g.drawLine(toX - 10.0f, y - 5.0f, toX, y, 2.0f);
        g.drawLine(toX - 10.0f, y + 5.0f, toX, y, 2.0f);
    };

    drawBus(sourceX + columnWidth, effectX, bounds.getY() + 150.0f, cw::nodeCategoryColour(cw::NodeCategory::source));
    drawBus(effectX + columnWidth, sinkX, bounds.getY() + 150.0f, cw::nodeCategoryColour(cw::NodeCategory::effect));
}

void GraphPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto top = area.removeFromTop(60);
    headerLabel.setBounds(top.removeFromLeft(240));
    graphEnabledToggle.setBounds(top.removeFromRight(120));
    subtitleLabel.setBounds(area.removeFromTop(24));

    area.removeFromTop(10);

    auto columnWidth = juce::jmin(230, (area.getWidth() - 2 * 20) / 3);
    auto cardHeight = 160;
    auto gap = 20;
    auto sourceX = area.getX();
    auto effectX = sourceX + columnWidth + gap;
    auto sinkX = effectX + columnWidth + gap;
    int sourceRow = 0;
    int effectRow = 0;
    int sinkRow = 0;

    for (auto* card : nodeCards)
    {
        if (card->nodeTemplate.category == cw::NodeCategory::source)
            card->setBounds(sourceX, area.getY() + 20 + sourceRow++ * (cardHeight + 12), columnWidth, cardHeight);
        else if (card->nodeTemplate.category == cw::NodeCategory::effect)
            card->setBounds(effectX, area.getY() + 20 + effectRow++ * (cardHeight + 12), columnWidth, cardHeight);
        else
            card->setBounds(sinkX, area.getY() + 20 + sinkRow++ * (cardHeight + 12), columnWidth, cardHeight);
    }
}
