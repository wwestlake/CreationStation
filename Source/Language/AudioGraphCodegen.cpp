#include "AudioGraphCodegen.h"

#include <variant>

#include "AudioNodeCatalog.h"
#include "node_system/type_registry.h"

namespace cw::audionodes
{

using namespace ce::node_system;

Graph BuildSineToOutputDemoGraph(const NodeLibraryRegistry& libraries, float levelValue)
{
    Graph graph("SignalLabDemo");

    Node* sine = libraries.AddNode(graph, NodeType::SineOscillator);
    Node* output = libraries.AddNode(graph, NodeType::Output);
    if (sine == nullptr || output == nullptr)
        return graph;

    sine->SetEditorPosition(0.0f, 0.0f);
    output->SetEditorPosition(220.0f, 0.0f);

    // Override the level pin's default with the caller-supplied value --
    // the registry only supplies the STARTING default (see
    // NodeTypeDescriptor's own comment); a real instance is free to carry
    // a different one, same as any other node in this system.
    if (Pin* levelPin = sine->FindPin(sine->Inputs()[1].id))
        levelPin->defaultValue = levelValue;

    graph.Connect(sine->Id(), sine->Outputs()[0].id, output->Id(), output->Inputs()[0].id);

    return graph;
}

namespace
{

// v1: the level pin is always a literal (unconnected) -- reading it
// straight off the pin's default value. A level fed by another node's
// output (a Value node, an envelope, ...) is real, specified work for a
// later slice once codegen actually walks connections into other nodes'
// outputs rather than just literals.
float ReadFloatDefault(const Pin& pin, float fallback)
{
    if (const float* value = std::get_if<float>(&pin.defaultValue))
        return *value;
    return fallback;
}

} // namespace

FrustGraphCompileResult GenerateAudioSource(const Graph& graph, const NodeLibraryRegistry& libraries,
                                             const std::string& functionName)
{
    std::vector<std::string> validationErrors;
    if (!ValidateAgainstRegistry(graph, libraries.TypeRegistry(), &validationErrors))
    {
        FrustGraphCompileResult result;
        result.ok = false;
        result.error = validationErrors.empty() ? "graph validation failed" : validationErrors.front();
        return result;
    }

    const Node* outputNode = nullptr;
    for (const auto& [id, node] : graph.Nodes())
    {
        if (node->TypeName() == NodeType::Output)
        {
            outputNode = node.get();
            break;
        }
    }
    if (outputNode == nullptr)
    {
        FrustGraphCompileResult result;
        result.error = "graph has no Output node";
        return result;
    }

    const Pin& signalInPin = outputNode->Inputs()[0];
    const Connection* feedingConnection = nullptr;
    for (const auto& connection : graph.Connections())
    {
        if (connection.toNode == outputNode->Id() && connection.toPin == signalInPin.id)
        {
            feedingConnection = &connection;
            break;
        }
    }
    if (feedingConnection == nullptr)
    {
        FrustGraphCompileResult result;
        result.error = "node " + std::to_string(outputNode->Id()) + " ('Output'): signalIn is not connected";
        return result;
    }

    const Node* source = graph.FindNode(feedingConnection->fromNode);
    if (source == nullptr || source->TypeName() != NodeType::SineOscillator)
    {
        FrustGraphCompileResult result;
        result.error = "node " + std::to_string(outputNode->Id())
                        + " ('Output'): only a SineOscillator source is supported in this v1 slice";
        return result;
    }

    const float phase = ReadFloatDefault(source->Inputs()[0], 0.5f);
    const float level = ReadFloatDefault(source->Inputs()[1], 0.0f);

    // CompileBehaviorGraphToFrust must never see the Output sink node --
    // TopologicalDataOrder walks every node in a graph unconditionally, and
    // the compiler's pure-node loop hard-errors on any node whose output
    // count isn't exactly 1 (Output has zero). Build a fresh, Output-free
    // graph containing only a re-created source node carrying the same
    // level, and point resultNode/resultPin straight at it -- the same
    // "one real computation node, no sink node in the compiled graph"
    // shape Creation Engine's own node-codegen already uses.
    Graph compileGraph("SignalLabCompile");
    Node* compileSource = libraries.AddNode(compileGraph, NodeType::SineOscillator);
    if (compileSource == nullptr)
    {
        FrustGraphCompileResult result;
        result.error = "could not re-create the source node for compilation";
        return result;
    }
    if (Pin* phasePin = compileSource->FindPin(compileSource->Inputs()[0].id))
        phasePin->defaultValue = phase;
    if (Pin* levelPin = compileSource->FindPin(compileSource->Inputs()[1].id))
        levelPin->defaultValue = level;

    FrustGraphCompileOptions options;
    options.functionName = functionName;
    options.resultNode = compileSource->Id();
    options.resultPin = compileSource->Outputs()[0].id;
    options.sourceModules = { kSignalLabFrustModule };
    options.manifestJson = "{\"name\":\"signal_lab_compiled\",\"version\":\"0.1.0\"}";

    return CompileBehaviorGraphToFrust(compileGraph, libraries, options);
}

} // namespace cw::audionodes
