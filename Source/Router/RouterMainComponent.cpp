#include "RouterMainComponent.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff12161d); }
juce::Colour cardColour() { return juce::Colour(0xff1b2230); }
juce::Colour accentColour() { return juce::Colour(0xff67a6ff); }
juce::Colour textMuted() { return juce::Colour(0xff8ea0b7); }
}

RouterMainComponent::HeaderBar::HeaderBar()
{
    logoImage = branding::createDjehutiRouterLogoImage(64);

    titleLabel.setText("Djehuti Router", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Patch audio from apps, microphones, speakers, and headphones. DjeRoute is the short name.", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, textMuted());
    addAndMakeVisible(subtitleLabel);

    statusLabel.setText("Ready to build a route.", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9cb0c9));
    addAndMakeVisible(statusLabel);

    savePresetButton.onClick = [this]
    {
        setStatus("Saving preset...");
    };
    addAndMakeVisible(savePresetButton);

    loadPresetButton.onClick = [this]
    {
        setStatus("Loading preset...");
    };
    addAndMakeVisible(loadPresetButton);

    helpButton.onClick = [this]
    {
        setStatus("Pick a source on the left, then turn on the outputs you want.");
    };
    addAndMakeVisible(helpButton);
}

void RouterMainComponent::HeaderBar::setStatus(const juce::String& text)
{
    statusText = text;
    statusLabel.setText(text, juce::dontSendNotification);
}

void RouterMainComponent::HeaderBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0c0f14));
    g.setColour(juce::Colour(0xff222b3a));
    g.drawLine(0.0f, static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()), static_cast<float>(getHeight()) - 1.0f, 1.0f);
    if (logoImage.isValid())
        g.drawImageWithin(logoImage, 14, 9, 44, 44, juce::RectanglePlacement::centred, false);
}

void RouterMainComponent::HeaderBar::resized()
{
    auto area = getLocalBounds().reduced(18, 10);
    area.removeFromLeft(60);

    auto right = area.removeFromRight(408);
    helpButton.setBounds(right.removeFromRight(120).reduced(0, 6));
    right.removeFromRight(8);
    loadPresetButton.setBounds(right.removeFromRight(120).reduced(0, 6));
    right.removeFromRight(8);
    savePresetButton.setBounds(right.removeFromRight(120).reduced(0, 6));
    statusLabel.setBounds(right);

    titleLabel.setBounds(area.removeFromTop(30));
    subtitleLabel.setBounds(area.removeFromTop(20));
}

RouterMainComponent::SourcePanel::SourceRow::SourceRow()
{
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(15.0f).boldened());
    addAndMakeVisible(titleLabel);

    detailLabel.setColour(juce::Label::textColourId, textMuted());
    detailLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(detailLabel);

    selectButton.onClick = [this]
    {
        if (onSelected)
            onSelected();
    };
    addAndMakeVisible(selectButton);
}

void RouterMainComponent::SourcePanel::SourceRow::setText(const juce::String& title, const juce::String& detail)
{
    titleLabel.setText(title, juce::dontSendNotification);
    detailLabel.setText(detail, juce::dontSendNotification);
}

void RouterMainComponent::SourcePanel::SourceRow::setSelected(bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void RouterMainComponent::SourcePanel::SourceRow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(selected ? juce::Colour(0xff243655) : cardColour());
    g.fillRoundedRectangle(bounds, 14.0f);
    g.setColour(selected ? accentColour() : juce::Colour(0xff2f3949));
    g.drawRoundedRectangle(bounds, 14.0f, selected ? 2.0f : 1.0f);
}

void RouterMainComponent::SourcePanel::SourceRow::resized()
{
    auto area = getLocalBounds().reduced(14, 10);
    selectButton.setBounds(area.removeFromRight(72));
    titleLabel.setBounds(area.removeFromTop(22));
    detailLabel.setBounds(area.removeFromTop(18));
}

RouterMainComponent::SourcePanel::SourcePanel()
{
    headingLabel.setText("Sources", juce::dontSendNotification);
    headingLabel.setFont(juce::Font(20.0f).boldened());
    headingLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headingLabel);
}

void RouterMainComponent::SourcePanel::setSources(const juce::StringArray& names, const juce::StringArray& details)
{
    rows.clear(true);
    for (int i = 0; i < names.size(); ++i)
    {
        auto* row = rows.add(new SourceRow());
        row->setText(names[i], details[i]);
        row->onSelected = [this, i]
        {
            selectSource(i);
        };
        addAndMakeVisible(row);
    }

    selectSource(juce::jlimit(0, juce::jmax(0, rows.size() - 1), selectedSource));
}

void RouterMainComponent::SourcePanel::selectSource(int index)
{
    selectedSource = juce::jlimit(0, juce::jmax(0, rows.size() - 1), index);

    for (int i = 0; i < rows.size(); ++i)
        rows.getUnchecked(i)->setSelected(i == selectedSource);

    if (onSourceSelected)
        onSourceSelected(selectedSource);
}

void RouterMainComponent::SourcePanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void RouterMainComponent::SourcePanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    headingLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);

    auto rowHeight = 80;
    for (auto* row : rows)
    {
        row->setBounds(area.removeFromTop(rowHeight));
        area.removeFromTop(10);
    }
}

RouterMainComponent::SinkPanel::SinkCard::SinkCard()
{
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(15.0f).boldened());
    addAndMakeVisible(titleLabel);

    detailLabel.setColour(juce::Label::textColourId, textMuted());
    detailLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(detailLabel);

    enableButton.onClick = [this]
    {
        setEnabledState(enableButton.getToggleState());
        if (onToggled)
            onToggled(enabled);
    };
    addAndMakeVisible(enableButton);
}

void RouterMainComponent::SinkPanel::SinkCard::setText(const juce::String& title, const juce::String& detail)
{
    titleLabel.setText(title, juce::dontSendNotification);
    detailLabel.setText(detail, juce::dontSendNotification);
}

void RouterMainComponent::SinkPanel::SinkCard::setEnabledState(bool shouldEnable)
{
    enabled = shouldEnable;
    repaint();
}

void RouterMainComponent::SinkPanel::SinkCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(enabled ? juce::Colour(0xff28415f) : cardColour());
    g.fillRoundedRectangle(bounds, 14.0f);
    g.setColour(enabled ? accentColour() : juce::Colour(0xff2f3949));
    g.drawRoundedRectangle(bounds, 14.0f, enabled ? 2.0f : 1.0f);
}

void RouterMainComponent::SinkPanel::SinkCard::resized()
{
    auto area = getLocalBounds().reduced(14, 10);
    enableButton.setBounds(area.removeFromRight(88));
    titleLabel.setBounds(area.removeFromTop(22));
    detailLabel.setBounds(area.removeFromTop(18));
}

RouterMainComponent::SinkPanel::SinkPanel()
{
    headingLabel.setText("Cables", juce::dontSendNotification);
    headingLabel.setFont(juce::Font(20.0f).boldened());
    headingLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headingLabel);
}

void RouterMainComponent::SinkPanel::setSinks(const juce::StringArray& names, const juce::StringArray& details)
{
    cards.clear(true);
    for (int i = 0; i < names.size(); ++i)
    {
        auto* card = cards.add(new SinkCard());
        card->setText(names[i], details[i]);
        card->onToggled = [this, i](bool shouldEnable)
        {
            setSinkEnabled(i, shouldEnable);
            if (onSinkToggled)
                onSinkToggled(i, shouldEnable);
        };
        addAndMakeVisible(card);
    }
}

void RouterMainComponent::SinkPanel::setSinkEnabled(int index, bool shouldEnable)
{
    if (! juce::isPositiveAndBelow(index, cards.size()))
        return;

    cards.getUnchecked(index)->setEnabledState(shouldEnable);
    cards.getUnchecked(index)->enableButton.setToggleState(shouldEnable, juce::dontSendNotification);
}

bool RouterMainComponent::SinkPanel::isSinkEnabled(int index) const
{
    return juce::isPositiveAndBelow(index, cards.size()) ? cards.getUnchecked(index)->enableButton.getToggleState() : false;
}

void RouterMainComponent::SinkPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void RouterMainComponent::SinkPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    headingLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);

    auto cardHeight = 74;
    for (auto* card : cards)
    {
        card->setBounds(area.removeFromTop(cardHeight));
        area.removeFromTop(10);
    }
}

RouterMainComponent::ByokPanel::ByokPanel()
{
    headingLabel.setText("BYOK AI", juce::dontSendNotification);
    headingLabel.setFont(juce::Font(20.0f).boldened());
    headingLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headingLabel);

    subtitleLabel.setText("Bring your own model, or use it to draft routing ideas.", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(12.5f));
    subtitleLabel.setColour(juce::Label::textColourId, textMuted());
    addAndMakeVisible(subtitleLabel);

    promptEditor.setMultiLine(true);
    promptEditor.setReturnKeyStartsNewLine(true);
    promptEditor.setText("Route Reaper monitor to Bluetooth speakers for casual guitar play, and keep headphones as a fallback.");
    addAndMakeVisible(promptEditor);

    responseEditor.setMultiLine(true);
    responseEditor.setReadOnly(true);
    responseEditor.setText("The assistant will suggest a patch plan here.");
    addAndMakeVisible(responseEditor);

    draftButton.onClick = [this]
    {
        responseEditor.setText("Draft: source selected in the left panel, then enable the speaker sink for live monitoring.\n\nPrompt:\n" + promptEditor.getText(),
                               juce::dontSendNotification);
    };
    addAndMakeVisible(draftButton);

    applyButton.onClick = [this]
    {
        responseEditor.setText("Patch applied locally in the UI model. Next step is connecting this to the real audio engine.",
                               juce::dontSendNotification);
    };
    addAndMakeVisible(applyButton);
}

void RouterMainComponent::ByokPanel::setSummary(const juce::String& text)
{
    summaryText = text;
    repaint();
}

void RouterMainComponent::ByokPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11151c));
    g.setColour(juce::Colour(0xff223041));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 16.0f);
    g.setColour(juce::Colour(0xff2f3949));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 16.0f, 1.0f);

    g.setColour(textMuted());
    g.setFont(juce::Font(12.0f));
    g.drawFittedText(summaryText, getLocalBounds().reduced(18), juce::Justification::bottomLeft, 2);
}

void RouterMainComponent::ByokPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    headingLabel.setBounds(area.removeFromTop(28));
    subtitleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(10);

    auto buttonRow = area.removeFromBottom(34);
    applyButton.setBounds(buttonRow.removeFromRight(112));
    buttonRow.removeFromRight(8);
    draftButton.setBounds(buttonRow.removeFromRight(112));

    auto responseArea = area.removeFromBottom(area.getHeight() / 2);
    responseEditor.setBounds(responseArea);
    area.removeFromTop(10);
    promptEditor.setBounds(area);
}

RouterMainComponent::RouterMainComponent()
{
    sourceNames.add("Practice Source");
    sourceNames.add("Mic");
    sourceNames.add("Instrument");
    sourceNames.add("System Audio");
    sourceNames.add("Reaper Monitor");

    sourceDetails.add("Default routing deck");
    sourceDetails.add("Vocal capture");
    sourceDetails.add("Guitar and line-in");
    sourceDetails.add("Apps, browser, and games");
    sourceDetails.add("DAW monitor feed");

    sinkNames = cableModel.getCableNames();
    sinkDetails = cableModel.getCablePurposes();

    sourcePanel.setSources(sourceNames, sourceDetails);
    sinkPanel.setSinks(sinkNames, sinkDetails);
    sourcePanel.selectSource(0);

    sourcePanel.onSourceSelected = [this](int)
    {
        selectedSourceIndex = sourcePanel.getSelectedSource();
        cableModel.setSourceIndex(selectedSourceIndex);
        audioEngine.setRouteSourceIndex(selectedSourceIndex);
        if (! isApplyingPreset)
            presetName = "Custom";
        refreshSummary();
    };

    sinkPanel.onSinkToggled = [this](int index, bool shouldEnable)
    {
        if (! isApplyingPreset)
            presetName = "Custom";
        cableModel.setCableEnabled(index, shouldEnable);
        refreshSummary();
    };

    headerBar.savePresetButton.onClick = [this]
    {
        savePreset();
    };

    headerBar.loadPresetButton.onClick = [this]
    {
        loadPreset();
    };

    practicePresetButton.onClick = [this]
    {
        applyPreset("Practice");
    };

    recordPresetButton.onClick = [this]
    {
        applyPreset("Record");
    };

    streamPresetButton.onClick = [this]
    {
        applyPreset("Stream");
    };

    clearPresetButton.onClick = [this]
    {
        applyPreset("Custom");
    };

    addAndMakeVisible(headerBar);
    addAndMakeVisible(inputDeviceBox);
    addAndMakeVisible(outputDeviceBox);
    driverContractLabel.setJustificationType(juce::Justification::centredLeft);
    driverContractLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9cb0c9));
    driverContractLabel.setFont(juce::Font(12.0f));
    driverContractLabel.setText("Driver target: 48 kHz, 10 ms cable, named endpoints for OBS/Reaper.", juce::dontSendNotification);
    addAndMakeVisible(driverContractLabel);
    addAndMakeVisible(practicePresetButton);
    addAndMakeVisible(recordPresetButton);
    addAndMakeVisible(streamPresetButton);
    addAndMakeVisible(clearPresetButton);
    presetStatusLabel.setJustificationType(juce::Justification::centredLeft);
    presetStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9cb0c9));
    presetStatusLabel.setFont(juce::Font(12.0f));
    presetStatusLabel.setText("Preset: Custom", juce::dontSendNotification);
    addAndMakeVisible(presetStatusLabel);
    addAndMakeVisible(sourcePanel);
    addAndMakeVisible(sinkPanel);
    addAndMakeVisible(byokPanel);
    addAndMakeVisible(patchSummaryLabel);

    patchSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    patchSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9cb0c9));
    patchSummaryLabel.setFont(juce::Font(13.0f));
    addAndMakeVisible(patchSummaryLabel);

    refreshDeviceMenus();
    refreshSummary();
    applyPreset("Practice");
}

void RouterMainComponent::refreshDeviceMenus()
{
    inputDeviceBox.clear();
    outputDeviceBox.clear();

    auto inputNames = audioEngine.getInputDeviceNames();
    auto outputNames = audioEngine.getOutputDeviceNames();

    for (int i = 0; i < inputNames.size(); ++i)
        inputDeviceBox.addItem(inputNames[i], i + 1);

    for (int i = 0; i < outputNames.size(); ++i)
        outputDeviceBox.addItem(outputNames[i], i + 1);

    inputDeviceBox.onChange = [this]
    {
        auto id = inputDeviceBox.getSelectedId();
        if (id > 0)
            audioEngine.setInputDeviceByName(inputDeviceBox.getItemText(id - 1));
    };

    outputDeviceBox.onChange = [this]
    {
        auto id = outputDeviceBox.getSelectedId();
        if (id > 0)
            audioEngine.setOutputDeviceByName(outputDeviceBox.getItemText(id - 1));
    };

    if (inputNames.size() > 0)
        inputDeviceBox.setSelectedId(1, juce::dontSendNotification);
    if (outputNames.size() > 0)
        outputDeviceBox.setSelectedId(1, juce::dontSendNotification);
}

void RouterMainComponent::refreshSummary()
{
    selectedSourceIndex = sourcePanel.getSelectedSource();
    cableModel.setSourceIndex(selectedSourceIndex);
    audioEngine.setRouteSourceIndex(selectedSourceIndex);
    currentSourceName = cableModel.getSourceName(selectedSourceIndex);
    presetStatusLabel.setText("Preset: " + presetName, juce::dontSendNotification);
    currentSummary = cableModel.buildSummary();
    patchSummaryLabel.setText(currentSummary, juce::dontSendNotification);
    byokPanel.setSummary(currentSummary);

    if (sourcePanel.getSelectedSource() >= 0)
    {
        headerBar.setStatus("Selected: " + currentSourceName + ". Turn on the sinks you want.");
    }
}

void RouterMainComponent::applyPreset(const juce::String& preset)
{
    isApplyingPreset = true;
    presetName = preset;

    if (preset == "Practice")
    {
        sourcePanel.selectSource(0);
        for (int i = 0; i < sinkNames.size(); ++i)
        {
            auto enabled = (i == 0 || i == 1);
            sinkPanel.setSinkEnabled(i, enabled);
            cableModel.setCableEnabled(i, enabled);
        }
    }
    else if (preset == "Record")
    {
        sourcePanel.selectSource(2);
        for (int i = 0; i < sinkNames.size(); ++i)
        {
            auto enabled = (i == 2);
            sinkPanel.setSinkEnabled(i, enabled);
            cableModel.setCableEnabled(i, enabled);
        }
    }
    else if (preset == "Stream")
    {
        sourcePanel.selectSource(4);
        for (int i = 0; i < sinkNames.size(); ++i)
        {
            auto enabled = (i == 1 || i == 3);
            sinkPanel.setSinkEnabled(i, enabled);
            cableModel.setCableEnabled(i, enabled);
        }
    }
    else
    {
        presetName = "Custom";
    }

    isApplyingPreset = false;
    refreshSummary();
    headerBar.setStatus("Preset: " + presetName);
}

juce::String RouterMainComponent::makeSummary() const
{
    return cableModel.buildSummary();
}

juce::File RouterMainComponent::getPresetFile() const
{
    auto baseFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Creation Station")
        .getChildFile("Djehuti Router");
    return baseFolder.getChildFile("default-route.json");
}

void RouterMainComponent::savePreset()
{
    auto presetFile = getPresetFile();
    auto parentFolder = presetFile.getParentDirectory();
    juce::String errorMessage;

    if (! parentFolder.exists() && ! parentFolder.createDirectory())
    {
        headerBar.setStatus("Could not create preset folder.");
        return;
    }

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("sourceIndex", selectedSourceIndex);
    data->setProperty("presetName", presetName);
    data->setProperty("inputDeviceName", audioEngine.getCurrentInputDeviceName());
    data->setProperty("outputDeviceName", audioEngine.getCurrentOutputDeviceName());

    juce::Array<juce::var> cables;
    for (int i = 0; i < sinkNames.size(); ++i)
    {
        juce::DynamicObject::Ptr cable = new juce::DynamicObject();
        cable->setProperty("name", sinkNames[i]);
        cable->setProperty("enabled", sinkPanel.isSinkEnabled(i));
        cables.add(juce::var(cable.get()));
    }
    data->setProperty("cables", juce::var(cables));
    data->setProperty("sinks", juce::var(cables));

    auto json = juce::JSON::toString(juce::var(data.get()), true);
    if (! presetFile.replaceWithText(json))
    {
        headerBar.setStatus("Could not save preset.");
        return;
    }

    headerBar.setStatus("Preset saved to Documents.");
}

void RouterMainComponent::loadPreset()
{
    auto presetFile = getPresetFile();
    if (! presetFile.existsAsFile())
    {
        headerBar.setStatus("No preset file yet.");
        return;
    }

    auto parsed = juce::JSON::parse(presetFile);
    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        headerBar.setStatus("Preset file is invalid.");
        return;
    }

    auto sourceIndex = static_cast<int>(object->getProperty("sourceIndex"));
    isApplyingPreset = true;
    sourcePanel.selectSource(sourceIndex);
    selectedSourceIndex = sourcePanel.getSelectedSource();

    auto inputDeviceName = object->getProperty("inputDeviceName").toString();
    auto outputDeviceName = object->getProperty("outputDeviceName").toString();

    if (inputDeviceName.isNotEmpty())
        audioEngine.setInputDeviceByName(inputDeviceName);

    if (outputDeviceName.isNotEmpty())
        audioEngine.setOutputDeviceByName(outputDeviceName);

    for (int i = 0; i < inputDeviceBox.getNumItems(); ++i)
        if (inputDeviceBox.getItemText(i) == inputDeviceName)
            inputDeviceBox.setSelectedItemIndex(i, juce::dontSendNotification);

    for (int i = 0; i < outputDeviceBox.getNumItems(); ++i)
        if (outputDeviceBox.getItemText(i) == outputDeviceName)
            outputDeviceBox.setSelectedItemIndex(i, juce::dontSendNotification);

    auto loadedPresetName = object->getProperty("presetName").toString();
    presetName = loadedPresetName.isNotEmpty() ? loadedPresetName : "Custom";

    auto cables = object->getProperty("cables");
    if (auto* array = cables.getArray())
    {
        for (int i = 0; i < array->size(); ++i)
        {
            auto* cableObject = array->getReference(i).getDynamicObject();
            if (cableObject == nullptr)
                continue;

            auto enabled = static_cast<bool>(cableObject->getProperty("enabled"));
            sinkPanel.setSinkEnabled(i, enabled);
            cableModel.setCableEnabled(i, enabled);
        }
    }
    else
    {
        auto sinks = object->getProperty("sinks");
        if (auto* sinkArray = sinks.getArray())
        {
            for (int i = 0; i < sinkArray->size(); ++i)
            {
                auto* sinkObject = sinkArray->getReference(i).getDynamicObject();
                if (sinkObject == nullptr)
                    continue;

                auto enabled = static_cast<bool>(sinkObject->getProperty("enabled"));
                sinkPanel.setSinkEnabled(i, enabled);
                cableModel.setCableEnabled(i, enabled);
            }
        }
    }

    isApplyingPreset = false;
    refreshSummary();
    headerBar.setStatus("Preset loaded.");
}

void RouterMainComponent::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void RouterMainComponent::resized()
{
    auto area = getLocalBounds();
    headerBar.setBounds(area.removeFromTop(72));

    auto deviceRow = area.removeFromTop(34).reduced(16, 0);
    inputDeviceBox.setBounds(deviceRow.removeFromLeft(deviceRow.getWidth() / 2 - 4));
    deviceRow.removeFromLeft(8);
    outputDeviceBox.setBounds(deviceRow);

    driverContractLabel.setBounds(area.removeFromTop(20).reduced(18, 0));

    auto presetRow = area.removeFromTop(34).reduced(16, 0);
    practicePresetButton.setBounds(presetRow.removeFromLeft(110));
    presetRow.removeFromLeft(8);
    recordPresetButton.setBounds(presetRow.removeFromLeft(110));
    presetRow.removeFromLeft(8);
    streamPresetButton.setBounds(presetRow.removeFromLeft(110));
    presetRow.removeFromLeft(8);
    clearPresetButton.setBounds(presetRow.removeFromLeft(120));
    presetRow.removeFromLeft(8);
    presetStatusLabel.setBounds(presetRow);

    auto footer = area.removeFromBottom(30);
    patchSummaryLabel.setBounds(footer.reduced(18, 4));

    auto content = area.reduced(16);
    auto leftWidth = juce::jlimit(260, 360, content.getWidth() / 4);
    auto rightWidth = juce::jlimit(320, 420, content.getWidth() / 3);
    auto centreWidth = content.getWidth() - leftWidth - rightWidth - 20;

    sourcePanel.setBounds(content.removeFromLeft(leftWidth));
    content.removeFromLeft(10);
    byokPanel.setBounds(content.removeFromRight(rightWidth));
    content.removeFromRight(10);
    sinkPanel.setBounds(content.withWidth(centreWidth));
}
