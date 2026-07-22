#include "GraphPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff13171d); }
juce::Colour cardColour() { return juce::Colour(0xff202838); }
juce::Colour paletteCardColour() { return juce::Colour(0xff1a2230); }
juce::Colour detailColour() { return juce::Colour(0xff171d28); }
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

GraphPanel::PaletteCard::PaletteCard(const cw::NodeTemplate& templateData)
    : nodeTemplate(templateData)
{
    addButton.onClick = [this]
    {
        if (onAddRequested)
            onAddRequested(nodeTemplate);
    };
    addAndMakeVisible(addButton);
}

void GraphPanel::PaletteCard::setAlreadyPlaced(bool newAlreadyPlaced)
{
    alreadyPlaced = newAlreadyPlaced;
    addButton.setButtonText(alreadyPlaced ? "Placed" : "Add");
    addButton.setEnabled(! alreadyPlaced);
    repaint();
}

void GraphPanel::PaletteCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto accent = cw::nodeCategoryColour(nodeTemplate.category);
    g.setColour(paletteCardColour());
    g.fillRoundedRectangle(bounds.reduced(2.0f), 12.0f);
    g.setColour(alreadyPlaced ? juce::Colour(0xff4d5a6d) : accent);
    g.drawRoundedRectangle(bounds.reduced(2.0f), 12.0f, 2.0f);

    auto textArea = getLocalBounds().reduced(12);
    textArea.removeFromRight(84);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText(nodeTemplate.name, textArea.removeFromTop(22), juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xff96a8be));
    g.setFont(juce::Font(12.0f));
    g.drawText(nodeTemplate.description, textArea, juce::Justification::topLeft, true);
}

void GraphPanel::PaletteCard::resized()
{
    addButton.setBounds(getLocalBounds().removeFromRight(78).reduced(10, 16));
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
    setAmount(templateData.defaultValue);

    enableToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(enableToggle);

    deleteButton.onClick = [this]
    {
        if (onDeleteRequested)
            onDeleteRequested();
    };
    deleteButton.setTooltip("Delete node");
    addAndMakeVisible(deleteButton);

    pluginLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    pluginLabel.setJustificationType(juce::Justification::centredLeft);
    pluginLabel.setVisible(isVstHost());
    addAndMakeVisible(pluginLabel);

    assignButton.onClick = [this]
    {
        if (onAssignVstRequested)
            onAssignVstRequested();
    };
    assignButton.setVisible(isVstHost());
    addAndMakeVisible(assignButton);

    openButton.onClick = [this]
    {
        if (onOpenVstRequested)
            onOpenVstRequested();
    };
    openButton.setVisible(isVstHost());
    openButton.setEnabled(false);
    addAndMakeVisible(openButton);

    if (isVstHost())
        setAssignedPluginName({});
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

void GraphPanel::NodeCard::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    repaint();
}

bool GraphPanel::NodeCard::isVstHost() const noexcept
{
    return nodeTemplate.name == "VST Host";
}

void GraphPanel::NodeCard::setAssignedPluginName(const juce::String& pluginName)
{
    if (! isVstHost())
        return;

    pluginLabel.setText(pluginName.isNotEmpty() ? "Plugin: " + pluginName : "Plugin: none", juce::dontSendNotification);
    openButton.setEnabled(pluginName.isNotEmpty());
}

void GraphPanel::NodeCard::setCustomPosition(juce::Point<int> newPosition)
{
    customPositionSet = true;
    setTopLeftPosition(newPosition);
}

juce::Point<int> GraphPanel::NodeCard::getCustomPosition() const noexcept
{
    return getPosition();
}

bool GraphPanel::NodeCard::hasCustomPosition() const noexcept
{
    return customPositionSet;
}

bool GraphPanel::NodeCard::hasInputPort() const noexcept
{
    return nodeTemplate.category != cw::NodeCategory::source;
}

bool GraphPanel::NodeCard::hasOutputPort() const noexcept
{
    return nodeTemplate.category != cw::NodeCategory::sink;
}

juce::Point<float> GraphPanel::NodeCard::getInputPortCentre() const noexcept
{
    return { 6.0f, 46.0f };
}

juce::Point<float> GraphPanel::NodeCard::getOutputPortCentre() const noexcept
{
    return { (float) getWidth() - 6.0f, 46.0f };
}

void GraphPanel::NodeCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto accent = cw::nodeCategoryColour(nodeTemplate.category);
    g.setColour(cardColour().withAlpha(enabled ? 0.95f : 0.6f));
    g.fillRoundedRectangle(bounds.reduced(3.0f), 14.0f);
    g.setColour(selected ? juce::Colour(0xff8fd3ff) : (enabled ? accent : juce::Colour(0xff4d5a6d)));
    g.drawRoundedRectangle(bounds.reduced(3.0f), 14.0f, selected ? 3.0f : 2.0f);

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

    auto drawPort = [&](juce::Point<float> centre, juce::Colour colour, bool filled)
    {
        auto area = juce::Rectangle<float>(12.0f, 12.0f).withCentre(centre);
        g.setColour(colour);
        if (filled)
            g.fillEllipse(area);
        else
            g.drawEllipse(area, 2.0f);
    };

    if (hasInputPort())
        drawPort(getInputPortCentre(), juce::Colour(0xff8fbaff), false);

    if (hasOutputPort())
        drawPort(getOutputPortCentre(), selected ? juce::Colour(0xff8fd3ff) : accent, true);
}

void GraphPanel::NodeCard::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto headerRow = area.removeFromTop(24);
    deleteButton.setBounds(headerRow.removeFromRight(28));
    enableToggle.setBounds(headerRow.removeFromRight(70));
    categoryLabel.setBounds(area.removeFromTop(20));
    title.setBounds(area.removeFromTop(24));
    subtitle.setBounds(area.removeFromTop(20));
    if (isVstHost())
    {
        pluginLabel.setBounds(area.removeFromTop(20));
        auto buttonRow = area.removeFromTop(30);
        assignButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        openButton.setBounds(buttonRow.removeFromLeft(84));
        area.removeFromTop(4);
    }
    amountSlider.setBounds(area.removeFromBottom(44));
}

void GraphPanel::NodeCard::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (onDeleteRequested)
            onDeleteRequested();
        return;
    }

    auto localPoint = event.position;
    if (hasOutputPort() && localPoint.getDistanceFrom(getOutputPortCentre()) <= 12.0f)
    {
        connectionDragActive = true;
        if (onConnectionDragStarted)
            onConnectionDragStarted(event.getEventRelativeTo(getParentComponent()).position);

        if (onSelected)
            onSelected();
        return;
    }

    dragger.startDraggingComponent(this, event);

    if (onSelected)
        onSelected();
}

void GraphPanel::NodeCard::mouseDrag(const juce::MouseEvent& event)
{
    if (connectionDragActive)
    {
        if (onConnectionDragged)
            onConnectionDragged(event.getEventRelativeTo(getParentComponent()).position);
        return;
    }

    if (auto* parent = getParentComponent())
    {
        constrainer.setMinimumOnscreenAmounts(getHeight() / 2, getWidth() / 2, getHeight() / 2, getWidth() / 2);
        dragger.dragComponent(this, event, &constrainer);

        auto area = parent->getLocalBounds().reduced(24);
        auto constrainedBounds = getBounds();
        constrainedBounds.setX(juce::jlimit(area.getX(), juce::jmax(area.getX(), area.getRight() - getWidth()), constrainedBounds.getX()));
        constrainedBounds.setY(juce::jlimit(area.getY() + 32, juce::jmax(area.getY() + 32, area.getBottom() - getHeight()), constrainedBounds.getY()));
        setBounds(constrainedBounds);
    }

    customPositionSet = true;
    if (onMoved)
        onMoved();
}

void GraphPanel::NodeCard::mouseUp(const juce::MouseEvent& event)
{
    if (connectionDragActive)
    {
        connectionDragActive = false;
        if (onConnectionDropped)
            onConnectionDropped(event.getEventRelativeTo(getParentComponent()).position);
        return;
    }

    juce::ignoreUnused(event);
    customPositionSet = true;

    if (onMoved)
        onMoved();

    if (onSelected)
        onSelected();
}

void GraphPanel::Canvas::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());

    auto bounds = getLocalBounds().reduced(24).toFloat();
    g.setColour(juce::Colour(0xff7d8fa8));
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText(owner.autoConnectEnabled ? "Auto chain mode" : "Manual patch mode",
               juce::Rectangle<int>(24, 18, 280, 24),
               juce::Justification::centredLeft,
               false);

    if (owner.placedNodeCards.isEmpty())
    {
        g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(18.0f));
        g.drawText("Add nodes from the toolbox to start building a patch.",
                   getLocalBounds().reduced(30),
                   juce::Justification::centred,
                   true);
        return;
    }

    auto sourceNodes = owner.getOrderedNodesForCategory(cw::NodeCategory::source);
    auto effectNodes = owner.getOrderedNodesForCategory(cw::NodeCategory::effect);
    auto sinkNodes = owner.getOrderedNodesForCategory(cw::NodeCategory::sink);
    auto chainNodes = owner.getChainOrderedNodes();

    auto drawPatchLead = [&](juce::Point<float> from, juce::Point<float> to, juce::Colour colour)
    {
        juce::Path path;
        auto bend = juce::jmax(36.0f, (to.x - from.x) * 0.45f);
        path.startNewSubPath(from);
        path.cubicTo({ from.x + bend, from.y },
                     { to.x - bend, to.y },
                     to);

        g.setColour(colour.withAlpha(0.75f));
        g.strokePath(path, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto arrowBase = juce::Point<float>(to.x - 10.0f, to.y);
        g.drawLine(arrowBase.x - 4.0f, arrowBase.y - 5.0f, to.x, to.y, 2.0f);
        g.drawLine(arrowBase.x - 4.0f, arrowBase.y + 5.0f, to.x, to.y, 2.0f);
    };

    auto drawNodeHeader = [&](NodeCard* card)
    {
        auto area = juce::Rectangle<int>(card->getX(), 18, card->getWidth(), 24);
        juce::String title = card->nodeTemplate.category == cw::NodeCategory::source ? "Source"
                           : card->nodeTemplate.category == cw::NodeCategory::sink ? "Output"
                           : card->nodeTemplate.name;
        g.setColour(cw::nodeCategoryColour(card->nodeTemplate.category).withAlpha(0.9f));
        g.drawText(title, area, juce::Justification::centredLeft, false);
    };

    if (owner.autoConnectEnabled)
    {
        for (auto* card : chainNodes)
            drawNodeHeader(card);

        for (int index = 0; index + 1 < chainNodes.size(); ++index)
        {
            auto* fromCard = chainNodes[index];
            auto* toCard = chainNodes[index + 1];
            auto fromPoint = juce::Point<float>((float) fromCard->getX(), (float) fromCard->getY()) + fromCard->getOutputPortCentre();
            auto toPoint = juce::Point<float>((float) toCard->getX(), (float) toCard->getY()) + toCard->getInputPortCentre();
            drawPatchLead(fromPoint, toPoint, cw::nodeCategoryColour(fromCard->nodeTemplate.category));
        }
    }
    else
    {
        for (auto* card : sourceNodes) drawNodeHeader(card);
        for (auto* card : effectNodes) drawNodeHeader(card);
        for (auto* card : sinkNodes) drawNodeHeader(card);

        for (const auto& connection : owner.getEffectiveConnections())
        {
            auto* fromCard = owner.findPlacedNodeByName(connection.from);
            auto* toCard = owner.findPlacedNodeByName(connection.to);
            if (fromCard == nullptr || toCard == nullptr)
                continue;

            auto fromPoint = juce::Point<float>((float) fromCard->getX(), (float) fromCard->getY()) + fromCard->getOutputPortCentre();
            auto toPoint = juce::Point<float>((float) toCard->getX(), (float) toCard->getY()) + toCard->getInputPortCentre();
            drawPatchLead(fromPoint, toPoint, cw::nodeCategoryColour(fromCard->nodeTemplate.category));
        }
    }

    if (owner.manualConnectionInProgress)
        drawPatchLead(owner.manualConnectionStart, owner.manualConnectionCurrent, juce::Colour(0xff8fd3ff));
}

GraphPanel::GraphPanel()
    : canvas(*this)
{
    setName("Patch Lab");
    headerLabel.setText("Patch Workspace", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    subtitleLabel.setText("Pick node types from the toolbox, place them on the canvas, and shape the chain there.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    graphEnabledToggle.setToggleState(true, juce::dontSendNotification);
    graphEnabledToggle.onClick = [this]
    {
        if (onEnabledChanged)
            onEnabledChanged(graphEnabledToggle.getToggleState());
    };
    addAndMakeVisible(graphEnabledToggle);

    autoConnectToggle.setToggleState(true, juce::dontSendNotification);
    autoConnectToggle.onClick = [this]
    {
        if (! autoConnectToggle.getToggleState())
            manualConnections = getEffectiveConnections();

        autoConnectEnabled = autoConnectToggle.getToggleState();
        refreshSelectionSummary();
        layoutPlacedNodes();
    };
    addAndMakeVisible(autoConnectToggle);

    clearCanvasButton.onClick = [this]
    {
        clearPlacedNodes();
    };
    addAndMakeVisible(clearCanvasButton);

    vstStatusLabel.setText("VST node: none assigned", juce::dontSendNotification);
    vstStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(vstStatusLabel);

    toolboxTitleLabel.setText("Toolbox", juce::dontSendNotification);
    toolboxTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    toolboxTitleLabel.setFont(juce::Font(18.0f).boldened());
    addAndMakeVisible(toolboxTitleLabel);

    selectionTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    selectionTitleLabel.setFont(juce::Font(18.0f).boldened());
    addAndMakeVisible(selectionTitleLabel);

    selectionBodyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa7b6cb));
    addAndMakeVisible(selectionBodyLabel);

    selectionMetaLabel.setColour(juce::Label::textColourId, accentColour());
    addAndMakeVisible(selectionMetaLabel);

    detailControlLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd7deea));
    detailControlLabel.setVisible(false);
    addAndMakeVisible(detailControlLabel);

    detailControlSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    detailControlSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 22);
    detailControlSlider.setVisible(false);
    addAndMakeVisible(detailControlSlider);

    paletteViewport.setViewedComponent(&paletteHost, false);
    paletteViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(paletteViewport);

    canvasViewport.setViewedComponent(&canvas, false);
    canvasViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(canvasViewport);

    auto templates = cw::getStarterNodeTemplates();
    for (const auto& node : templates)
    {
        auto* card = paletteCards.add(new PaletteCard(node));
        card->onAddRequested = [this](const cw::NodeTemplate& templateData)
        {
            addPlacedNode(templateData);
        };
        paletteHost.addAndMakeVisible(card);
    }

    refreshPaletteState();
    refreshSelectionSummary();
}

void GraphPanel::setInput(float amount)
{
    if (auto* node = findPlacedNodeByName("Oscillator"))
        node->setAmount(amount);
}

void GraphPanel::setDrive(float amount)
{
    if (auto* node = findPlacedNodeByName("Drive"))
        node->setAmount(amount);
}

void GraphPanel::setTone(float amount)
{
    if (auto* node = findPlacedNodeByName("Filter"))
        node->setAmount(amount);
}

void GraphPanel::setEcho(float amount)
{
    if (auto* node = findPlacedNodeByName("Delay"))
        node->setAmount(amount);
}

void GraphPanel::setWidth(float amount)
{
    if (auto* node = findPlacedNodeByName("Modulator"))
        node->setAmount(amount);
}

void GraphPanel::setEnabled(bool shouldEnable)
{
    graphEnabledToggle.setToggleState(shouldEnable, juce::dontSendNotification);
    for (auto* card : placedNodeCards)
        card->setEnabled(shouldEnable);
}

void GraphPanel::setAssignedVstPlugin(const juce::String& pluginName, const juce::String& pluginPath)
{
    assignedVstPluginName = pluginName;
    assignedVstPluginPath = pluginPath;
    vstStatusLabel.setText(pluginName.isNotEmpty() ? "VST node: " + pluginName : "VST node: none assigned",
                           juce::dontSendNotification);

    if (findPlacedNodeIndexByName("VST Host") < 0)
        addPlacedNode(findTemplateByName("VST Host"));

    if (auto* node = findPlacedNodeByName("VST Host"))
        node->setAssignedPluginName(pluginName);

    refreshSelectionSummary();
}

void GraphPanel::clearAssignedVstPlugin()
{
    assignedVstPluginName.clear();
    assignedVstPluginPath.clear();
    vstStatusLabel.setText("VST node: none assigned", juce::dontSendNotification);

    if (auto* node = findPlacedNodeByName("VST Host"))
        node->setAssignedPluginName({});

    refreshSelectionSummary();
}

void GraphPanel::setVstMix(float amount)
{
    if (auto* node = findPlacedNodeByName("VST Host"))
        node->setAmount(amount);
    refreshSelectionSummary();
}

void GraphPanel::setVstEnabled(bool shouldEnable)
{
    if (auto* node = findPlacedNodeByName("VST Host"))
        node->setEnabled(shouldEnable);
    refreshSelectionSummary();
}

void GraphPanel::setOscillatorFrequency(float hz)
{
    detailControlSlider.setValue(hz, juce::dontSendNotification);
    refreshSelectionSummary();
}

void GraphPanel::setOutputLevel(float amount)
{
    if (auto* node = findPlacedNodeByName("Speakers"))
        node->setAmount(amount);
    refreshSelectionSummary();
}

float GraphPanel::getVstMix() const noexcept
{
    if (auto* node = findPlacedNodeByName("VST Host"))
        return static_cast<float>(node->amountSlider.getValue());

    return 0.5f;
}

bool GraphPanel::isVstEnabled() const noexcept
{
    if (auto* node = findPlacedNodeByName("VST Host"))
        return node->enableToggle.getToggleState();

    return true;
}

float GraphPanel::getOscillatorFrequency() const noexcept
{
    return static_cast<float>(detailControlSlider.getValue());
}

float GraphPanel::getOutputLevel() const noexcept
{
    if (auto* node = findPlacedNodeByName("Speakers"))
        return static_cast<float>(node->amountSlider.getValue());

    return 0.8f;
}

juce::String GraphPanel::getAssignedVstPluginPath() const
{
    return assignedVstPluginPath;
}

juce::String GraphPanel::getSelectedNodeName() const
{
    if (juce::isPositiveAndBelow(selectedNodeIndex, placedNodeCards.size()))
        return placedNodeCards[selectedNodeIndex]->nodeTemplate.name;

    return {};
}

bool GraphPanel::hasNode(const juce::String& name) const
{
    return findPlacedNodeIndexByName(name) >= 0;
}

void GraphPanel::applyAiMacro(const juce::String& macroName)
{
    auto name = macroName.trim();
    if (name.isEmpty())
        return;

    graphEnabledToggle.setToggleState(true, juce::dontSendNotification);

    addPlacedNode(findTemplateByName("Oscillator"));
    addPlacedNode(findTemplateByName("Drive"));
    addPlacedNode(findTemplateByName("Filter"));
    addPlacedNode(findTemplateByName("Delay"));
    addPlacedNode(findTemplateByName("Modulator"));
    addPlacedNode(findTemplateByName("Speakers"));

    if (name == "Noise Cleanup")
    {
        setEnabled(true);
        setInput(0.42f);
        setDrive(0.06f);
        setTone(0.78f);
        setEcho(0.02f);
        setWidth(0.12f);
    }
    else if (name == "Instrument Voice")
    {
        setEnabled(true);
        setInput(0.55f);
        setDrive(0.18f);
        setTone(0.62f);
        setEcho(0.14f);
        setWidth(0.68f);
    }
    else if (name == "Wide Atmosphere")
    {
        setEnabled(true);
        setInput(0.48f);
        setDrive(0.12f);
        setTone(0.57f);
        setEcho(0.42f);
        setWidth(0.88f);
    }

    refreshSelectionSummary();
}

juce::ValueTree GraphPanel::createState() const
{
    juce::ValueTree graphState("NodeGraph");
    graphState.setProperty("enabled", graphEnabledToggle.getToggleState(), nullptr);
    graphState.setProperty("autoConnectEnabled", autoConnectEnabled, nullptr);
    graphState.setProperty("selectedNodeIndex", selectedNodeIndex, nullptr);
    graphState.setProperty("assignedVstPluginName", assignedVstPluginName, nullptr);
    graphState.setProperty("assignedVstPluginPath", assignedVstPluginPath, nullptr);

    for (const auto* card : placedNodeCards)
    {
        juce::ValueTree nodeState("Node");
        nodeState.setProperty("name", card->nodeTemplate.name, nullptr);
        nodeState.setProperty("category", cw::nodeCategoryName(card->nodeTemplate.category), nullptr);
        nodeState.setProperty("amount", card->amountSlider.getValue(), nullptr);
        nodeState.setProperty("enabled", card->enableToggle.getToggleState(), nullptr);
        nodeState.setProperty("x", card->getX(), nullptr);
        nodeState.setProperty("y", card->getY(), nullptr);
        nodeState.setProperty("customPosition", card->hasCustomPosition(), nullptr);
        graphState.addChild(nodeState, -1, nullptr);
    }

    for (const auto& connection : manualConnections)
    {
        juce::ValueTree connectionState("Connection");
        connectionState.setProperty("from", connection.from, nullptr);
        connectionState.setProperty("to", connection.to, nullptr);
        graphState.addChild(connectionState, -1, nullptr);
    }

    return graphState;
}

void GraphPanel::restoreState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    clearPlacedNodes();
    graphEnabledToggle.setToggleState((bool) state.getProperty("enabled", true), juce::dontSendNotification);
    autoConnectEnabled = (bool) state.getProperty("autoConnectEnabled", true);
    autoConnectToggle.setToggleState(autoConnectEnabled, juce::dontSendNotification);
    manualConnections.clear();

    for (const auto child : state)
    {
        if (! child.hasType("Node"))
            continue;

        auto name = child.getProperty("name").toString();
        auto templateData = findTemplateByName(name);
        if (templateData.name.isEmpty())
            continue;

        addPlacedNode(templateData);
        if (auto* card = findPlacedNodeByName(name))
        {
            card->setAmount((float) child.getProperty("amount", templateData.defaultValue));
            card->setEnabled((bool) child.getProperty("enabled", true));
            if ((bool) child.getProperty("customPosition", false))
                card->setCustomPosition({ (int) child.getProperty("x", 0), (int) child.getProperty("y", 0) });
        }
    }

    for (const auto child : state)
    {
        if (! child.hasType("Connection"))
            continue;

        Connection connection;
        connection.from = child.getProperty("from").toString();
        connection.to = child.getProperty("to").toString();
        if (connection.from.isNotEmpty() && connection.to.isNotEmpty())
            manualConnections.add(connection);
    }

    setAssignedVstPlugin(state.getProperty("assignedVstPluginName").toString(),
                         state.getProperty("assignedVstPluginPath").toString());
    layoutPlacedNodes();
    setSelectedNodeIndex((int) state.getProperty("selectedNodeIndex", placedNodeCards.isEmpty() ? -1 : 0));
}

void GraphPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());

    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(detailColour());
    g.fillRoundedRectangle(bounds, 18.0f);
    g.setColour(juce::Colour(0xff263243));
    g.drawRoundedRectangle(bounds, 18.0f, 1.0f);
}

void GraphPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto top = area.removeFromTop(60);
    headerLabel.setBounds(top.removeFromLeft(280));
    graphEnabledToggle.setBounds(top.removeFromRight(120));
    autoConnectToggle.setBounds(top.removeFromRight(150));
    subtitleLabel.setBounds(area.removeFromTop(24));

    auto vstRow = area.removeFromTop(30);
    vstStatusLabel.setBounds(vstRow.removeFromLeft(520));
    clearCanvasButton.setBounds(vstRow.removeFromRight(120));

    area.removeFromTop(10);

    auto detailHeight = detailControlSlider.isVisible() ? 108 : 78;
    auto detailArea = area.removeFromTop(detailHeight);
    selectionTitleLabel.setBounds(detailArea.removeFromTop(24));
    selectionBodyLabel.setBounds(detailArea.removeFromTop(22));
    selectionMetaLabel.setBounds(detailArea.removeFromTop(22));
    if (detailControlSlider.isVisible())
    {
        auto extraRow = detailArea.removeFromTop(30);
        detailControlLabel.setBounds(extraRow.removeFromLeft(140));
        detailControlSlider.setBounds(extraRow);
    }
    else
    {
        detailControlLabel.setBounds({});
        detailControlSlider.setBounds({});
    }

    area.removeFromTop(8);

    auto leftColumn = area.removeFromLeft(290);
    toolboxTitleLabel.setBounds(leftColumn.removeFromTop(24));
    leftColumn.removeFromTop(8);
    paletteViewport.setBounds(leftColumn);

    area.removeFromLeft(12);
    canvasViewport.setBounds(area);

    auto paletteWidth = juce::jmax(240, paletteViewport.getWidth() - 18);
    int paletteY = 0;
    for (auto* card : paletteCards)
    {
        card->setBounds(0, paletteY, paletteWidth, 86);
        paletteY += 94;
    }
    paletteHost.setSize(paletteWidth, juce::jmax(paletteY, paletteViewport.getHeight()));

    layoutPlacedNodes();
}

void GraphPanel::addPlacedNode(const cw::NodeTemplate& nodeTemplate)
{
    if (nodeTemplate.name.isEmpty() || findPlacedNodeIndexByName(nodeTemplate.name) >= 0)
        return;

    auto* card = placedNodeCards.add(new NodeCard(nodeTemplate));
    canvas.addAndMakeVisible(card);
    refreshNodeCallbacks();
    refreshPaletteState();
    layoutPlacedNodes();
    setSelectedNodeIndex(placedNodeCards.size() - 1);
}

void GraphPanel::clearPlacedNodes()
{
    manualConnections.clear();
    manualConnectionInProgress = false;
    manualConnectionSource.clear();
    placedNodeCards.clear(true);
    selectedNodeIndex = -1;
    refreshPaletteState();
    refreshSelectionSummary();
    updateCanvasExtent();
}

void GraphPanel::removePlacedNodeByName(const juce::String& name)
{
    auto index = findPlacedNodeIndexByName(name);
    if (! juce::isPositiveAndBelow(index, placedNodeCards.size()))
        return;

    removeConnectionsForNode(name);
    placedNodeCards.remove(index);
    if (onNodeDeleted)
        onNodeDeleted(name);
    selectedNodeIndex = -1;
    refreshPaletteState();
    refreshNodeCallbacks();
    layoutPlacedNodes();
    setSelectedNodeIndex(placedNodeCards.isEmpty() ? -1 : juce::jlimit(0, placedNodeCards.size() - 1, index));
}

void GraphPanel::setSelectedNodeIndex(int index)
{
    if (placedNodeCards.isEmpty())
    {
        selectedNodeIndex = -1;
        refreshSelectionSummary();
        return;
    }

    selectedNodeIndex = juce::jlimit(0, placedNodeCards.size() - 1, index);

    for (int cardIndex = 0; cardIndex < placedNodeCards.size(); ++cardIndex)
        placedNodeCards[cardIndex]->setSelected(cardIndex == selectedNodeIndex);

    refreshSelectionSummary();
    auto nodeBounds = placedNodeCards[selectedNodeIndex]->getBounds();
    auto viewX = canvasViewport.getViewPositionX();
    auto viewY = canvasViewport.getViewPositionY();
    auto viewWidth = canvasViewport.getMaximumVisibleWidth();
    auto viewHeight = canvasViewport.getMaximumVisibleHeight();

    if (nodeBounds.getRight() > viewX + viewWidth)
        viewX = nodeBounds.getRight() - viewWidth;
    else if (nodeBounds.getX() < viewX)
        viewX = nodeBounds.getX();

    if (nodeBounds.getBottom() > viewY + viewHeight)
        viewY = nodeBounds.getBottom() - viewHeight;
    else if (nodeBounds.getY() < viewY)
        viewY = nodeBounds.getY();

    canvasViewport.setViewPosition(juce::jmax(0, viewX), juce::jmax(0, viewY));
}

void GraphPanel::refreshSelectionSummary()
{
    if (! juce::isPositiveAndBelow(selectedNodeIndex, placedNodeCards.size()))
    {
        selectionTitleLabel.setText("No node selected", juce::dontSendNotification);
        selectionBodyLabel.setText("Add a node from the toolbox, then click it to inspect and adjust it here.", juce::dontSendNotification);
        selectionMetaLabel.setText("Toolbox on the left - canvas on the right", juce::dontSendNotification);
        detailControlLabel.setVisible(false);
        detailControlSlider.setVisible(false);
        resized();
        return;
    }

    auto* card = placedNodeCards[selectedNodeIndex];
    selectionTitleLabel.setText(card->nodeTemplate.name, juce::dontSendNotification);
    selectionBodyLabel.setText(card->nodeTemplate.description, juce::dontSendNotification);

    auto meta = cw::nodeCategoryName(card->nodeTemplate.category)
                + "  |  "
                + card->nodeTemplate.parameterLabel
                + ": "
                + juce::String(card->amountSlider.getValue(), 2);

    if (card->nodeTemplate.name == "VST Host")
    {
        meta << "  |  Plugin: " << (assignedVstPluginName.isNotEmpty() ? assignedVstPluginName : "none");
        meta << "  |  State: " << (card->enableToggle.getToggleState() ? "On" : "Bypassed");
        detailControlLabel.setVisible(false);
        detailControlSlider.setVisible(false);
    }
    else if (card->nodeTemplate.name == "Oscillator")
    {
        detailControlLabel.setText("Frequency", juce::dontSendNotification);
        detailControlSlider.setRange(40.0, 2000.0, 1.0);
        detailControlSlider.setTextValueSuffix(" Hz");
        detailControlSlider.setValue(getOscillatorFrequency(), juce::dontSendNotification);
        detailControlSlider.onValueChange = [this]
        {
            if (onOscillatorFrequencyChanged)
                onOscillatorFrequencyChanged(static_cast<float>(detailControlSlider.getValue()));
        };
        detailControlSlider.setVisible(true);
        detailControlLabel.setVisible(true);
    }
    else if (card->nodeTemplate.name == "Speakers")
    {
        detailControlLabel.setText("Output Level", juce::dontSendNotification);
        detailControlSlider.setRange(0.0, 1.0, 0.001);
        detailControlSlider.setTextValueSuffix("");
        detailControlSlider.setValue(getOutputLevel(), juce::dontSendNotification);
        detailControlSlider.onValueChange = [this]
        {
            if (onOutputLevelChanged)
                onOutputLevelChanged(static_cast<float>(detailControlSlider.getValue()));
        };
        detailControlSlider.setVisible(true);
        detailControlLabel.setVisible(true);
    }
    else
    {
        detailControlLabel.setVisible(false);
        detailControlSlider.setVisible(false);
    }

    selectionMetaLabel.setText(meta, juce::dontSendNotification);
    resized();
}

void GraphPanel::refreshPaletteState()
{
    for (auto* card : paletteCards)
        card->setAlreadyPlaced(findPlacedNodeIndexByName(card->nodeTemplate.name) >= 0);
}

void GraphPanel::refreshNodeCallbacks()
{
    for (int index = 0; index < placedNodeCards.size(); ++index)
    {
        auto* card = placedNodeCards[index];
        card->onSelected = [this, index]
        {
            setSelectedNodeIndex(index);
        };
        card->onMoved = [this]
        {
            updateCanvasExtent();
            canvas.repaint();
        };
        card->onConnectionDragStarted = [this, card](const juce::Point<float>& point)
        {
            beginManualConnection(card->nodeTemplate.name, point);
        };
        card->onConnectionDragged = [this](const juce::Point<float>& point)
        {
            dragManualConnection(point);
        };
        card->onConnectionDropped = [this](const juce::Point<float>& point)
        {
            finishManualConnection(point);
        };
        card->onDeleteRequested = [this, card]
        {
            removePlacedNodeByName(card->nodeTemplate.name);
        };
        if (card->isVstHost())
        {
            card->onAssignVstRequested = [this]
            {
                if (onAssignVstPluginRequested)
                    onAssignVstPluginRequested();
            };
            card->onOpenVstRequested = [this]
            {
                if (onOpenAssignedVstRequested)
                    onOpenAssignedVstRequested();
            };
            card->setAssignedPluginName(assignedVstPluginName);
        }
        syncNamedNodeCallbacks(*card);
    }
}

void GraphPanel::syncNamedNodeCallbacks(NodeCard& card)
{
    if (card.nodeTemplate.name == "Oscillator")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onInputChanged)
                onInputChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        return;
    }

    if (card.nodeTemplate.name == "Drive")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onDriveChanged)
                onDriveChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        return;
    }

    if (card.nodeTemplate.name == "Filter")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onToneChanged)
                onToneChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        return;
    }

    if (card.nodeTemplate.name == "Delay")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onEchoChanged)
                onEchoChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        return;
    }

    if (card.nodeTemplate.name == "Modulator")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onWidthChanged)
                onWidthChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        return;
    }

    if (card.nodeTemplate.name == "VST Host")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onVstMixChanged)
                onVstMixChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        card.enableToggle.onClick = [this, &card]
        {
            if (onVstEnabledChanged)
                onVstEnabledChanged(card.enableToggle.getToggleState());
            refreshSelectionSummary();
        };
        return;
    }

    if (card.nodeTemplate.name == "Speakers")
    {
        card.amountSlider.onValueChange = [this, &card]
        {
            card.setAmount(static_cast<float>(card.amountSlider.getValue()));
            if (onOutputLevelChanged)
                onOutputLevelChanged(static_cast<float>(card.amountSlider.getValue()));
            refreshSelectionSummary();
        };
        return;
    }

    card.amountSlider.onValueChange = [this, &card]
    {
        card.setAmount(static_cast<float>(card.amountSlider.getValue()));
        refreshSelectionSummary();
    };
}

int GraphPanel::findPlacedNodeIndexByName(const juce::String& name) const
{
    for (int index = 0; index < placedNodeCards.size(); ++index)
        if (placedNodeCards[index]->nodeTemplate.name == name)
            return index;

    return -1;
}

GraphPanel::NodeCard* GraphPanel::findPlacedNodeByName(const juce::String& name) const
{
    auto index = findPlacedNodeIndexByName(name);
    return juce::isPositiveAndBelow(index, placedNodeCards.size()) ? placedNodeCards[index] : nullptr;
}

cw::NodeTemplate GraphPanel::findTemplateByName(const juce::String& name) const
{
    auto templates = cw::getStarterNodeTemplates();
    for (const auto& templateData : templates)
        if (templateData.name == name)
            return templateData;

    return {};
}

void GraphPanel::layoutPlacedNodes()
{
    updateCanvasExtent();

    auto bounds = canvas.getLocalBounds().reduced(24);
    auto columnWidth = 260;
    auto gap = 28;
    auto rowGap = 18;
    auto sourceNodes = getOrderedNodesForCategory(cw::NodeCategory::source);
    auto effectNodes = getOrderedNodesForCategory(cw::NodeCategory::effect);
    auto sinkNodes = getOrderedNodesForCategory(cw::NodeCategory::sink);
    auto sourceX = bounds.getX();
    auto sourceColumnCount = sourceNodes.isEmpty() ? 1 : 1;
    auto effectStartX = sourceX + sourceColumnCount * (columnWidth + gap);
    auto sinkX = effectStartX + juce::jmax(1, effectNodes.size()) * (columnWidth + gap);
    int sourceRow = 0;
    int sinkRow = 0;

    for (auto* card : placedNodeCards)
    {
        if (card->hasCustomPosition())
            continue;

        auto cardHeight = card->isVstHost() ? 214 : 160;
        if (card->nodeTemplate.category == cw::NodeCategory::source)
            card->setBounds(sourceX, bounds.getY() + 60 + sourceRow++ * (cardHeight + rowGap), columnWidth, cardHeight);
        else if (card->nodeTemplate.category == cw::NodeCategory::effect)
        {
            auto effectIndex = effectNodes.indexOf(card);
            auto effectX = effectStartX + juce::jmax(0, effectIndex) * (columnWidth + gap);
            card->setBounds(effectX, bounds.getY() + 60, columnWidth, cardHeight);
        }
        else
            card->setBounds(sinkX, bounds.getY() + 60 + sinkRow++ * (cardHeight + rowGap), columnWidth, cardHeight);
    }

    updateCanvasExtent();
    canvas.repaint();
}

void GraphPanel::updateCanvasExtent()
{
    auto canvasBounds = getCanvasContentBounds();
    canvas.setSize(canvasBounds.getWidth(), canvasBounds.getHeight());
}

juce::Array<GraphPanel::NodeCard*> GraphPanel::getOrderedNodesForCategory(cw::NodeCategory category) const
{
    juce::Array<NodeCard*> ordered;
    for (auto* card : placedNodeCards)
        if (card->nodeTemplate.category == category)
            ordered.add(card);

    std::sort(ordered.begin(), ordered.end(),
              [](const NodeCard* left, const NodeCard* right)
              {
                  if (left->getY() == right->getY())
                      return left->getX() < right->getX();

                  return left->getY() < right->getY();
              });

    return ordered;
}

juce::Array<GraphPanel::NodeCard*> GraphPanel::getChainOrderedNodes() const
{
    juce::Array<NodeCard*> ordered;
    for (auto* card : placedNodeCards)
        ordered.add(card);

    std::sort(ordered.begin(), ordered.end(),
              [](const NodeCard* left, const NodeCard* right)
              {
                  if (left->getX() == right->getX())
                      return left->getY() < right->getY();

                  return left->getX() < right->getX();
              });

    return ordered;
}

juce::Array<GraphPanel::Connection> GraphPanel::getEffectiveConnections() const
{
    if (! autoConnectEnabled)
        return manualConnections;

    juce::Array<Connection> connections;
    auto chainNodes = getChainOrderedNodes();
    for (int index = 0; index + 1 < chainNodes.size(); ++index)
        connections.add({ chainNodes[index]->nodeTemplate.name, chainNodes[index + 1]->nodeTemplate.name });

    return connections;
}

bool GraphPanel::connectionExists(const juce::String& from, const juce::String& to) const
{
    for (const auto& connection : manualConnections)
        if (connection.from == from && connection.to == to)
            return true;

    return false;
}

void GraphPanel::beginManualConnection(const juce::String& sourceNode, juce::Point<float> startPoint)
{
    if (autoConnectEnabled)
        autoConnectToggle.triggerClick();

    manualConnectionInProgress = true;
    manualConnectionSource = sourceNode;
    manualConnectionStart = startPoint;
    manualConnectionCurrent = startPoint;
    canvas.repaint();
}

void GraphPanel::dragManualConnection(juce::Point<float> currentPoint)
{
    if (! manualConnectionInProgress)
        return;

    manualConnectionCurrent = currentPoint;
    canvas.repaint();
}

void GraphPanel::finishManualConnection(juce::Point<float> dropPoint)
{
    if (! manualConnectionInProgress)
        return;

    auto* sourceCard = findPlacedNodeByName(manualConnectionSource);
    auto* targetCard = findNodeAtInputPort(dropPoint);

    if (sourceCard != nullptr && targetCard != nullptr
        && sourceCard != targetCard
        && sourceCard->hasOutputPort()
        && targetCard->hasInputPort()
        && ! connectionExists(sourceCard->nodeTemplate.name, targetCard->nodeTemplate.name))
    {
        for (int index = manualConnections.size(); --index >= 0;)
            if (manualConnections.getReference(index).to == targetCard->nodeTemplate.name)
                manualConnections.remove(index);

        manualConnections.add({ sourceCard->nodeTemplate.name, targetCard->nodeTemplate.name });
    }

    manualConnectionInProgress = false;
    manualConnectionSource.clear();
    canvas.repaint();
}

GraphPanel::NodeCard* GraphPanel::findNodeAtInputPort(juce::Point<float> canvasPoint) const
{
    for (auto* card : placedNodeCards)
    {
        if (! card->hasInputPort())
            continue;

        auto centre = juce::Point<float>((float) card->getX(), (float) card->getY()) + card->getInputPortCentre();
        if (centre.getDistanceFrom(canvasPoint) <= 14.0f)
            return card;
    }

    return nullptr;
}

void GraphPanel::removeConnectionsForNode(const juce::String& nodeName)
{
    for (int index = manualConnections.size(); --index >= 0;)
    {
        auto& connection = manualConnections.getReference(index);
        if (connection.from == nodeName || connection.to == nodeName)
            manualConnections.remove(index);
    }
}

juce::Rectangle<int> GraphPanel::getCanvasContentBounds() const
{
    auto sourceRows = 0;
    auto effectRows = 0;
    auto sinkRows = 0;
    auto extraVstHeight = 0;
    auto effectColumns = juce::jmax(1, getOrderedNodesForCategory(cw::NodeCategory::effect).size());
    auto totalColumns = 1 + effectColumns + 1;
    auto maxRight = 24 + (260 * totalColumns) + (28 * juce::jmax(0, totalColumns - 1)) + 24;
    auto maxBottom = 24 + 60 + 160 + 80;
    for (auto* card : placedNodeCards)
    {
        switch (card->nodeTemplate.category)
        {
            case cw::NodeCategory::source: ++sourceRows; break;
            case cw::NodeCategory::effect: ++effectRows; break;
            case cw::NodeCategory::sink: ++sinkRows; break;
        }
        if (card->isVstHost())
            extraVstHeight = juce::jmax(extraVstHeight, 54);

        maxRight = juce::jmax(maxRight, card->getRight() + 48);
        maxBottom = juce::jmax(maxBottom, card->getBottom() + 48);
    }

    auto maxRows = juce::jmax(sourceRows, juce::jmax(effectRows, sinkRows));
    auto width = 24 + (260 * 3) + (28 * 2) + 24;
    auto height = 24 + 60 + (juce::jmax(1, maxRows) * 160) + extraVstHeight + (juce::jmax(0, maxRows - 1) * 12) + 80;
    width = juce::jmax(width, maxRight);
    height = juce::jmax(height, maxBottom);
    width = juce::jmax(width, canvasViewport.getWidth() - 8);
    height = juce::jmax(height, canvasViewport.getHeight() - 8);
    return { 0, 0, width, height };
}
