#pragma once

#include <JuceHeader.h>
#include <optional>
#include "PromptInputParts.h"
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

    // Who the assistant is working as. The Engineer changes the project by writing scripts; the Producer talks about the
    // music and listens through measuring tools. The choice sits in the title row.
    enum class Role { engineer, producer };
    void setRole(Role newRole);
    Role getRole() const noexcept { return role; }
    std::function<void(Role role)> onRoleChanged;

    // Saved conversations. "Chats" asks the host to show the list (open, archive, export, delete) and "New" to start a
    // fresh conversation; what they do is the host's, which owns the storage.
    std::function<void()> onChatsRequested;
    std::function<void()> onNewConversationRequested;
    // Empties the transcript (a new conversation), or replaces it with a saved one: each turn is (true for the user, text).
    void clearTranscript();
    void showConversation(const std::vector<std::pair<bool, juce::String>>& turns);

    void setAccessLevel(AccessLevel newLevel);
    AccessLevel getAccessLevel() const noexcept { return accessLevel; }

    // A user-named suite AI account (Settings -> Suite AI Accounts). The Virtual Engineer picks
    // among these by name - it never configures a provider itself.
    struct AccountEntry
    {
        juce::String accountId;
        juce::String displayName;
    };

    void setAvailableAccounts(const juce::Array<AccountEntry>& accounts, const juce::String& selectedAccountId);
    juce::String getSelectedAccountId() const;
    void setAvailableModels(const juce::StringArray& modelIds, const juce::String& statusText);
    void setSelectedModel(const juce::String& modelName);
    juce::String getSelectedModel() const;

    void setContextPacket(const CreationStationContextEngine::ContextPacket& packet);
    void setTaskPlan(const CreationStationTaskPlanner::TaskPlan& plan);

    void setAssistantResponse(const juce::String& responseText);
    void appendUserMessage(const juce::String& promptText);
    juce::String getPromptText() const;
    // What the user typed in the message they last sent, without the mode and access text added to what is sent to the model.
    juce::String getLastQuestion() const { return lastQuestion; }
    juce::String buildSubmissionPrompt() const;
    void setCollapsed(bool shouldCollapse);

    // Whether Enter sends the message (Shift+Enter is then a new line). Off by default: Enter is a new
    // line and Ctrl+Enter sends. The toggle sits under the message box.
    void setEnterSendsMessage(bool shouldSend);
    bool getEnterSendsMessage() const noexcept { return promptEditor.enterSends; }
    std::function<void(bool enterSends)> onEnterSendsChanged;

    // What the round button at the right of the message box shows, and its tooltip. The host changes it as
    // the assistant's state changes (send while idle, stop while a request runs).
    void setSendButtonIcon(station_ui::SendArrowButton::Icon icon, const juce::String& tooltip);

    // While the assistant is working the arrow becomes Stop, and pressing it (not Enter) calls onStopRequested.
    void setRunning(bool isRunning);
    bool isRunning() const noexcept { return running; }
    std::function<void()> onStopRequested;
    bool isCollapsed() const noexcept { return collapsed; }

    std::function<void(GuidanceMode mode)> onModeChanged;
    std::function<void(AccessLevel level)> onAccessChanged;
    std::function<void(const juce::String& accountId)> onAccountChanged;
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
    int measurePromptHeight(int width) const;
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
    juce::Label accountLabel;
    juce::ComboBox accountComboBox;
    juce::Label modelLabel;
    juce::ComboBox modelComboBox;
    juce::Label accessLabelTitle;
    juce::ComboBox accessComboBox;
    juce::ComboBox roleComboBox;
    juce::TextButton chatsButton { "Chats" };
    juce::TextButton newChatButton { "New" };
    Role role = Role::engineer;
    juce::Label promptLabel;
    juce::Viewport transcriptViewport;
    std::unique_ptr<ChatTranscriptComponent> transcriptContent;
    station_ui::PromptEditor promptEditor;
    juce::String lastQuestion;
    station_ui::SendArrowButton sendButton;
    juce::ToggleButton enterSendsToggle { "Enter sends" };
    juce::TextButton collapseButton { "Hide" };
    juce::Label footerHintLabel;
    std::optional<CreationStationTaskPlanner::TaskPlan> currentPlan;
    GuidanceMode guidanceMode = GuidanceMode::normal;
    AccessLevel accessLevel = AccessLevel::askFirst;
    bool collapsed = false;
    int promptEditorHeight = 0;
    bool running = false;
    bool updatingComboBoxes = false;
    juce::Array<AccountEntry> availableAccounts;
    juce::StringArray availableModels;
    juce::String latestContextSummary;
    juce::String latestPlanSummary;
    int pendingAssistantBubbleIndex = -1;
};
