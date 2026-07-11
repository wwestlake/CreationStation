#include "MixerPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff171a21); }
juce::Colour stripColour() { return juce::Colour(0xff232836); }
}

MixerPanel::ChannelStrip::ChannelStrip(int channelIndex, const juce::String& channelName)
    : index(channelIndex)
{
    nameLabel.setText(channelName, juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    gainSlider.setRange(0.0, 1.0, 0.001);
    gainSlider.setValue(0.85);
    gainSlider.onValueChange = [this]
    {
        if (auto* parent = dynamic_cast<MixerPanel*>(getParentComponent()))
            if (parent->onGainChanged)
                parent->onGainChanged(index, static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);

    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    panSlider.setRange(-1.0, 1.0, 0.001);
    panSlider.setValue(0.0);
    panSlider.onValueChange = [this]
    {
        if (auto* parent = dynamic_cast<MixerPanel*>(getParentComponent()))
            if (parent->onPanChanged)
                parent->onPanChanged(index, static_cast<float>(panSlider.getValue()));
    };
    addAndMakeVisible(panSlider);

    muteButton.onClick = [this]
    {
        if (auto* parent = dynamic_cast<MixerPanel*>(getParentComponent()))
            if (parent->onMuteChanged)
                parent->onMuteChanged(index, muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);

    soloButton.onClick = [this]
    {
        if (auto* parent = dynamic_cast<MixerPanel*>(getParentComponent()))
            if (parent->onSoloChanged)
                parent->onSoloChanged(index, soloButton.getToggleState());
    };
    addAndMakeVisible(soloButton);
}

void MixerPanel::ChannelStrip::setGain(float gain)
{
    gainSlider.setValue(gain, juce::dontSendNotification);
}

void MixerPanel::ChannelStrip::setPan(float pan)
{
    panSlider.setValue(pan, juce::dontSendNotification);
}

void MixerPanel::ChannelStrip::setMuted(bool shouldMute)
{
    muteButton.setToggleState(shouldMute, juce::dontSendNotification);
}

void MixerPanel::ChannelStrip::setSoloed(bool shouldSolo)
{
    soloButton.setToggleState(shouldSolo, juce::dontSendNotification);
}

void MixerPanel::ChannelStrip::paint(juce::Graphics& g)
{
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 12.0f);
    g.setColour(stripColour().withAlpha(0.85f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f);
}

void MixerPanel::ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced(14);
    nameLabel.setBounds(area.removeFromTop(30));

    auto controlArea = area;
    muteButton.setBounds(controlArea.removeFromBottom(28));
    soloButton.setBounds(controlArea.removeFromBottom(28));
    panSlider.setBounds(controlArea.removeFromBottom(44));
    gainSlider.setBounds(controlArea.reduced(12, 0));
}

MixerPanel::MixerPanel()
{
    headerLabel.setText("Mixing Console", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(juce::FontOptions(24.0f)).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    int index = 0;
    for (const auto& name : { "Drums", "Bass", "Music", "Voice", "Master" })
    {
        auto* strip = strips.add(new ChannelStrip(index++, name));
        addAndMakeVisible(strip);
    }
}

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void MixerPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    headerLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(12);

    auto stripArea = area;
    auto count = juce::jmax(1, strips.size());
    auto gap = 12;
    auto stripWidth = (stripArea.getWidth() - (count - 1) * gap) / count;

    for (auto* strip : strips)
    {
        strip->setBounds(stripArea.removeFromLeft(stripWidth));
        stripArea.removeFromLeft(gap);
    }
}
