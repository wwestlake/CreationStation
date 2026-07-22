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
};

struct PatchAutomationLane
{
    juce::String id;
    juce::String name;
    juce::String targetParameter;
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
};

struct PatchConnection
{
    juce::String from;
    juce::String to;
};

struct PatchNode
{
    juce::String id;
    juce::String kind;
    juce::NamedValueSet properties;
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
