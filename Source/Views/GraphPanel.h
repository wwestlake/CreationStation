#pragma once

#include <JuceHeader.h>
#include "../Language/SignalGraph.h"

class GraphPanel final : public juce::Component
{
public:
    GraphPanel();

    void setDrive(float amount);
    void setInput(float amount);
    void setTone(float amount);
    void setEcho(float amount);
    void setWidth(float amount);
    void setEnabled(bool shouldEnable);
    void applyAiMacro(const juce::String& macroName);
    void setAssignedVstPlugin(const juce::String& pluginName, const juce::String& pluginPath);
    void clearAssignedVstPlugin();
    void setVstMix(float amount);
    void setVstEnabled(bool shouldEnable);
    void setOscillatorFrequency(float hz);
    void setOutputLevel(float amount);
    float getVstMix() const noexcept;
    bool isVstEnabled() const noexcept;
    float getOscillatorFrequency() const noexcept;
    float getOutputLevel() const noexcept;
    juce::String getAssignedVstPluginPath() const;
    juce::String getSelectedNodeName() const;
    bool hasNode(const juce::String& name) const;
    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);

    std::function<void(float)> onDriveChanged;
    std::function<void(float)> onInputChanged;
    std::function<void(float)> onToneChanged;
    std::function<void(float)> onEchoChanged;
    std::function<void(float)> onWidthChanged;
    std::function<void(bool)> onEnabledChanged;
    std::function<void(float)> onOscillatorFrequencyChanged;
    std::function<void(float)> onOutputLevelChanged;
    std::function<void(float)> onVstMixChanged;
    std::function<void(bool)> onVstEnabledChanged;
    std::function<void(const juce::String&)> onNodeDeleted;
    std::function<void()> onAssignVstPluginRequested;
    std::function<void()> onOpenAssignedVstRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct PaletteCard final : public juce::Component
    {
        explicit PaletteCard(const cw::NodeTemplate& nodeTemplate);
        void setAlreadyPlaced(bool alreadyPlaced);
        void paint(juce::Graphics& g) override;
        void resized() override;

        std::function<void(const cw::NodeTemplate&)> onAddRequested;
        cw::NodeTemplate nodeTemplate;
        juce::TextButton addButton { "Add" };
        bool alreadyPlaced = false;
    };

    struct NodeCard final : public juce::Component
    {
        explicit NodeCard(const cw::NodeTemplate& nodeTemplate);

        void setAmount(float newAmount);
        void setEnabled(bool shouldEnable);
        void setSelected(bool shouldSelect);
        bool isVstHost() const noexcept;
        void setAssignedPluginName(const juce::String& pluginName);
        void setCustomPosition(juce::Point<int> newPosition);
        juce::Point<int> getCustomPosition() const noexcept;
        bool hasCustomPosition() const noexcept;
        bool hasInputPort() const noexcept;
        bool hasOutputPort() const noexcept;
        juce::Point<float> getInputPortCentre() const noexcept;
        juce::Point<float> getOutputPortCentre() const noexcept;

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        std::function<void()> onSelected;
        std::function<void()> onMoved;
        std::function<void()> onDeleteRequested;
        std::function<void()> onAssignVstRequested;
        std::function<void()> onOpenVstRequested;
        std::function<void(const juce::Point<float>&)> onConnectionDragStarted;
        std::function<void(const juce::Point<float>&)> onConnectionDragged;
        std::function<void(const juce::Point<float>&)> onConnectionDropped;
        cw::NodeTemplate nodeTemplate;
        juce::Label title;
        juce::Label subtitle;
        juce::Label categoryLabel;
        juce::Slider amountSlider;
        juce::ToggleButton enableToggle { "On" };
        juce::TextButton deleteButton { "X" };
        juce::Label pluginLabel;
        juce::TextButton assignButton { "Assign VST" };
        juce::TextButton openButton { "Open UI" };
        juce::ComponentDragger dragger;
        juce::ComponentBoundsConstrainer constrainer;
        float amount = 0.5f;
        bool enabled = true;
        bool selected = false;
        bool customPositionSet = false;
        bool connectionDragActive = false;
    };

    struct Connection
    {
        juce::String from;
        juce::String to;
    };

    struct Canvas final : public juce::Component
    {
        explicit Canvas(GraphPanel& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics& g) override;
        GraphPanel& owner;
    };

    void addPlacedNode(const cw::NodeTemplate& nodeTemplate);
    void clearPlacedNodes();
    void removePlacedNodeByName(const juce::String& name);
    void setSelectedNodeIndex(int index);
    void refreshSelectionSummary();
    void refreshPaletteState();
    void refreshNodeCallbacks();
    void syncNamedNodeCallbacks(NodeCard& card);
    void layoutPlacedNodes();
    void updateCanvasExtent();
    juce::Array<NodeCard*> getOrderedNodesForCategory(cw::NodeCategory category) const;
    juce::Array<NodeCard*> getChainOrderedNodes() const;
    juce::Array<Connection> getEffectiveConnections() const;
    bool connectionExists(const juce::String& from, const juce::String& to) const;
    void beginManualConnection(const juce::String& sourceNode, juce::Point<float> startPoint);
    void dragManualConnection(juce::Point<float> currentPoint);
    void finishManualConnection(juce::Point<float> dropPoint);
    NodeCard* findNodeAtInputPort(juce::Point<float> canvasPoint) const;
    void removeConnectionsForNode(const juce::String& nodeName);
    int findPlacedNodeIndexByName(const juce::String& name) const;
    NodeCard* findPlacedNodeByName(const juce::String& name) const;
    cw::NodeTemplate findTemplateByName(const juce::String& name) const;
    juce::Rectangle<int> getCanvasContentBounds() const;

    juce::Label headerLabel;
    juce::Label subtitleLabel;
    juce::ToggleButton graphEnabledToggle { "Graph On" };
    juce::ToggleButton autoConnectToggle { "Auto Connect" };
    juce::TextButton clearCanvasButton { "Clear Canvas" };
    juce::Label vstStatusLabel;
    juce::Label toolboxTitleLabel;
    juce::Label selectionTitleLabel;
    juce::Label selectionBodyLabel;
    juce::Label selectionMetaLabel;
    juce::Label detailControlLabel;
    juce::Slider detailControlSlider;
    juce::Viewport paletteViewport;
    juce::Component paletteHost;
    juce::OwnedArray<PaletteCard> paletteCards;
    juce::Viewport canvasViewport;
    Canvas canvas;
    juce::OwnedArray<NodeCard> placedNodeCards;
    juce::Array<Connection> manualConnections;
    juce::String assignedVstPluginName;
    juce::String assignedVstPluginPath;
    int selectedNodeIndex = -1;
    bool autoConnectEnabled = true;
    bool manualConnectionInProgress = false;
    juce::String manualConnectionSource;
    juce::Point<float> manualConnectionStart;
    juce::Point<float> manualConnectionCurrent;
};
