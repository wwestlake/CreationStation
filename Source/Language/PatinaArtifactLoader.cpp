#include "PatinaArtifactLoader.h"

namespace cw::patina
{
namespace
{
TypeSpec parseTypeSpec(const juce::var& value)
{
    TypeSystem typeSystem;

    if (auto* object = value.getDynamicObject())
    {
        auto raw = object->getProperty("raw").toString();
        if (raw.isNotEmpty())
            return typeSystem.parseType(raw);

        auto family = object->getProperty("family").toString();
        if (family.isNotEmpty())
            return typeSystem.parseType(family);
    }

    return {};
}

ir::ValueRef parseValueRef(const juce::var& value)
{
    ir::ValueRef valueRef;
    if (auto* object = value.getDynamicObject())
    {
        auto kind = object->getProperty("kind").toString();
        if (kind == "graphParameter")
            valueRef.kind = ir::ValueRef::Kind::graphParameter;
        else if (kind == "localConstant")
            valueRef.kind = ir::ValueRef::Kind::localConstant;
        else if (kind == "symbolic")
            valueRef.kind = ir::ValueRef::Kind::symbolic;
        else
            valueRef.kind = ir::ValueRef::Kind::literal;

        if (valueRef.kind == ir::ValueRef::Kind::literal)
            valueRef.literalValue = object->getProperty("value");
        else
            valueRef.referenceName = object->getProperty("reference").toString();
    }

    return valueRef;
}

juce::Array<juce::var>* getArrayProperty(juce::DynamicObject* object, const juce::Identifier& property)
{
    if (object == nullptr)
        return nullptr;

    return object->getProperty(property).getArray();
}
} // namespace

bool ArtifactLoader::loadJson(const juce::String& jsonText, ir::Document& document, juce::String& errorMessage) const
{
    document = {};

    auto parsed = juce::JSON::parse(jsonText);
    if (parsed.isVoid())
    {
        errorMessage = "That Patina artifact is not valid JSON.";
        return false;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        errorMessage = "That Patina artifact does not contain a JSON object root.";
        return false;
    }

    document.schemaVersion = root->getProperty("schemaVersion").toString();
    document.runtimeVersion = root->getProperty("runtimeVersion").toString();
    document.abiVersion = root->getProperty("abiVersion").toString();
    document.packageName = root->getProperty("package").toString();
    document.packageVersion = root->getProperty("version").toString();

    if (auto* graphs = getArrayProperty(root, "graphs"))
    {
        for (const auto& graphValue : *graphs)
        {
            auto* graphObject = graphValue.getDynamicObject();
            if (graphObject == nullptr)
                continue;

            ir::Graph graph;
            graph.name = graphObject->getProperty("name").toString();

            if (auto* parameters = getArrayProperty(graphObject, "parameters"))
            {
                for (const auto& parameterValue : *parameters)
                {
                    auto* parameterObject = parameterValue.getDynamicObject();
                    if (parameterObject == nullptr)
                        continue;

                    ir::Parameter parameter;
                    parameter.name = parameterObject->getProperty("name").toString();
                    parameter.typeText = parameterObject->getProperty("type").toString();
                    parameter.type = parseTypeSpec(parameterObject->getProperty("typeSpec"));
                    parameter.hasDefaultValue = (bool) parameterObject->getProperty("hasDefaultValue");
                    if (parameter.hasDefaultValue)
                        parameter.defaultValue = parseValueRef(parameterObject->getProperty("defaultValue"));
                    graph.parameters.add(parameter);
                }
            }

            if (auto* nodes = getArrayProperty(graphObject, "nodes"))
            {
                for (const auto& nodeValue : *nodes)
                {
                    auto* nodeObject = nodeValue.getDynamicObject();
                    if (nodeObject == nullptr)
                        continue;

                    ir::Node node;
                    node.id = nodeObject->getProperty("id").toString();
                    node.kind = nodeObject->getProperty("kind").toString();
                    auto domainText = nodeObject->getProperty("domain").toString();
                    node.domain = domainText == "control" ? ir::Domain::control
                                : domainText == "event" ? ir::Domain::event
                                : domainText == "worker" ? ir::Domain::worker
                                                         : ir::Domain::audio;
                    node.line = (int) nodeObject->getProperty("line");

                    if (auto* arguments = getArrayProperty(nodeObject, "arguments"))
                    {
                        for (const auto& argumentValue : *arguments)
                        {
                            auto* argumentObject = argumentValue.getDynamicObject();
                            if (argumentObject == nullptr)
                                continue;

                            ir::NodeArgument argument;
                            argument.name = argumentObject->getProperty("name").toString();
                            argument.value = parseValueRef(argumentObject->getProperty("value"));
                            argument.valueType = parseTypeSpec(argumentObject->getProperty("valueType"));
                            node.arguments.add(argument);
                        }
                    }

                    if (auto* inputs = getArrayProperty(nodeObject, "inputs"))
                    {
                        for (const auto& inputValue : *inputs)
                        {
                            auto* inputObject = inputValue.getDynamicObject();
                            if (inputObject == nullptr)
                                continue;

                            node.inputs.add({ inputObject->getProperty("name").toString(),
                                              parseTypeSpec(inputObject->getProperty("type")) });
                        }
                    }

                    if (auto* outputs = getArrayProperty(nodeObject, "outputs"))
                    {
                        for (const auto& outputValue : *outputs)
                        {
                            auto* outputObject = outputValue.getDynamicObject();
                            if (outputObject == nullptr)
                                continue;

                            node.outputs.add({ outputObject->getProperty("name").toString(),
                                               parseTypeSpec(outputObject->getProperty("type")) });
                        }
                    }

                    graph.nodes.add(node);
                }
            }

            if (auto* edges = getArrayProperty(graphObject, "edges"))
            {
                for (const auto& edgeValue : *edges)
                {
                    auto* edgeObject = edgeValue.getDynamicObject();
                    if (edgeObject == nullptr)
                        continue;

                    ir::Edge edge;
                    edge.sourceNode = edgeObject->getProperty("sourceNode").toString();
                    edge.sourcePort = edgeObject->getProperty("sourcePort").toString();
                    edge.destinationNode = edgeObject->getProperty("destinationNode").toString();
                    edge.destinationPort = edgeObject->getProperty("destinationPort").toString();
                    edge.signalType = parseTypeSpec(edgeObject->getProperty("signalType"));
                    edge.line = (int) edgeObject->getProperty("line");
                    graph.edges.add(edge);
                }
            }

            document.graphs.add(graph);
        }
    }

    if (auto* exports = getArrayProperty(root, "exports"))
    {
        for (const auto& exportValue : *exports)
        {
            auto* exportObject = exportValue.getDynamicObject();
            if (exportObject == nullptr)
                continue;

            document.exports.add({ exportObject->getProperty("kind").toString(),
                                   exportObject->getProperty("graph").toString() });
        }
    }

    return true;
}
} // namespace cw::patina
