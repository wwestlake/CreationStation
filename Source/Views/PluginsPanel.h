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
    std::function<void(int)> onRemovePathRequested;
    std::function<void()> onRescanRequested;
    std::function<void(const VstPluginCatalog::Entry&)> onLoadIntoInsertRequested;
    std::function<void(const VstPluginCatalog::Entry&)> onAssignNodeRequested;

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
        juce::TextButton removeButton { "Remove" };
    };

    class PluginCard final : public juce::Component
    {
    public:
        PluginCard();
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

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label targetLabel;
    juce::Label statusLabel;
    juce::Label pathsLabel;
    juce::Label pluginsLabel;
    juce::TextButton addPathButton { "Add Folder" };
    juce::TextButton rescanButton { "Rescan" };
    juce::Viewport pathsViewport;
    juce::Component pathsHost;
    juce::OwnedArray<PathCard> pathCards;
    juce::Viewport pluginsViewport;
    juce::Component pluginsHost;
    juce::OwnedArray<PluginCard> pluginCards;
    juce::StringArray searchPaths;
    juce::Array<VstPluginCatalog::Entry> plugins;
};
