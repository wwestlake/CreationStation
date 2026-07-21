#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../AI/CreationStationContextEngine.h"
#include "../AI/CreationStationTaskPlanner.h"

class ChatTranscriptComponent;

class AiPanel final : public juce::Component,
                      private juce::TextEditor::Listener,
                      private juce::ComboBox::Listener
{
public:
    enum class GuidanceMode
    {
        normal,
        learn,
        research
    };

    enum class AccessLevel
    {
        askFirst,
        appOnly,
        fileChanges,
        fullAccess
    };

    AiPanel();
    ~AiPanel() override;

    void setGuidanceMode(GuidanceMode newMode);
    GuidanceMode getGuidanceMode() const noexcept { return guidanceMode; }

    void setAccessLevel(AccessLevel newLevel);
    AccessLevel getAccessLevel() const noexcept { return accessLevel; }

    void setAvailableModels(const juce::StringArray& modelIds, const juce::String& statusText);
    void setSelectedProvider(const juce::String& providerName);
    juce::String getSelectedProvider() const;
    void setSelectedModel(const juce::String& modelName);
    juce::String getSelectedModel() const;

    void setContextPacket(const CreationStationContextEngine::ContextPacket& packet);
    void setTaskPlan(const CreationStationTaskPlanner::TaskPlan& plan);

    void setAssistantResponse(const juce::String& responseText);
    void appendUserMessage(const juce::String& promptText);
    juce::String getPromptText() const;
    juce::String buildSubmissionPrompt() const;
    void setCollapsed(bool shouldCollapse);
    bool isCollapsed() const noexcept { return collapsed; }

    std::function<void(GuidanceMode mode)> onModeChanged;
    std::function<void(AccessLevel level)> onAccessChanged;
    std::function<void(const juce::String& providerName)> onProviderChanged;
    std::function<void(const juce::String& modelName)> onModelChanged;
    std::function<void(const juce::String& prompt)> onPromptSubmitted;
    std::function<void(const CreationStationTaskPlanner::TaskStep& step)> onExecuteNextStep;
    std::function<void(bool shouldCollapse)> onCollapsedChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static juce::String modeLabel(GuidanceMode mode);
    static juce::String modeDescription(GuidanceMode mode);
    static juce::String modePromptPrefix(GuidanceMode mode);
    static juce::String accessLabel(AccessLevel level);
    static juce::String accessDescription(AccessLevel level);
    static juce::String accessPromptPrefix(AccessLevel level);

    void refreshModeUi();
    void refreshAccessUi();
    void refreshPromptHeight();
    void refreshChatLayout();
    void scrollChatToBottom();
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    juce::Label headerLabel;
    juce::Label subtitleLabel;
    juce::Label modeLabelTitle;
    juce::TextButton normalModeButton { "Normal" };
    juce::TextButton learnModeButton { "Learn" };
    juce::TextButton researchModeButton { "Research" };
    juce::Label providerLabel;
    juce::ComboBox providerComboBox;
    juce::Label modelLabel;
    juce::ComboBox modelComboBox;
    juce::Label accessLabelTitle;
    juce::ComboBox accessComboBox;
    juce::Label promptLabel;
    juce::Viewport transcriptViewport;
    std::unique_ptr<ChatTranscriptComponent> transcriptContent;
    juce::TextEditor promptEditor;
    juce::TextButton sendButton { "Send" };
    juce::TextButton collapseButton { "Hide" };
    juce::Label footerHintLabel;
    std::optional<CreationStationTaskPlanner::TaskPlan> currentPlan;
    GuidanceMode guidanceMode = GuidanceMode::normal;
    AccessLevel accessLevel = AccessLevel::askFirst;
    bool collapsed = false;
    int promptEditorHeight = 0;
    bool updatingComboBoxes = false;
    juce::StringArray availableModels;
    juce::String latestContextSummary;
    juce::String latestPlanSummary;
    int pendingAssistantBubbleIndex = -1;
};
