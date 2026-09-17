#include "FoleyPanel.h"

#include <algorithm>
#include <map>
#include <sstream>

#include <node_system/frust_codegen.h>

FoleyPanel::FoleyPanel()
{
    titleLabel_.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel_);

    hintLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    hintLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(hintLabel_);

    generateButton_.onClick = [this] { generateSource(); };
    addAndMakeVisible(generateButton_);

    buildPodButton_.onClick = [this] { requestPodBuild(); };
    buildPodButton_.setTooltip("Build this graph as a reusable FRust pod in the current project");
    addAndMakeVisible(buildPodButton_);

    registryPodButton_.onClick = [this] { requestRegistryPodLoad(); };
    registryPodButton_.setTooltip("Resolve a published Frate pod and add its reflected nodes to this palette");
    addAndMakeVisible(registryPodButton_);

    setupMenuButton_.onClick = [this] { showSetupMenu(); };
    setupMenuButton_.setTooltip("Save or load this node setup as a named project asset");
    addAndMakeVisible(setupMenuButton_);

    statusLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(statusLabel_);

    sourceView_.setMultiLine(true);
    sourceView_.setReadOnly(true);
    sourceView_.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    sourceView_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0e1218));
    sourceView_.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd7e3f0));
    sourceView_.setText("Generated FRust source will appear here.", juce::dontSendNotification);
    addAndMakeVisible(sourceView_);

    addAndMakeVisible(palette_);

    graphComponent_.onSelectionChanged = [this](ce::node_system::NodeId id)
    {
        inspector_.SetSelectedNode(id);
    };
    addAndMakeVisible(graphComponent_);

    addAndMakeVisible(inspector_);
}

void FoleyPanel::generateSource()
{
    juce::String error;
    const auto source = generateSourceText(false, error);
    if (source.isEmpty())
    {
        sourceView_.setText(error, juce::dontSendNotification);
        statusLabel_.setText("Generation failed - see errors below.", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
        return;
    }
    sourceView_.setText(source, juce::dontSendNotification);
    statusLabel_.setText("Generated OK.", juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff67e8a5));
}

juce::String FoleyPanel::generateSourceText(bool exposeAsNodes, juce::String& error) const
{
    // One compiled function per On Trigger node -- same "each Event node is
    // its own entry point, concatenated into one file" shape CreationEngine's
    // PodEditorPanel uses for its own multi-entry Behavior graphs, since a
    // Foley setup legitimately has more than one independent cue.
    std::vector<ce::node_system::NodeId> triggerNodes;
    for (const auto& [id, node] : graph_.Nodes())
        if (node->TypeName() == cw::foleynodes::NodeType::OnTrigger)
            triggerNodes.push_back(id);

    if (triggerNodes.empty())
    {
        error = "Add an On Trigger node to generate a cue.";
        return {};
    }
    std::sort(triggerNodes.begin(), triggerNodes.end());

    std::map<std::string, std::string> externsByName;
    std::vector<std::string> functionBodies;
    for (const auto triggerId : triggerNodes)
    {
        ce::node_system::FrustGraphCompileOptions options;
        options.functionName = "on_trigger_" + std::to_string(triggerId);
        options.entryNode = triggerId;
        options.emitManifestAndImports = false;
        options.exposeAsNode = exposeAsNodes;

        const auto result = ce::node_system::CompileBehaviorGraphToFrust(graph_, libraries_, options);
        if (!result.ok)
        {
            error = juce::String(result.error);
            return {};
        }
        functionBodies.push_back(result.source);
        for (const auto& decl : result.externDeclarations)
            externsByName[decl] = decl;
    }

    std::ostringstream combined;
    for (const auto& [name, decl] : externsByName) { (void) name; combined << decl; }
    if (!externsByName.empty())
        combined << "\n";
    for (const auto& body : functionBodies)
        combined << body << "\n";

    return combined.str();
}

void FoleyPanel::requestPodBuild()
{
    juce::String error;
    const auto source = generateSourceText(true, error);
    if (source.isEmpty())
    {
        setBuildStatus(error, false);
        return;
    }

    auto* prompt = new juce::AlertWindow("Build FRust Pod", "Name this reusable node pod:",
                                         juce::MessageBoxIconType::QuestionIcon);
    prompt->addTextEditor("podName", "foley-cues");
    prompt->addButton("Build", 1);
    prompt->addButton("Cancel", 0);
    auto safeThis = juce::Component::SafePointer<FoleyPanel>(this);
    prompt->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, prompt, source](int result) mutable
        {
            std::unique_ptr<juce::AlertWindow> dialog(prompt);
            if (result != 1 || safeThis == nullptr) return;
            const auto name = dialog->getTextEditorContents("podName").trim().toLowerCase().replace(" ", "-");
            if (safeThis->onPodBuildRequested) safeThis->onPodBuildRequested(name, source);
        }), true);
}

void FoleyPanel::requestRegistryPodLoad()
{
    auto* prompt = new juce::AlertWindow("Add Frate Pod", "Load reflected nodes from the pod registry:",
                                         juce::MessageBoxIconType::QuestionIcon);
    prompt->addTextEditor("podName", "frust_dsp");
    prompt->addTextEditor("version", "0.1.0");
    prompt->addButton("Add", 1);
    prompt->addButton("Cancel", 0);
    auto safeThis = juce::Component::SafePointer<FoleyPanel>(this);
    prompt->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, prompt](int result) mutable
        {
            std::unique_ptr<juce::AlertWindow> dialog(prompt);
            if (result != 1 || safeThis == nullptr) return;
            if (safeThis->onRegistryPodLoadRequested)
                safeThis->onRegistryPodLoadRequested(dialog->getTextEditorContents("podName").trim(),
                                                      dialog->getTextEditorContents("version").trim());
        }), true);
}

void FoleyPanel::refreshNodePalette()
{
    palette_.RefreshFromRegistry();
}

void FoleyPanel::setBuildStatus(const juce::String& status, bool success)
{
    statusLabel_.setText(status, juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId,
                           success ? juce::Colour(0xff67e8a5) : juce::Colour(0xffff6b6b));
}

juce::String FoleyPanel::serializeGraph() const
{
    return ce::node_system::SerializeGraph(graph_);
}

bool FoleyPanel::loadGraph(const juce::String& frgraphText, juce::String& errorMessage)
{
    std::string errorOut;
    auto loaded = ce::node_system::DeserializeGraph(frgraphText.toStdString(), errorOut);
    if (loaded == nullptr)
    {
        errorMessage = juce::String(errorOut);
        return false;
    }

    graph_ = std::move(*loaded);
    graphComponent_.GraphReplaced();
    return true;
}

void FoleyPanel::showSetupMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save Setup...");
    menu.addItem(2, "Load Setup...");

    auto area = setupMenuButton_.getScreenBounds();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
                       [this](int result)
                       {
                           if (result == 1)
                           {
                               auto* prompt = new juce::AlertWindow("Save Foley Setup",
                                                                    "Enter a name for this node setup:",
                                                                    juce::MessageBoxIconType::QuestionIcon);
                               prompt->addTextEditor("setupName", "Foley Setup");
                               prompt->addButton("Save", 1);
                               prompt->addButton("Cancel", 0);

                               auto options = juce::Component::SafePointer<FoleyPanel>(this);
                               prompt->enterModalState(true, juce::ModalCallbackFunction::create([options, prompt](int promptResult) mutable
                               {
                                   std::unique_ptr<juce::AlertWindow> dialog(prompt);
                                   if (promptResult != 1 || options == nullptr)
                                       return;

                                   auto name = dialog->getTextEditorContents("setupName").trim();
                                   if (name.isEmpty())
                                       return;

                                   if (options->onSetupSaveRequested)
                                       options->onSetupSaveRequested(name);
                               }), true);
                           }
                           else if (result == 2)
                           {
                               if (onSetupLoadRequested)
                                   onSetupLoadRequested();
                           }
                       });
}

void FoleyPanel::resized()
{
    auto area = getLocalBounds().reduced(16, 12);

    auto header = area.removeFromTop(48);
    titleLabel_.setBounds(header.removeFromTop(24));
    hintLabel_.setBounds(header);

    area.removeFromTop(8);

    auto footer = area.removeFromBottom(160);
    auto footerHeader = footer.removeFromTop(24);
    generateButton_.setBounds(footerHeader.removeFromLeft(130));
    footerHeader.removeFromLeft(8);
    buildPodButton_.setBounds(footerHeader.removeFromLeft(100));
    footerHeader.removeFromLeft(8);
    registryPodButton_.setBounds(footerHeader.removeFromLeft(90));
    footerHeader.removeFromLeft(8);
    setupMenuButton_.setBounds(footerHeader.removeFromLeft(90));
    footerHeader.removeFromLeft(8);
    statusLabel_.setBounds(footerHeader);
    footer.removeFromTop(4);
    sourceView_.setBounds(footer);

    area.removeFromBottom(8);

    auto paletteArea = area.removeFromLeft(180);
    palette_.setBounds(paletteArea);
    area.removeFromLeft(8);

    auto inspectorArea = area.removeFromRight(220);
    inspector_.setBounds(inspectorArea);
    area.removeFromRight(8);

    graphComponent_.setBounds(area);
}

void FoleyPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
}
