#pragma once

#include <JuceHeader.h>
#include <array>

namespace cw
{
struct PatchParameter
{
    juce::String id;
    juce::String name;
    juce::String kind;
    double defaultValue = 0.0;
    double minValue = 0.0;
    double maxValue = 1.0;
    juce::String unit;
};

struct PatchAutomationPoint
{
    double time = 0.0;
    double value = 0.0;
    juce::String curve { "linear" };
};

struct PatchAutomationLane
{
    juce::String id;
    juce::String name;
    juce::String targetParameter;
    juce::String interpolation { "linear" };
    double startTime = 0.0;
    double endTime = 1.0;
    double rangeMin = 0.0;
    double rangeMax = 1.0;
    juce::Array<PatchAutomationPoint> points;
};

struct PatchSource
{
    juce::String id;
    juce::String kind;
    juce::String waveform;
    juce::String noiseType;
    double level = 0.0;
    juce::String frequencyParameter;
    int canvasX = 0;
    int canvasY = 0;
};

struct PatchConnection
{
    juce::String from;
    juce::String to;
    // Empty for older/simple connections -- only meaningful for multi-port
    // nodes (e.g. which Mixer channel this feeds).
    juce::String fromPort;
    juce::String toPort;
    // Signal-path gain for this specific connection (Mixer channel weight).
    // 1.0 for a plain unweighted pass-through connection.
    double weight = 1.0;
};

struct PatchNode
{
    juce::String id;
    juce::String kind;
    juce::NamedValueSet properties;
    int canvasX = 0;
    int canvasY = 0;
};

struct PatchOutput
{
    juce::String channelMode { "stereo" };
    double gain = 0.9;
    double pan = 0.0;
};

struct PatchDocument
{
    juce::String schemaVersion { "1.0" };
    juce::String patchId;
    juce::String name;
    juce::String type;
    juce::String author { "LagDaemon" };
    juce::String description;
    juce::String createdAt;
    juce::String updatedAt;
    juce::String runtime { "creation-station" };
    juce::String minimumVersion { "0.2.0" };
    juce::Array<PatchParameter> parameters;
    juce::Array<PatchAutomationLane> automationLanes;
    juce::Array<PatchSource> sources;
    juce::Array<PatchNode> nodes;
    juce::Array<PatchConnection> connections;
    PatchOutput output;
};

juce::String makePatchId(const juce::String& baseName);
juce::String serialisePatchDocumentJson(const PatchDocument& document);
bool parsePatchDocumentJson(const juce::String& jsonText, PatchDocument& document, juce::String& errorMessage);
}
