#include "PatchModel.h"

namespace
{
juce::String isoNow()
{
    return juce::Time::getCurrentTime().toISO8601(true);
}

juce::var pointToVar(const cw::PatchAutomationPoint& point)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("time", point.time);
    object->setProperty("value", point.value);
    return juce::var(object);
}

juce::var parameterToVar(const cw::PatchParameter& parameter)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", parameter.id);
    object->setProperty("name", parameter.name);
    object->setProperty("kind", parameter.kind);
    object->setProperty("default", parameter.defaultValue);
    object->setProperty("min", parameter.minValue);
    object->setProperty("max", parameter.maxValue);
    object->setProperty("unit", parameter.unit);
    return juce::var(object);
}

juce::var laneToVar(const cw::PatchAutomationLane& lane)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", lane.id);
    object->setProperty("name", lane.name);
    object->setProperty("targetParameter", lane.targetParameter);

    auto* range = new juce::DynamicObject();
    range->setProperty("min", lane.rangeMin);
    range->setProperty("max", lane.rangeMax);
    object->setProperty("range", juce::var(range));

    juce::Array<juce::var> points;
    for (const auto& point : lane.points)
        points.add(pointToVar(point));

    object->setProperty("points", juce::var(points));
    return juce::var(object);
}

juce::var sourceToVar(const cw::PatchSource& source)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", source.id);
    object->setProperty("kind", source.kind);

    if (source.waveform.isNotEmpty())
        object->setProperty("waveform", source.waveform);
    if (source.noiseType.isNotEmpty())
        object->setProperty("noiseType", source.noiseType);
    if (source.frequencyParameter.isNotEmpty())
        object->setProperty("frequencyParameter", source.frequencyParameter);

    object->setProperty("level", source.level);
    return juce::var(object);
}

juce::var nodeToVar(const cw::PatchNode& node)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", node.id);
    object->setProperty("kind", node.kind);

    for (int index = 0; index < node.properties.size(); ++index)
        object->setProperty(node.properties.getName(index), node.properties.getValueAt(index));

    return juce::var(object);
}

juce::var connectionToVar(const cw::PatchConnection& connection)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("from", connection.from);
    object->setProperty("to", connection.to);
    return juce::var(object);
}
}

namespace cw
{
juce::String makePatchId(const juce::String& baseName)
{
    auto slug = baseName.trim().toLowerCase();
    slug = slug.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");

    if (slug.isEmpty())
        slug = "creation-station-patch";

    return slug + "-" + juce::Uuid().toString().substring(0, 8);
}

juce::String serialisePatchDocumentJson(const PatchDocument& document)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schemaVersion", document.schemaVersion);
    root->setProperty("patchId", document.patchId.isNotEmpty() ? document.patchId : makePatchId(document.name));
    root->setProperty("name", document.name);
    root->setProperty("type", document.type);
    root->setProperty("author", document.author);
    root->setProperty("description", document.description);
    root->setProperty("createdAt", document.createdAt.isNotEmpty() ? document.createdAt : isoNow());
    root->setProperty("updatedAt", document.updatedAt.isNotEmpty() ? document.updatedAt : isoNow());

    auto* engine = new juce::DynamicObject();
    engine->setProperty("runtime", document.runtime);
    engine->setProperty("minimumVersion", document.minimumVersion);
    root->setProperty("engine", juce::var(engine));

    juce::Array<juce::var> parameters;
    for (const auto& parameter : document.parameters)
        parameters.add(parameterToVar(parameter));
    root->setProperty("parameters", juce::var(parameters));

    juce::Array<juce::var> lanes;
    for (const auto& lane : document.automationLanes)
        lanes.add(laneToVar(lane));
    root->setProperty("automationLanes", juce::var(lanes));

    juce::Array<juce::var> sources;
    for (const auto& source : document.sources)
        sources.add(sourceToVar(source));
    root->setProperty("sources", juce::var(sources));

    auto* graph = new juce::DynamicObject();
    juce::Array<juce::var> nodes;
    for (const auto& node : document.nodes)
        nodes.add(nodeToVar(node));
    graph->setProperty("nodes", juce::var(nodes));

    juce::Array<juce::var> connections;
    for (const auto& connection : document.connections)
        connections.add(connectionToVar(connection));
    graph->setProperty("connections", juce::var(connections));
    root->setProperty("graph", juce::var(graph));

    root->setProperty("assets", juce::var(juce::Array<juce::var>()));

    auto* output = new juce::DynamicObject();
    output->setProperty("channelMode", document.output.channelMode);
    output->setProperty("gain", document.output.gain);
    output->setProperty("pan", document.output.pan);
    root->setProperty("output", juce::var(output));

    return juce::JSON::toString(juce::var(root), true);
}

bool parsePatchDocumentJson(const juce::String& jsonText, PatchDocument& document, juce::String& errorMessage)
{
    auto parsed = juce::JSON::parse(jsonText);
    if (parsed.isVoid())
    {
        errorMessage = "That patch file is not valid JSON.";
        return false;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        errorMessage = "That patch file does not contain a valid patch object.";
        return false;
    }

    document = {};
    document.schemaVersion = root->getProperty("schemaVersion").toString();
    document.patchId = root->getProperty("patchId").toString();
    document.name = root->getProperty("name").toString();
    document.type = root->getProperty("type").toString();
    document.author = root->getProperty("author").toString();
    document.description = root->getProperty("description").toString();
    document.createdAt = root->getProperty("createdAt").toString();
    document.updatedAt = root->getProperty("updatedAt").toString();

    if (auto* engine = root->getProperty("engine").getDynamicObject())
    {
        document.runtime = engine->getProperty("runtime").toString();
        document.minimumVersion = engine->getProperty("minimumVersion").toString();
    }

    auto parseArray = [](const juce::var& value) -> const juce::Array<juce::var>* { return value.getArray(); };

    if (auto* parameters = parseArray(root->getProperty("parameters")))
    {
        for (const auto& value : *parameters)
        {
            if (auto* object = value.getDynamicObject())
            {
                PatchParameter parameter;
                parameter.id = object->getProperty("id").toString();
                parameter.name = object->getProperty("name").toString();
                parameter.kind = object->getProperty("kind").toString();
                parameter.defaultValue = (double) object->getProperty("default");
                parameter.minValue = (double) object->getProperty("min");
                parameter.maxValue = (double) object->getProperty("max");
                parameter.unit = object->getProperty("unit").toString();
                document.parameters.add(parameter);
            }
        }
    }

    if (auto* lanes = parseArray(root->getProperty("automationLanes")))
    {
        for (const auto& value : *lanes)
        {
            if (auto* object = value.getDynamicObject())
            {
                PatchAutomationLane lane;
                lane.id = object->getProperty("id").toString();
                lane.name = object->getProperty("name").toString();
                lane.targetParameter = object->getProperty("targetParameter").toString();

                if (auto* range = object->getProperty("range").getDynamicObject())
                {
                    lane.rangeMin = (double) range->getProperty("min");
                    lane.rangeMax = (double) range->getProperty("max");
                }

                if (auto* points = parseArray(object->getProperty("points")))
                {
                    for (const auto& pointValue : *points)
                    {
                        if (auto* pointObject = pointValue.getDynamicObject())
                        {
                            PatchAutomationPoint point;
                            point.time = (double) pointObject->getProperty("time");
                            point.value = (double) pointObject->getProperty("value");
                            lane.points.add(point);
                        }
                    }
                }

                document.automationLanes.add(lane);
            }
        }
    }

    if (auto* sources = parseArray(root->getProperty("sources")))
    {
        for (const auto& value : *sources)
        {
            if (auto* object = value.getDynamicObject())
            {
                PatchSource source;
                source.id = object->getProperty("id").toString();
                source.kind = object->getProperty("kind").toString();
                source.waveform = object->getProperty("waveform").toString();
                source.noiseType = object->getProperty("noiseType").toString();
                source.level = (double) object->getProperty("level");
                source.frequencyParameter = object->getProperty("frequencyParameter").toString();
                document.sources.add(source);
            }
        }
    }

    if (auto* graph = root->getProperty("graph").getDynamicObject())
    {
        if (auto* nodes = parseArray(graph->getProperty("nodes")))
        {
            for (const auto& value : *nodes)
            {
                if (auto* object = value.getDynamicObject())
                {
                    PatchNode node;
                    node.id = object->getProperty("id").toString();
                    node.kind = object->getProperty("kind").toString();

                    for (const auto& property : object->getProperties())
                    {
                        auto propertyName = property.name.toString();
                        if (propertyName != "id" && propertyName != "kind")
                            node.properties.set(property.name, property.value);
                    }

                    document.nodes.add(node);
                }
            }
        }

        if (auto* connections = parseArray(graph->getProperty("connections")))
        {
            for (const auto& value : *connections)
            {
                if (auto* object = value.getDynamicObject())
                {
                    PatchConnection connection;
                    connection.from = object->getProperty("from").toString();
                    connection.to = object->getProperty("to").toString();
                    document.connections.add(connection);
                }
            }
        }
    }

    if (auto* output = root->getProperty("output").getDynamicObject())
    {
        document.output.channelMode = output->getProperty("channelMode").toString();
        document.output.gain = (double) output->getProperty("gain");
        document.output.pan = (double) output->getProperty("pan");
    }

    if (document.name.isEmpty() || document.type.isEmpty())
    {
        errorMessage = "That patch file is missing required top-level fields.";
        return false;
    }

    return true;
}
}
