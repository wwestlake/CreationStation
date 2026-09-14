#include "FoleyNodeCatalog.h"

#include "node_system/core_control_flow.h"

namespace cw::foleynodes
{

using ce::node_system::DataType;
using ce::node_system::Domain;
using ce::node_system::GraphTarget;
using ce::node_system::NodeLibraryDescriptor;
using ce::node_system::NodeLibraryRegistry;
using ce::node_system::NodeTypeDescriptor;
using ce::node_system::NodeTypeRegistry;
using ce::node_system::PinKind;
using ce::node_system::PinSignature;
using ce::node_system::PinTypeDesc;

namespace
{

PinTypeDesc ExecPin()
{
    return PinTypeDesc{ PinKind::Exec, DataType::Int };
}

PinTypeDesc StringPin()
{
    return PinTypeDesc{ PinKind::Data, DataType::String };
}

PinTypeDesc FloatPin()
{
    return PinTypeDesc{ PinKind::Data, DataType::Float };
}

PinSignature Exec(const char* name)
{
    return PinSignature{ name, ExecPin(), {} };
}

// Always an unconnectable "configuration" literal (the sample to play),
// same reasoning as every other StringConfig-style pin elsewhere in this
// suite's node catalogs -- not something a graph computes and wires in.
PinSignature StringConfig(const char* name, std::string defaultValue = "")
{
    return PinSignature{ name, StringPin(), std::move(defaultValue) };
}

PinSignature Float(const char* name, float defaultValue)
{
    return PinSignature{ name, FloatPin(), defaultValue };
}

} // namespace

NodeLibraryRegistry BuildFoleyNodeCatalog()
{
    NodeLibraryRegistry libraries;

    {
        NodeTypeRegistry controlFlowTypes;
        ce::node_system::RegisterCoreControlFlowNodes(controlFlowTypes);

        NodeLibraryDescriptor library;
        library.id = "core-structural";
        library.displayName = "Core Structural Nodes";
        library.description = "Control-flow nodes native to NodeSystem itself (Branch/Sequence/Random Select/...) -- not FRust-reflected, shared across every domain that compiles an exec-chain Behavior graph.";
        library.target = GraphTarget::Behavior;
        for (const auto& [name, descriptor] : controlFlowTypes.Types())
            library.nodeTypes.push_back(descriptor);
        libraries.Register(std::move(library));
    }

    NodeLibraryDescriptor library;
    library.id = "foley.nodes";
    library.displayName = "Foley Nodes";
    library.target = GraphTarget::Behavior;

    // Zero inputs, one Exec output -- nothing feeds this, it IS the start of
    // an exec chain (same shape as CreationEngine's On Tick/On Begin Play).
    NodeTypeDescriptor onTrigger;
    onTrigger.typeName = NodeType::OnTrigger;
    onTrigger.domain = Domain::Event;
    onTrigger.outputs = { Exec(PinName::Then) };
    onTrigger.displayName = "On Trigger";
    onTrigger.category = "Foley";
    library.nodeTypes.push_back(std::move(onTrigger));

    // PlaySample/GainMix/Delay are all Domain::Event, ControlFlowKind::None,
    // host-extern callable action nodes -- exactly one Exec input, exactly
    // one Exec output named "then", reached only via the exec chain
    // (frust_codegen.cpp's lowerExecChain requires this shape and this
    // domain for a mid-chain callable; see its own comment on why).

    NodeTypeDescriptor playSample;
    playSample.typeName = NodeType::PlaySample;
    playSample.domain = Domain::Event;
    playSample.inputs = { Exec(PinName::Execute), StringConfig(PinName::SampleName) };
    playSample.outputs = { Exec(PinName::Then) };
    playSample.displayName = "Play Sample";
    playSample.category = "Foley";
    playSample.frustEntryPoint = kPlaySampleHostFunction;
    playSample.isHostExtern = true;
    library.nodeTypes.push_back(std::move(playSample));

    NodeTypeDescriptor gainMix;
    gainMix.typeName = NodeType::GainMix;
    gainMix.domain = Domain::Event;
    gainMix.inputs = { Exec(PinName::Execute), Float(PinName::Gain, 1.0f) };
    gainMix.outputs = { Exec(PinName::Then) };
    gainMix.displayName = "Gain Mix";
    gainMix.category = "Foley";
    gainMix.frustEntryPoint = kGainMixHostFunction;
    gainMix.isHostExtern = true;
    library.nodeTypes.push_back(std::move(gainMix));

    // Blocking: the compiled call synchronously waits `seconds` before the
    // exec chain continues (there is no async/scheduling primitive in
    // FRust's own execution model this could hook into instead). Only
    // sound when the host runs a triggered Foley cue off its own thread,
    // never the UI/message thread -- true of every host today, since no
    // app in this suite loads/runs a compiled Foley graph yet (see
    // FoleyPanel.h's own comment on current scope).
    NodeTypeDescriptor delay;
    delay.typeName = NodeType::Delay;
    delay.domain = Domain::Event;
    delay.inputs = { Exec(PinName::Execute), Float(PinName::Seconds, 0.0f) };
    delay.outputs = { Exec(PinName::Then) };
    delay.displayName = "Delay";
    delay.category = "Foley";
    delay.frustEntryPoint = kDelayHostFunction;
    delay.isHostExtern = true;
    library.nodeTypes.push_back(std::move(delay));

    libraries.Register(std::move(library));
    return libraries;
}

} // namespace cw::foleynodes
