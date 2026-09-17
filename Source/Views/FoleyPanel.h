#pragma once

#include <JuceHeader.h>
#include <creation/node_editor_ui/NodeGraphComponent.h>
#include <creation/node_editor_ui/NodeInspector.h>
#include <creation/node_editor_ui/NodePalette.h>
#include <node_system/graph.h>
#include <node_system/frgraph_serialization.h>

#include "FoleyNodeCatalog.h"

// The Foley workspace tab: an execution-style (UE4-Blueprint-like) node graph editor for
// sequencing/choice logic, compiled to FRust. Reuses the generic, domain-agnostic node-editor UI
// (shared/NodeEditorUI, itself ported from CreationEngine's own node editor) with Foley's own
// node catalog (FoleyNodeCatalog.h) - per the suite's explicit stance, share the
// editing machinery across domains, never merge the domain-specific node catalogs.
//
// Generate provides source inspection; Build Pod sends the same graph through
// Station's VFS-backed Frate service and registers its compiler-reflected node
// functions back into this editor's shared node catalog.
//
// PUBLICLY inherits juce::DragAndDropContainer (must be public, not private - JUCE's
// DragAndDropContainer::findParentDragContainerFor walks the component tree using
// dynamic_cast<DragAndDropContainer*>, which only succeeds through a public base) so dragging a
// node type from the palette actually lands on the graph canvas - see NodePalette's own header
// comment, and CreationEngine's LogicPanel, which this mirrors exactly for the same reason.
class FoleyPanel final : public juce::Component, public juce::DragAndDropContainer
{
public:
    FoleyPanel();

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Save/load this graph as one named project asset, via the shared
    // ProjectAssetService::saveGeneratedAsset mechanism -- see the Setup toolbar button.
    // Uses node_system's own .frgraph text format (frgraph_serialization.h), the same byte-stable
    // round-trip format graphs already use elsewhere in the suite.
    juce::String serializeGraph() const;
    bool loadGraph(const juce::String& frgraphText, juce::String& errorMessage);

    std::function<void(const juce::String& name)> onSetupSaveRequested;
    std::function<void()> onSetupLoadRequested;
    std::function<void(const juce::String& podName, const juce::String& generatedSource)> onPodBuildRequested;
    std::function<void(const juce::String& podName, const juce::String& version)> onRegistryPodLoadRequested;

    ce::node_system::NodeLibraryRegistry& nodeLibraries() noexcept { return libraries_; }
    void refreshNodePalette();
    void setBuildStatus(const juce::String& status, bool success);

private:
    void generateSource();
    juce::String generateSourceText(bool exposeAsNodes, juce::String& error) const;
    void requestPodBuild();
    void requestRegistryPodLoad();
    void showSetupMenu();

    ce::node_system::NodeLibraryRegistry libraries_ = cw::foleynodes::BuildFoleyNodeCatalog();
    ce::node_system::Graph graph_ { "Foley" };

    juce::Label titleLabel_ { {}, "Foley" };
    juce::Label hintLabel_ { {}, "Drag a node from the palette onto the canvas. Right-drag to pan, wheel to zoom." };
    juce::TextButton generateButton_ { "Generate FRust" };
    juce::TextButton buildPodButton_ { "Build Pod" };
    juce::TextButton registryPodButton_ { "Add Pod" };
    juce::TextButton setupMenuButton_ { "Setup" };
    juce::Label statusLabel_;
    juce::TextEditor sourceView_;

    creation::node_editor_ui::NodePalette palette_ { libraries_.TypeRegistry() };
    creation::node_editor_ui::NodeGraphComponent graphComponent_ { graph_, libraries_.TypeRegistry() };
    creation::node_editor_ui::NodeInspector inspector_ { graph_ };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoleyPanel)
};
