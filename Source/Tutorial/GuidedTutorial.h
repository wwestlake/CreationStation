#pragma once

#include <JuceHeader.h>

namespace cw::tutorial
{
enum class ActionType
{
    switchWorkspace,
    applySignalTemplate,
    applyGraphMacro
};

struct Action
{
    ActionType type = ActionType::switchWorkspace;
    juce::String value;
};

struct Scene
{
    juce::String id;
    juce::String title;
    juce::String body;
    juce::String targetId;
    bool advanceOnTargetClick = true;
    bool drawConnector = true;
    juce::String nextButtonText;
    juce::Array<Action> actions;
};

struct Script
{
    juce::String id;
    juce::String name;
    juce::String description;
    juce::Array<Scene> scenes;
};

juce::String getBuiltInGettingStartedTutorialSource();
juce::String getBuiltInVstNodeDemoTutorialSource();
Script makeGettingStartedTutorial();
}
