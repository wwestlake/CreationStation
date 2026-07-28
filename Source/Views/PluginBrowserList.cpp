#include "PluginBrowserList.h"

namespace
{
juce::Colour cardColour() { return juce::Colour(0xff1a2030); }
juce::Colour cardSelectedColour() { return juce::Colour(0xff1f5f86); }
juce::Colour borderColour() { return juce::Colour(0xff2a3445); }
juce::Colour dimText() { return juce::Colour(0xff8ea0b7); }
juce::Colour creationStationAccent() { return juce::Colour(0xff56f4ff); }

constexpr int kHeaderHeight = 32;
constexpr int kRowHeight = 40;

// The manufacturer name every in-house plugin actually reports (COMPANY_NAME in
// cmake/CreationStationPlugin.cmake) - used to pull them into their own always-expanded section at
// the top of the browser. They still also appear under their normal category/"Developer" sections
// below; this top section is an additional filtered view, not an exclusive container.
const juce::String& creationStationManufacturer()
{
    static const juce::String name = "LagDaemon Software";
    return name;
}
}

PluginBrowserList::Row::Row()
{
    setInterceptsMouseClicks(true, false);
}

void PluginBrowserList::Row::setEntry(const VstPluginCatalog::Entry& newEntry)
{
    entry = newEntry;
    repaint();
}

void PluginBrowserList::Row::setSelected(bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void PluginBrowserList::Row::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f, 1.0f);
    g.setColour(selected ? cardSelectedColour() : cardColour());
    g.fillRoundedRectangle(bounds, 6.0f);
    if (selected)
    {
        g.setColour(creationStationAccent());
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    auto textArea = bounds.reduced(10.0f, 2.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText(entry.name, textArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    if (entry.manufacturer.isNotEmpty())
    {
        g.setColour(dimText());
        g.setFont(juce::Font(10.0f));
        g.drawText(entry.manufacturer, textArea, juce::Justification::topLeft, true);
    }
}

void PluginBrowserList::Row::mouseDown(const juce::MouseEvent&)
{
    if (onClicked)
        onClicked(*this);
}

void PluginBrowserList::Row::mouseDoubleClick(const juce::MouseEvent&)
{
    if (onDoubleClicked)
        onDoubleClicked(*this);
}

PluginBrowserList::Section::Section(const juce::String& sectionName, bool isCreationStationSection)
    : name(sectionName), isCreationStation(isCreationStationSection), collapsed(! isCreationStationSection)
{
}

void PluginBrowserList::Section::setEntries(const juce::Array<VstPluginCatalog::Entry>& newEntries)
{
    rows.clear(true);

    for (const auto& entry : newEntries)
    {
        auto* row = rows.add(new Row());
        row->setEntry(entry);
        row->onClicked = [this](Row& r)
        {
            clearSelectionExcept(&r);
            r.setSelected(true);
            if (onRowActivated)
                onRowActivated(r);
        };
        row->onDoubleClicked = [this](Row& r)
        {
            if (onRowChosen)
                onRowChosen(r);
        };
        addAndMakeVisible(row);
    }

    resized();
}

void PluginBrowserList::Section::clearSelectionExcept(Row* except)
{
    for (auto* row : rows)
        if (row != except)
            row->setSelected(false);
}

void PluginBrowserList::Section::setCollapsed(bool shouldCollapse)
{
    collapsed = shouldCollapse;
    for (auto* row : rows)
        row->setVisible(! collapsed);
    repaint();
}

void PluginBrowserList::Section::paint(juce::Graphics& g)
{
    auto headerArea = getLocalBounds().removeFromTop(kHeaderHeight).toFloat();
    if (isCreationStation)
        g.setColour(creationStationAccent().withAlpha(0.1f));
    else
        g.setColour(borderColour().withAlpha(0.3f));
    g.fillRoundedRectangle(headerArea, 6.0f);

    g.setColour(isCreationStation ? creationStationAccent() : borderColour());
    g.drawRoundedRectangle(headerArea, 6.0f, 1.0f);

    auto textBounds = headerArea.reduced(10.0f, 0.0f);
    g.setColour(isCreationStation ? creationStationAccent() : juce::Colours::white);
    g.setFont(juce::Font(12.0f).boldened());
    g.drawText(collapsed ? "\xE2\x96\xB6" : "\xE2\x96\xBC", textBounds.removeFromLeft(16), juce::Justification::centredLeft, false);
    g.drawText(name, textBounds, juce::Justification::centredLeft, false);

    g.setColour(dimText());
    g.setFont(juce::Font(10.0f));
    g.drawText("(" + juce::String(rows.size()) + ")", textBounds.removeFromRight(46), juce::Justification::centredRight, false);
}

void PluginBrowserList::Section::resized()
{
    if (collapsed)
    {
        setSize(getWidth(), kHeaderHeight);
        return;
    }

    auto y = kHeaderHeight;
    for (auto* row : rows)
    {
        row->setBounds(0, y, getWidth(), kRowHeight);
        y += kRowHeight;
    }

    setSize(getWidth(), y);
}

void PluginBrowserList::Section::mouseUp(const juce::MouseEvent& event)
{
    if (event.y > kHeaderHeight)
        return;

    setCollapsed(! collapsed);
    resized();

    if (onToggled)
        onToggled();
}

int PluginBrowserList::Section::getContentHeight() const
{
    if (collapsed)
        return kHeaderHeight;

    return kHeaderHeight + (rows.size() * kRowHeight);
}

PluginBrowserList::PluginBrowserList()
{
    viewport.setViewedComponent(&host, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void PluginBrowserList::setPlugins(const juce::Array<VstPluginCatalog::Entry>& entries)
{
    allPlugins = entries;
    hasSelection = false;
    rebuild();
}

void PluginBrowserList::setFilterText(const juce::String& filterText)
{
    filter = filterText.trim().toLowerCase();
    rebuild();
}

void PluginBrowserList::rebuild()
{
    sections.clear(true);

    juce::Array<VstPluginCatalog::Entry> filtered;
    for (const auto& entry : allPlugins)
        if (filter.isEmpty() || entry.name.toLowerCase().contains(filter))
            filtered.add(entry);

    auto addSection = [this](const juce::String& sectionName, const juce::Array<VstPluginCatalog::Entry>& sectionEntries, bool isCS, bool expandedByDefault)
    {
        if (sectionEntries.isEmpty())
            return;

        auto* section = sections.add(new Section(sectionName, isCS));
        section->setEntries(sectionEntries);
        section->onRowActivated = [this](Row& row)
        {
            for (auto* other : sections)
                other->clearSelectionExcept(&row);

            selectedEntry = row.getEntry();
            hasSelection = true;
            if (onSelectionChanged)
                onSelectionChanged(selectedEntry);
        };
        section->onRowChosen = [this](Row& row)
        {
            selectedEntry = row.getEntry();
            hasSelection = true;
            if (onEntryChosen)
                onEntryChosen(row.getEntry());
        };
        section->onToggled = [this] { resized(); };
        section->setCollapsed(! expandedByDefault);
        host.addAndMakeVisible(section);
    };

    juce::Array<VstPluginCatalog::Entry> csPlugins, instruments, effects;
    for (const auto& entry : filtered)
    {
        // Independent checks, not if/else-if: a Creation Station plugin still lands in its normal
        // Instruments/Effects bucket below (and its Developer bucket further down) in addition to
        // the dedicated top section - every section here is a filtered view of the same catalog,
        // not an exclusive container.
        if (entry.manufacturer.equalsIgnoreCase(creationStationManufacturer()))
            csPlugins.add(entry);

        if (entry.type == VstPluginCatalog::PluginType::Instrument)
            instruments.add(entry);
        else if (entry.type == VstPluginCatalog::PluginType::Effect)
            effects.add(entry);
    }

    addSection("Creation Station Plugins", csPlugins, true, true);
    addSection("Instruments", instruments, false, false);

    juce::StringArray categories;
    for (const auto& entry : effects)
        if (entry.category.isNotEmpty() && ! categories.contains(entry.category, true))
            categories.add(entry.category);
    categories.sort(true);

    for (const auto& category : categories)
    {
        juce::Array<VstPluginCatalog::Entry> categoryPlugins;
        for (const auto& entry : effects)
            if (entry.category.equalsIgnoreCase(category))
                categoryPlugins.add(entry);

        addSection("Effects: " + category, categoryPlugins, false, false);
    }

    juce::StringArray manufacturers;
    for (const auto& entry : filtered)
        if (entry.manufacturer.isNotEmpty() && ! manufacturers.contains(entry.manufacturer, true))
            manufacturers.add(entry.manufacturer);
    manufacturers.sort(true);

    for (const auto& manufacturer : manufacturers)
    {
        juce::Array<VstPluginCatalog::Entry> manufacturerPlugins;
        for (const auto& entry : filtered)
            if (entry.manufacturer.equalsIgnoreCase(manufacturer))
                manufacturerPlugins.add(entry);

        addSection("Developer: " + manufacturer, manufacturerPlugins, false, false);
    }

    resized();
}

void PluginBrowserList::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d141d));
}

void PluginBrowserList::resized()
{
    auto width = juce::jmax(240, viewport.getWidth() - 24);
    auto y = 0;
    for (auto* section : sections)
    {
        section->setBounds(0, y, width, section->getContentHeight());
        y += section->getContentHeight() + 4;
    }
    host.setSize(width, juce::jmax(y, viewport.getHeight()));
    viewport.setBounds(getLocalBounds());
}
