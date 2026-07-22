#pragma once

#include <JuceHeader.h>
#include "CreationStationContextEngine.h"

class CreationStationTaskPlanner
{
public:
    enum class ActionTarget
    {
        workspace,
        signalLab,
        patchGraph,
        transport,
        context
    };

    enum class StepType
    {
        observe,
        analyze,
        decide,
        act,
        verify,
        recover
    };

    enum class StepStatus
    {
        pending,
        ready,
        blocked,
        complete
    };

    struct TaskStep
    {
        struct Action
        {
            ActionTarget target = ActionTarget::context;
            juce::String command;
            juce::String label;
            juce::String stringValue;
            float floatValue = 0.0f;
            bool boolValue = false;
        };

        juce::String id;
        juce::String title;
        juce::String detail;
        juce::String expectedResult;
        juce::String fallback;
        StepType type = StepType::observe;
        StepStatus status = StepStatus::pending;
        juce::StringArray tags;
        juce::Array<Action> actions;
    };

    struct TaskPlan
    {
        struct DataValue
        {
            juce::String key;
            juce::String description;
            juce::String stringValue;
            double numericValue = 0.0;
            bool hasNumericValue = false;
        };

        juce::String goal;
        juce::String workflow;
        juce::String summary;
        juce::Array<TaskStep> steps;
        juce::StringArray suggestedTools;
        juce::String verificationNote;
        juce::Array<DataValue> dataSchema;
    };

    TaskPlan buildPlan(const juce::String& prompt,
                       const CreationStationContextEngine::ContextPacket& packet) const;

    static juce::String describePlan(const TaskPlan& plan);

private:
    enum class WorkflowKind
    {
        generic,
        noiseReduction,
        instrumentDesign,
        contentAssembly,
        captureSession
    };

    static juce::StringArray tokenize(const juce::String& text);
    static WorkflowKind classifyPrompt(const juce::StringArray& tokens,
                                       const CreationStationContextEngine::ContextPacket& packet);
    static juce::String stepTypeName(StepType type);
    static juce::String stepStatusName(StepStatus status);
    static juce::String actionTargetName(ActionTarget target);
    static TaskStep::Action makeAction(ActionTarget target,
                                       juce::String command,
                                       juce::String label,
                                       juce::String stringValue = {},
                                       float floatValue = 0.0f,
                                       bool boolValue = false);
    static TaskStep makeStep(juce::String id,
                             StepType type,
                             juce::String title,
                             juce::String detail,
                             juce::String expectedResult,
                             juce::String fallback,
                             juce::StringArray tags = {},
                             juce::Array<TaskStep::Action> actions = {});
};
