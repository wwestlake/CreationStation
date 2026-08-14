#pragma once

#include <JuceHeader.h>
#include "../Audio/VstPluginCatalog.h"

class PluginsPanel final : public juce::Component
{
public:
    PluginsPanel();

    void setSearchPaths(const juce::StringArray& paths);
    void setPlugins(const juce::Array<VstPluginCatalog::Entry>& entries);
    void setStatusText(const juce::String& text);
    void setInsertTargetDescription(const juce::String& text);

    std::function<void()> onAddPathRequested;
    std::function<void()> onImportPathListRequested;
    std::function<void(int)> onRemovePathRequested;
    std::function<void()> onRescanRequested;
    std::function<void(const VstPluginCatalog::Entry&)> onLoadIntoInsertRequested;
    std::function<void(const VstPluginCatalog::Entry&)> onAssignNodeRequested;
    std::function<void()> onBuildSamplePackRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PathCard final : public juce::Component
    {
    public:
        PathCard();
        void setPath(const juce::String& newPath, int newIndex);
        std::function<void(int)> onRemoveRequested;
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        juce::String path;
        int index = -1;
        bool missing = false;
        juce::TextButton removeButton { "Remove" };
    };

    class PluginItem final : public juce::Component
    {
    public:
        PluginItem();
        void setEntry(const VstPluginCatalog::Entry& newEntry);
        std::function<void(const VstPluginCatalog::Entry&)> onLoadRequested;
        std::function<void(const VstPluginCatalog::Entry&)> onAssignRequested;
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        VstPluginCatalog::Entry entry;
        juce::TextButton loadButton { "Load To Insert" };
        juce::TextButton assignButton { "Assign To VST Node" };
    };

    class CategorySection final : public juce::Component
    {
    public:
        CategorySection(const juce::String& categoryName, bool isCreationStation = false);
        void setPlugins(const juce::Array<VstPluginCatalog::Entry>& pluginList);
        void setCollapsed(bool shouldCollapse);
        bool isCollapsed() const { return collapsed; }
        std::function<void(const VstPluginCatalog::Entry&)> onLoadRequested;
        std::function<void(const VstPluginCatalog::Entry&)> onAssignRequested;
        std::function<void()> onToggled;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseUp(const juce::MouseEvent& event) override;
        int getContentHeight() const;

    private:
        juce::String categoryName;
        bool isCreationStation;
        bool collapsed = false;
        juce::OwnedArray<PluginItem> pluginItems;
    };

    void rebuildPluginsList();

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label targetLabel;
    juce::Label statusLabel;
    juce::Label pathsLabel;
    juce::Label pluginsLabel;
    juce::TextButton addPathButton { "Add Folder" };
    juce::TextButton importPathListButton { "Import Path List" };
    juce::TextButton rescanButton { "Rescan" };
    juce::TextButton buildSamplePackButton { "Build Sample Pack..." };
    juce::TextEditor filterEditor;
    juce::ComboBox formatFilter;
    juce::Viewport pathsViewport;
    juce::Component pathsHost;
    juce::OwnedArray<PathCard> pathCards;
    juce::Viewport pluginsViewport;
    juce::Component pluginsHost;
    juce::OwnedArray<CategorySection> categorySections;
    juce::StringArray searchPaths;
    juce::Array<VstPluginCatalog::Entry> plugins;
};
