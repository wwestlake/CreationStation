#include "PluginsPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour cardColour() { return juce::Colour(0xff1a2030); }
juce::Colour borderColour() { return juce::Colour(0xff2a3445); }
juce::Colour dimText() { return juce::Colour(0xff8ea0b7); }
juce::Colour creationStationAccent() { return juce::Colour(0xff56f4ff); }

// The manufacturer name every in-house plugin actually reports (COMPANY_NAME in
// cmake/CreationStationPlugin.cmake) - used to pull them into their own always-expanded section at
// the top of the browser. They still also appear under their normal Instruments/Effects/Developer
// sections below; the top section is an additional filtered view, not an exclusive container.
const juce::String& creationStationManufacturer()
{
    static const juce::String name = "LagDaemon Software";
    return name;
}
}

PluginsPanel::PathCard::PathCard()
{
    removeButton.onClick = [this]
    {
        if (onRemoveRequested)
            onRemoveRequested(index);
    };
    removeButton.setTooltip("Remove this VST search folder");
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

PluginsPanel::PluginItem::PluginItem()
{
    loadButton.onClick = [this]
    {
        if (onLoadRequested)
            onLoadRequested(entry);
    };
    loadButton.setTooltip("Load this plugin into the selected track's insert chain");
    addAndMakeVisible(loadButton);

    assignButton.onClick = [this]
    {
        if (onAssignRequested)
            onAssignRequested(entry);
    };
    assignButton.setTooltip("Assign this plugin to the VST node in the signal graph");
    addAndMakeVisible(assignButton);
}

void PluginsPanel::PluginItem::setEntry(const VstPluginCatalog::Entry& newEntry)
{
    entry = newEntry;
    repaint();
}

void PluginsPanel::PluginItem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(cardColour());
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(borderColour());
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto textArea = getLocalBounds().reduced(12);
    textArea.removeFromRight(280);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(entry.name, textArea.removeFromTop(18), juce::Justification::centredLeft, true);

    g.setColour(dimText());
    g.setFont(juce::Font(11.0f));
    if (entry.manufacturer.isNotEmpty())
        g.drawText(entry.manufacturer, textArea, juce::Justification::topLeft, true);
}

void PluginsPanel::PluginItem::resized()
{
    auto buttons = getLocalBounds().removeFromRight(266).reduced(12, 12);
    loadButton.setBounds(buttons.removeFromTop(26));
    buttons.removeFromTop(6);
    assignButton.setBounds(buttons.removeFromTop(26));
}

PluginsPanel::CategorySection::CategorySection(const juce::String& name, bool isCS)
    : categoryName(name), isCreationStation(isCS), collapsed(false)
{
}

void PluginsPanel::CategorySection::setPlugins(const juce::Array<VstPluginCatalog::Entry>& pluginList)
{
    pluginItems.clear(true);

    for (const auto& entry : pluginList)
    {
        auto* item = pluginItems.add(new PluginItem());
        item->setEntry(entry);
        item->onLoadRequested = onLoadRequested;
        item->onAssignRequested = onAssignRequested;
        addAndMakeVisible(item);
    }

    resized();
}

void PluginsPanel::CategorySection::setCollapsed(bool shouldCollapse)
{
    collapsed = shouldCollapse;
    for (auto* item : pluginItems)
        item->setVisible(! collapsed);
    repaint();
}

void PluginsPanel::CategorySection::paint(juce::Graphics& g)
{
    // Header background
    auto headerArea = getLocalBounds().removeFromTop(40).toFloat();
    if (isCreationStation)
        g.setColour(creationStationAccent().withAlpha(0.1f));
    else
        g.setColour(borderColour().withAlpha(0.3f));
    g.fillRoundedRectangle(headerArea, 8.0f);

    // Header border
    g.setColour(isCreationStation ? creationStationAccent() : borderColour());
    g.drawRoundedRectangle(headerArea, 8.0f, 1.5f);

    // Header text
    auto textBounds = headerArea.reduced(14.0f, 0.0f);
    g.setColour(isCreationStation ? creationStationAccent() : juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());

    // Draw collapse/expand indicator
    g.drawText(collapsed ? "▶" : "▼", textBounds.removeFromLeft(20), juce::Justification::centredLeft, false);
    g.drawText(categoryName, textBounds, juce::Justification::centredLeft, false);

    g.setColour(dimText());
    g.setFont(juce::Font(11.0f));
    g.drawText("(" + juce::String(pluginItems.size()) + ")", textBounds.removeFromRight(60), juce::Justification::centredRight, false);
}

void PluginsPanel::CategorySection::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(44);

    if (collapsed)
    {
        setSize(getWidth(), 44);
        return;
    }

    auto itemY = 44;
    for (auto* item : pluginItems)
    {
        item->setBounds(0, itemY, getWidth(), 82);
        itemY += 90;
    }

    setSize(getWidth(), itemY);
}

void PluginsPanel::CategorySection::mouseUp(const juce::MouseEvent& event)
{
    if (event.y > 44)
        return;

    setCollapsed(! collapsed);
    resized();

    if (onToggled)
        onToggled();
}

int PluginsPanel::CategorySection::getContentHeight() const
{
    if (collapsed)
        return 44;

    return 44 + (pluginItems.size() * 90);
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

    pluginsLabel.setText("Plugin Browser", juce::dontSendNotification);
    pluginsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pluginsLabel);

    addPathButton.onClick = [this]
    {
        if (onAddPathRequested)
            onAddPathRequested();
    };
    addPathButton.setTooltip("Add a folder to search for VST plugins");
    addAndMakeVisible(addPathButton);

    importPathListButton.setTooltip("Paste a semicolon-separated list of folders (e.g. copied from Reaper's VST path setting) to add them all at once");
    importPathListButton.onClick = [this]
    {
        if (onImportPathListRequested)
            onImportPathListRequested();
    };
    addAndMakeVisible(importPathListButton);

    rescanButton.onClick = [this]
    {
        if (onRescanRequested)
            onRescanRequested();
    };
    rescanButton.setTooltip("Rescan all VST folders for plugins");
    addAndMakeVisible(rescanButton);

    buildSamplePackButton.onClick = [this]
    {
        if (onBuildSamplePackRequested)
            onBuildSamplePackRequested();
    };
    buildSamplePackButton.setTooltip("Turn a folder of scattered single-note recordings into a chromatic sample pack");
    addAndMakeVisible(buildSamplePackButton);

    filterEditor.setTextToShowWhenEmpty("Search plugins...", dimText());
    addAndMakeVisible(filterEditor);

    formatFilter.addItem("All Formats", 1);
    formatFilter.addItem("VST3", 2);
    formatFilter.addItem("VST", 3);
    formatFilter.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(formatFilter);

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
    rebuildPluginsList();
    resized();
}

void PluginsPanel::rebuildPluginsList()
{
    categorySections.clear(true);

    // Creation Station plugins section (always at top, expanded by default)
    juce::Array<VstPluginCatalog::Entry> csPlugins;
    for (const auto& entry : plugins)
    {
        if (entry.manufacturer.equalsIgnoreCase(creationStationManufacturer()))
            csPlugins.add(entry);
    }

    if (! csPlugins.isEmpty())
    {
        auto* csSection = categorySections.add(new CategorySection("Creation Station Plugins", true));
        csSection->setPlugins(csPlugins);
        csSection->onLoadRequested = [this](const VstPluginCatalog::Entry& entry)
        {
            if (onLoadIntoInsertRequested)
                onLoadIntoInsertRequested(entry);
        };
        csSection->onAssignRequested = [this](const VstPluginCatalog::Entry& entry)
        {
            if (onAssignNodeRequested)
                onAssignNodeRequested(entry);
        };
        csSection->onToggled = [this] { resized(); };
        csSection->setCollapsed(false);
        pluginsHost.addAndMakeVisible(csSection);
    }

    // Instruments section
    juce::Array<VstPluginCatalog::Entry> instruments;
    for (const auto& entry : plugins)
    {
        if (entry.type == VstPluginCatalog::PluginType::Instrument)
            instruments.add(entry);
    }

    if (! instruments.isEmpty())
    {
        auto* instSection = categorySections.add(new CategorySection("Instruments"));
        instSection->setPlugins(instruments);
        instSection->onLoadRequested = [this](const VstPluginCatalog::Entry& entry)
        {
            if (onLoadIntoInsertRequested)
                onLoadIntoInsertRequested(entry);
        };
        instSection->onAssignRequested = [this](const VstPluginCatalog::Entry& entry)
        {
            if (onAssignNodeRequested)
                onAssignNodeRequested(entry);
        };
        instSection->onToggled = [this] { resized(); };
        instSection->setCollapsed(true);
        pluginsHost.addAndMakeVisible(instSection);
    }

    // Effects section (with subcategories)
    juce::Array<VstPluginCatalog::Entry> effects;
    for (const auto& entry : plugins)
    {
        if (entry.type == VstPluginCatalog::PluginType::Effect)
            effects.add(entry);
    }

    if (! effects.isEmpty())
    {
        // Group by category
        juce::StringArray categories;
        for (const auto& entry : effects)
        {
            if (entry.category.isNotEmpty() && ! categories.contains(entry.category, true))
                categories.add(entry.category);
        }
        categories.sort(true);

        for (const auto& category : categories)
        {
            juce::Array<VstPluginCatalog::Entry> categoryPlugins;
            for (const auto& entry : effects)
            {
                if (entry.category.equalsIgnoreCase(category))
                    categoryPlugins.add(entry);
            }

            auto* catSection = categorySections.add(new CategorySection("Effects: " + category));
            catSection->setPlugins(categoryPlugins);
            catSection->onLoadRequested = [this](const VstPluginCatalog::Entry& entry)
            {
                if (onLoadIntoInsertRequested)
                    onLoadIntoInsertRequested(entry);
            };
            catSection->onAssignRequested = [this](const VstPluginCatalog::Entry& entry)
            {
                if (onAssignNodeRequested)
                    onAssignNodeRequested(entry);
            };
            catSection->onToggled = [this] { resized(); };
            catSection->setCollapsed(true);
            pluginsHost.addAndMakeVisible(catSection);
        }
    }

    // Developers section
    juce::StringArray manufacturers;
    for (const auto& entry : plugins)
    {
        if (entry.manufacturer.isNotEmpty() && ! manufacturers.contains(entry.manufacturer, true))
            manufacturers.add(entry.manufacturer);
    }
    manufacturers.sort(true);

    for (const auto& manufacturer : manufacturers)
    {
        juce::Array<VstPluginCatalog::Entry> manufacturerPlugins;
        for (const auto& entry : plugins)
        {
            if (entry.manufacturer.equalsIgnoreCase(manufacturer))
                manufacturerPlugins.add(entry);
        }

        auto* devSection = categorySections.add(new CategorySection("Developer: " + manufacturer));
        devSection->setPlugins(manufacturerPlugins);
        devSection->onLoadRequested = [this](const VstPluginCatalog::Entry& entry)
        {
            if (onLoadIntoInsertRequested)
                onLoadIntoInsertRequested(entry);
        };
        devSection->onAssignRequested = [this](const VstPluginCatalog::Entry& entry)
        {
            if (onAssignNodeRequested)
                onAssignNodeRequested(entry);
        };
        devSection->onToggled = [this] { resized(); };
        devSection->setCollapsed(true);
        pluginsHost.addAndMakeVisible(devSection);
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
    importPathListButton.setBounds(topRow.removeFromLeft(140));
    topRow.removeFromLeft(8);
    rescanButton.setBounds(topRow.removeFromLeft(90));
    topRow.removeFromLeft(8);
    buildSamplePackButton.setBounds(topRow.removeFromLeft(160));
    area.removeFromTop(8);

    auto pathArea = area.removeFromTop(juce::jmin(180, juce::jmax(72, 18 + pathCards.size() * 52)));
    pathsViewport.setBounds(pathArea);
    area.removeFromTop(14);

    auto filterRow = area.removeFromTop(40);
    pluginsLabel.setBounds(filterRow.removeFromLeft(140));
    filterEditor.setBounds(filterRow.removeFromLeft(200));
    filterRow.removeFromLeft(8);
    formatFilter.setBounds(filterRow.removeFromLeft(120));
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
    for (auto* section : categorySections)
    {
        section->setBounds(0, pluginY, pluginWidth, section->getContentHeight());
        pluginY += section->getContentHeight() + 8;
    }
    pluginsHost.setSize(pluginWidth, juce::jmax(pluginY, pluginsViewport.getHeight()));
}
