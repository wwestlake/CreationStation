#include "AudioNodeCatalog.h"

namespace cw::audionodes
{

using ce::node_system::DataType;
using ce::node_system::Domain;
using ce::node_system::GraphTarget;
using ce::node_system::NodeLibraryDescriptor;
using ce::node_system::NodeLibraryRegistry;
using ce::node_system::NodeTypeDescriptor;
using ce::node_system::PinKind;
using ce::node_system::PinSignature;
using ce::node_system::PinTypeDesc;

namespace
{

PinTypeDesc SignalPin()
{
    return PinTypeDesc{ PinKind::Data, DataType::AudioSignal };
}

PinTypeDesc FloatPin()
{
    return PinTypeDesc{ PinKind::Data, DataType::Float };
}

PinSignature Signal(const char* name)
{
    return PinSignature{ name, SignalPin(), {} };
}

PinSignature Float(const char* name, float defaultValue)
{
    return PinSignature{ name, FloatPin(), defaultValue };
}

} // namespace

NodeLibraryRegistry BuildAudioNodeCatalog()
{
    NodeLibraryRegistry libraries;

    NodeLibraryDescriptor library;
    library.id = "signal_lab.nodes";
    library.displayName = "Signal Lab Nodes";
    library.target = GraphTarget::Behavior;
    library.frustSourceModules = { kSignalLabFrustModule };

    NodeTypeDescriptor sineOscillator;
    sineOscillator.typeName = NodeType::SineOscillator;
    sineOscillator.domain = Domain::Audio;
    sineOscillator.inputs = { Float(PinName::Phase, 0.5f), Float(PinName::Level, 0.65f) };
    sineOscillator.outputs = { Signal(PinName::SignalOut) };
    sineOscillator.displayName = "Sine Oscillator";
    sineOscillator.category = "Signal Lab";
    // Real FRust function in BuiltInPlugins/SignalLabNodesLibrary.frust --
    // this is metadata CompileBehaviorGraphToFrust reads to emit the actual
    // call; it isn't interpreted at all until a graph actually reaches this
    // node type during compilation.
    sineOscillator.frustEntryPoint = "sine_oscillator";
    library.nodeTypes.push_back(std::move(sineOscillator));

    // Output is a graph-EDITOR-side sink marker only -- it must never be
    // handed to CompileBehaviorGraphToFrust as part of the compiled graph.
    // TopologicalDataOrder walks every node in a graph unconditionally, and
    // the compiler's pure-node loop hard-errors on any node whose output
    // count isn't exactly 1 -- Output has zero. AudioGraphCodegen walks
    // back from Output's input pin to find the real source node and builds
    // a SEPARATE, Output-free graph for the actual compile call (resultNode/
    // resultPin point straight at the source), exactly the same "single
    // real computation node, no sink node in the compiled graph" shape
    // Creation Engine's own node-codegen smoke tests already use. Output
    // still needs a registered NodeTypeDescriptor (with no frustEntryPoint)
    // so AddNode/the editor can place and validate it like any other type.
    NodeTypeDescriptor output;
    output.typeName = NodeType::Output;
    output.domain = Domain::Core;
    output.inputs = { Signal(PinName::SignalIn) };
    output.displayName = "Output";
    output.category = "Signal Lab";
    library.nodeTypes.push_back(std::move(output));

    libraries.Register(std::move(library));
    return libraries;
}

} // namespace cw::audionodes
