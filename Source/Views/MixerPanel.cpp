#include "MixerPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff171a21); }
juce::Colour stripColour() { return juce::Colour(0xff232836); }
juce::Colour masterStripColour() { return juce::Colour(0xff2f374a); }
}

MixerPanel::ChannelStrip::ChannelStrip(int channelIndex, const juce::String& channelName, bool isMasterStrip)
    : index(channelIndex), master(isMasterStrip)
{
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    insertLabel.setJustificationType(juce::Justification::centred);
    insertLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(insertLabel);

    indexLabel.setJustificationType(juce::Justification::centred);
    indexLabel.setColour(juce::Label::textColourId, juce::Colour(0xff90a4c3));
    addAndMakeVisible(indexLabel);

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

    if (! master)
    {
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

        fxButton.onClick = [this]
        {
            if (auto* parent = dynamic_cast<MixerPanel*>(getParentComponent()))
                if (parent->onInsertButtonClicked)
                    parent->onInsertButtonClicked(index);
        };
        addAndMakeVisible(fxButton);
    }

    setChannelName(channelName);
}

void MixerPanel::ChannelStrip::setChannelIndex(int newChannelIndex)
{
    index = newChannelIndex;
}

void MixerPanel::ChannelStrip::setChannelName(const juce::String& name)
{
    nameLabel.setText(name, juce::dontSendNotification);
}

void MixerPanel::ChannelStrip::setInsertName(const juce::String& name)
{
    insertLoaded = name.isNotEmpty();
    insertLabel.setText(name, juce::dontSendNotification);
}

void MixerPanel::ChannelStrip::setInsertBypassed(bool isBypassed)
{
    insertBypassed = isBypassed;
}

void MixerPanel::ChannelStrip::setSelected(bool isSelected)
{
    selected = isSelected;
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
    auto baseColour = master ? masterStripColour() : stripColour();

    if (! master && insertLoaded)
        baseColour = insertBypassed ? juce::Colour(0xff4d3d2e) : juce::Colour(0xff2a3a4c);

    g.setColour(baseColour.withAlpha(0.9f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f);

    if (selected)
    {
        g.setColour(juce::Colour(0xfff0c674).withAlpha(0.9f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f, 2.0f);
    }
    else if (master)
    {
        g.setColour(juce::Colour(0xff79c0ff).withAlpha(0.7f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f, 2.0f);
    }
    else
    {
        g.setColour(juce::Colour(0xff1f2532));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f, 1.0f);
    }
}

void MixerPanel::ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced(14);
    indexLabel.setBounds(area.removeFromTop(18));
    nameLabel.setBounds(area.removeFromTop(20));
    insertLabel.setBounds(area.removeFromTop(18));

    if (master)
    {
        gainSlider.setBounds(area.reduced(10, 4));
        return;
    }

    auto controlArea = area;
    if (! master)
        fxButton.setBounds(controlArea.removeFromBottom(24));
    muteButton.setBounds(controlArea.removeFromBottom(28));
    soloButton.setBounds(controlArea.removeFromBottom(28));
    panSlider.setBounds(controlArea.removeFromBottom(44));
    gainSlider.setBounds(controlArea.reduced(12, 0));
}

MixerPanel::MixerPanel()
{
    setName("Mixer");
    headerLabel.setText("Mixing Console", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    bankLabel.setText("Bank 1-8", juce::dontSendNotification);
    bankLabel.setJustificationType(juce::Justification::centredRight);
    bankLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(bankLabel);

    bankLeftButton.onClick = [this]
    {
        setBankOffset(bankOffset - visibleChannelCount);
        if (onBankOffsetChanged)
            onBankOffsetChanged(bankOffset);
    };
    addAndMakeVisible(bankLeftButton);

    bankRightButton.onClick = [this]
    {
        setBankOffset(bankOffset + visibleChannelCount);
        if (onBankOffsetChanged)
            onBankOffsetChanged(bankOffset);
    };
    addAndMakeVisible(bankRightButton);

    for (int slot = 0; slot < visibleChannelCount; ++slot)
    {
        auto* strip = strips.add(new ChannelStrip(slot, "Track " + juce::String(slot + 1)));
        addAndMakeVisible(strip);
    }

    masterStrip = std::make_unique<ChannelStrip>(visibleChannelCount, "Master", true);
    addAndMakeVisible(masterStrip.get());
    refreshVisibleStripLabels();
}

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void MixerPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto top = area.removeFromTop(44);

    headerLabel.setBounds(top.removeFromLeft(240));
    bankLeftButton.setBounds(top.removeFromLeft(90).reduced(0, 2));
    bankRightButton.setBounds(top.removeFromLeft(90).reduced(0, 2));
    bankLabel.setBounds(top);

    area.removeFromTop(8);

    auto masterArea = area.removeFromRight(170);
    masterStrip->setBounds(masterArea.withTrimmedBottom(8));

    auto stripArea = area;
    auto count = juce::jmax(1, strips.size());
    auto gap = 10;
    auto stripWidth = (stripArea.getWidth() - (count - 1) * gap) / count;

    for (auto* strip : strips)
    {
        strip->setBounds(stripArea.removeFromLeft(stripWidth));
        stripArea.removeFromLeft(gap);
    }
}

void MixerPanel::setChannelCount(int newTotalChannelCount)
{
    totalChannelCount = juce::jmax(visibleChannelCount, newTotalChannelCount);
    setBankOffset(bankOffset);
}

void MixerPanel::setBankOffset(int newBankOffset)
{
    auto maxBankOffset = juce::jmax(0, totalChannelCount - visibleChannelCount);
    bankOffset = juce::jlimit(0, maxBankOffset, newBankOffset);
    refreshVisibleStripLabels();

    auto bankStart = bankOffset + 1;
    auto bankEnd = juce::jmin(totalChannelCount, bankOffset + visibleChannelCount);
    bankLabel.setText("Bank " + juce::String(bankStart) + "-" + juce::String(bankEnd), juce::dontSendNotification);
    repaint();
}

void MixerPanel::refreshVisibleStripLabels()
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        auto* strip = strips[(size_t) slot];

        strip->setChannelIndex(absoluteIndex);

        if (absoluteIndex < totalChannelCount)
        {
            strip->setChannelName("Ch " + juce::String(absoluteIndex + 1));
            strip->setEnabled(true);
        }
        else
        {
            strip->setChannelName("-");
            strip->setEnabled(false);
        }
    }
}

void MixerPanel::setChannelName(int channelIndex, const juce::String& name)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setChannelName(name);
            break;
        }
    }
}

void MixerPanel::setChannelInsertName(int channelIndex, const juce::String& name)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setInsertName(name);
            break;
        }
    }
}

void MixerPanel::setChannelInsertBypassed(int channelIndex, bool isBypassed)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setInsertBypassed(isBypassed);
            break;
        }
    }
}

void MixerPanel::setSelectedChannel(int channelIndex)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        strips[(size_t) slot]->setSelected(absoluteIndex == channelIndex);
    }
}

void MixerPanel::setChannelGain(int channelIndex, float gain)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setGain(gain);
            break;
        }
    }
}

void MixerPanel::setChannelPan(int channelIndex, float pan)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setPan(pan);
            break;
        }
    }
}

void MixerPanel::setChannelMuted(int channelIndex, bool shouldMute)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setMuted(shouldMute);
            break;
        }
    }
}

void MixerPanel::setChannelSoloed(int channelIndex, bool shouldSolo)
{
    for (int slot = 0; slot < strips.size(); ++slot)
    {
        auto absoluteIndex = bankOffset + slot;
        if (absoluteIndex == channelIndex)
        {
            strips[(size_t) slot]->setSoloed(shouldSolo);
            break;
        }
    }
}

void MixerPanel::setMasterGain(float gain)
{
    if (masterStrip != nullptr)
        masterStrip->setGain(gain);
}
