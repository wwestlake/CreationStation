#include "CreationStationPluginHeader.h"
#include "CreationStationPluginPalette.h"
#include "../Branding.h"

namespace cs::plugins
{

CreationStationPluginHeader::CreationStationPluginHeader(const juce::String& pluginName)
{
    logoImage = branding::createCreationStationLogoImage(48);

    brandLabel.setText("CREATION STATION", juce::dontSendNotification);
    brandLabel.setFont(juce::Font(11.0f).boldened());
    brandLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    addAndMakeVisible(brandLabel);

    nameLabel.setText(pluginName, juce::dontSendNotification);
    nameLabel.setFont(juce::Font(20.0f).boldened());
    nameLabel.setColour(juce::Label::textColourId, palette::textPrimary);
    addAndMakeVisible(nameLabel);
}

void CreationStationPluginHeader::paint(juce::Graphics& g)
{
    g.fillAll(palette::panelBackground);
    g.setColour(palette::outline);
    g.drawLine(0.0f, (float) getHeight() - 1.0f, (float) getWidth(), (float) getHeight() - 1.0f, 1.0f);

    if (logoImage.isValid())
        g.drawImage(logoImage, juce::Rectangle<float>(12.0f, (float) getHeight() * 0.5f - 20.0f, 40.0f, 40.0f));
}

void CreationStationPluginHeader::resized()
{
    auto area = getLocalBounds().reduced(0, 6);
    area.removeFromLeft(64);
    brandLabel.setBounds(area.removeFromTop(16));
    nameLabel.setBounds(area);
}

}
