#pragma once

#include <JuceHeader.h>
#include "../Audio/VstPluginCatalog.h"

class PluginBrowserList final : public juce::Component
{
public:
    PluginBrowserList();

    void setPlugins(const juce::Array<VstPluginCatalog::Entry>& entries);
    void setFilterText(const juce::String& filterText);

    const VstPluginCatalog::Entry* getSelectedEntry() const noexcept { return hasSelection ? &selectedEntry : nullptr; }

    std::function<void(const VstPluginCatalog::Entry&)> onSelectionChanged;
    std::function<void(const VstPluginCatalog::Entry&)> onEntryChosen;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    class Row final : public juce::Component
    {
    public:
        Row();
        void setEntry(const VstPluginCatalog::Entry& newEntry);
        const VstPluginCatalog::Entry& getEntry() const noexcept { return entry; }
        void setSelected(bool shouldBeSelected);
        bool isSelected() const noexcept { return selected; }

        std::function<void(Row&)> onClicked;
        std::function<void(Row&)> onDoubleClicked;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;

    private:
        VstPluginCatalog::Entry entry;
        bool selected = false;
    };

    class Section final : public juce::Component
    {
    public:
        Section(const juce::String& sectionName, bool isCreationStationSection = false);
        void setEntries(const juce::Array<VstPluginCatalog::Entry>& newEntries);
        void setCollapsed(bool shouldCollapse);
        bool isCollapsed() const noexcept { return collapsed; }
        int getContentHeight() const;
        void clearSelectionExcept(Row* except);

        std::function<void(Row&)> onRowActivated;
        std::function<void(Row&)> onRowChosen;
        std::function<void()> onToggled;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        juce::String name;
        bool isCreationStation;
        bool collapsed;
        juce::OwnedArray<Row> rows;
    };

    void rebuild();

    juce::Array<VstPluginCatalog::Entry> allPlugins;
    juce::String filter;
    juce::Viewport viewport;
    juce::Component host;
    juce::OwnedArray<Section> sections;
    VstPluginCatalog::Entry selectedEntry;
    bool hasSelection = false;
};
