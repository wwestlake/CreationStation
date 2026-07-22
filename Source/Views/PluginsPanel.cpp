#include "PluginsPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour cardColour() { return juce::Colour(0xff1a2030); }
juce::Colour borderColour() { return juce::Colour(0xff2a3445); }
juce::Colour dimText() { return juce::Colour(0xff8ea0b7); }
}

PluginsPanel::PathCard::PathCard()
{
    removeButton.onClick = [this]
    {
        if (onRemoveRequested)
            onRemoveRequested(index);
    };
    addAndMakeVisible(removeButton);
}

void PluginsPanel::PathCard::setPath(const juce::String& newPath, int newIndex)
{
    path = newPath;
    index = newIndex;
    repaint();
}

void PluginsPanel::PathCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(cardColour());
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(borderColour());
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    auto textArea = getLocalBounds().reduced(12);
    textArea.removeFromRight(94);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(path, textArea, juce::Justification::centredLeft, true);
}

void PluginsPanel::PathCard::resized()
{
    removeButton.setBounds(getLocalBounds().removeFromRight(94).reduced(10, 8));
}

PluginsPanel::PluginCard::PluginCard()
{
    loadButton.onClick = [this]
    {
        if (onLoadRequested)
            onLoadRequested(entry);
    };
    addAndMakeVisible(loadButton);

    assignButton.onClick = [this]
    {
        if (onAssignRequested)
            onAssignRequested(entry);
    };
    addAndMakeVisible(assignButton);
}

void PluginsPanel::PluginCard::setEntry(const VstPluginCatalog::Entry& newEntry)
{
    entry = newEntry;
    repaint();
}

void PluginsPanel::PluginCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(cardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(borderColour());
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    auto textArea = getLocalBounds().reduced(14);
    textArea.removeFromRight(280);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f).boldened());
    g.drawText(entry.name, textArea.removeFromTop(22), juce::Justification::centredLeft, true);

    g.setColour(dimText());
    g.setFont(juce::Font(12.0f));
    g.drawText(entry.file.getFullPathName(), textArea, juce::Justification::topLeft, true);
}

void PluginsPanel::PluginCard::resized()
{
    auto buttons = getLocalBounds().removeFromRight(266).reduced(14, 18);
    loadButton.setBounds(buttons.removeFromTop(28));
    buttons.removeFromTop(8);
    assignButton.setBounds(buttons.removeFromTop(28));
}

PluginsPanel::PluginsPanel()
{
    titleLabel.setText("Plugin Studio", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Manage VST folders, browse discovered plugins, load inserts, and assign plugins to VST nodes.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, dimText());
    addAndMakeVisible(subtitleLabel);

    targetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(targetLabel);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa7b6cb));
    addAndMakeVisible(statusLabel);

    pathsLabel.setText("VST Folders", juce::dontSendNotification);
    pathsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pathsLabel);

    pluginsLabel.setText("Discovered Plugins", juce::dontSendNotification);
    pluginsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pluginsLabel);

    addPathButton.onClick = [this]
    {
        if (onAddPathRequested)
            onAddPathRequested();
    };
    addAndMakeVisible(addPathButton);

    rescanButton.onClick = [this]
    {
        if (onRescanRequested)
            onRescanRequested();
    };
    addAndMakeVisible(rescanButton);

    pathsViewport.setViewedComponent(&pathsHost, false);
    pathsViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(pathsViewport);

    pluginsViewport.setViewedComponent(&pluginsHost, false);
    pluginsViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(pluginsViewport);
}

void PluginsPanel::setSearchPaths(const juce::StringArray& paths)
{
    searchPaths = paths;
    pathCards.clear(true);

    for (int index = 0; index < searchPaths.size(); ++index)
    {
        auto* card = pathCards.add(new PathCard());
        card->setPath(searchPaths[index], index);
        card->onRemoveRequested = [this](int pathIndex)
        {
            if (onRemovePathRequested)
                onRemovePathRequested(pathIndex);
        };
        pathsHost.addAndMakeVisible(card);
    }

    resized();
}

void PluginsPanel::setPlugins(const juce::Array<VstPluginCatalog::Entry>& entries)
{
    plugins = entries;
    pluginCards.clear(true);

    for (const auto& entry : plugins)
    {
        auto* card = pluginCards.add(new PluginCard());
        card->setEntry(entry);
        card->onLoadRequested = [this](const VstPluginCatalog::Entry& selected)
        {
            if (onLoadIntoInsertRequested)
                onLoadIntoInsertRequested(selected);
        };
        card->onAssignRequested = [this](const VstPluginCatalog::Entry& selected)
        {
            if (onAssignNodeRequested)
                onAssignNodeRequested(selected);
        };
        pluginsHost.addAndMakeVisible(card);
    }

    resized();
}

void PluginsPanel::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void PluginsPanel::setInsertTargetDescription(const juce::String& text)
{
    targetLabel.setText(text, juce::dontSendNotification);
}

void PluginsPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void PluginsPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    subtitleLabel.setBounds(area.removeFromTop(22));
    targetLabel.setBounds(area.removeFromTop(22));
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    auto topRow = area.removeFromTop(30);
    pathsLabel.setBounds(topRow.removeFromLeft(140));
    addPathButton.setBounds(topRow.removeFromLeft(110));
    topRow.removeFromLeft(8);
    rescanButton.setBounds(topRow.removeFromLeft(90));
    area.removeFromTop(8);

    auto pathArea = area.removeFromTop(juce::jmin(180, juce::jmax(72, 18 + pathCards.size() * 52)));
    pathsViewport.setBounds(pathArea);
    area.removeFromTop(14);

    pluginsLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    pluginsViewport.setBounds(area);

    auto pathWidth = juce::jmax(320, pathsViewport.getWidth() - 24);
    auto pathY = 0;
    for (auto* card : pathCards)
    {
        card->setBounds(0, pathY, pathWidth, 44);
        pathY += 52;
    }
    pathsHost.setSize(pathWidth, juce::jmax(pathY, pathsViewport.getHeight()));

    auto pluginWidth = juce::jmax(320, pluginsViewport.getWidth() - 24);
    auto pluginY = 0;
    for (auto* card : pluginCards)
    {
        card->setBounds(0, pluginY, pluginWidth, 92);
        pluginY += 100;
    }
    pluginsHost.setSize(pluginWidth, juce::jmax(pluginY, pluginsViewport.getHeight()));
}
