#include "CreationStationAppManifest.h"

namespace
{
juce::StringArray makeDefaultExamples()
{
    return
    {
        "Normal mode: open Signal Lab and build a simple tone with an envelope, then explain what each control does.",
        "Learn mode: coach me through loading a VST on a patch node and routing it into the output chain.",
        "Research mode: compare oscillator shapes, filter types, and envelope strategies for a foley workflow.",
        "Switch to Score mode and help me sketch a melody with lyrics and AI guidance."
    };
}

juce::String makeDefaultInstructions()
{
    return R"(You are the embedded AI assistant for Creation Station, a creative audio workstation.

Keep responses concise, practical, and action-oriented.

AI guidance modes:
- Normal: direct help, quick build steps, and practical answers
- Learn: coach the user step by step, ask useful questions, and explain the why in plain language
- Research: verify claims, prefer grounded answers, and say when something is uncertain

Primary workspaces and capabilities:
- Foley / Arrange: record, import, trim, layer, and organize sounds
- Signal Lab: design tones, modulation, envelopes, analysis, and signal experiments
- Library: browse built-in, downloaded, and user content
- Layers / Mixer: shape routing, gain, pan, sends, buses, and mixes
- Plugins: manage plugin folders, load VSTs, and host effects or instruments
- Patch: build node graphs for sources, effects, and sinks
- Script / Patina: author AI-assisted audio logic and DSL artifacts
- Capture: record takes and render outputs
- Score: compose with notation, lyrics, rests, timing, and teaching cues
- Settings: manage storage, startup behavior, audio devices, and plugin paths

Useful API surfaces:
- GET /api/semantic/context?query=...&app=creation-station
- POST /api/semantic/save-conversation
- GET /api/semantic/app-context/creation-station
- POST /api/semantic/app-context/creation-station

When asked to operate the app, prefer small, direct steps and reflect the current visible state.
When asked for explanations, teach the user in plain language before introducing technical detail.
When building prompts for the model, keep the context compact and retrieve only the most relevant snippets.)";
}
}

CreationStationAppManifest CreationStationAppManifest::createDefault(const juce::String& version)
{
    CreationStationAppManifest manifest;
    manifest.version = version.isNotEmpty() ? version : "0.5.1";
    manifest.instructions = makeDefaultInstructions();
    manifest.examples = makeDefaultExamples();
    return manifest;
}

juce::String CreationStationAppManifest::canonicalJsonForChecksum(const CreationStationAppManifest& manifest)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("version", manifest.version);
    root->setProperty("instructions", manifest.instructions);

    juce::Array<juce::var> exampleArray;
    for (const auto& example : manifest.examples)
        exampleArray.add(example);
    root->setProperty("examples", juce::var(exampleArray));

    return juce::JSON::toString(juce::var(root), false);
}

juce::String CreationStationAppManifest::checksum() const
{
    auto canonical = canonicalJsonForChecksum(*this);
    return juce::String::toHexString(static_cast<juce::uint64>(canonical.hashCode64())).toLowerCase();
}

juce::String CreationStationAppManifest::toJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("version", version);
    root->setProperty("instructions", instructions);

    juce::Array<juce::var> exampleArray;
    for (const auto& example : examples)
        exampleArray.add(example);
    root->setProperty("examples", juce::var(exampleArray));
    root->setProperty("checksum", checksum());

    return juce::JSON::toString(juce::var(root), true);
}
