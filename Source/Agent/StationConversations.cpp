#include "../MainComponent.h"

#include "VfsAssistantFiles.h"

// The assistant's conversations: recorded as they happen, and managed from the Chats window. Each conversation is one file in
// the project's own storage under Assistants/Station/conversations, a chain of message blocks where every block commits to the
// one before it (shared/AssistantStore), so an edit made outside the assistant is detected. Message thread only.

namespace
{
using creation::assistant::AssistantArea;
using creation::assistant::Conversation;
using creation::assistant::ConversationLedger;
using creation::assistant::ConversationStore;
using creation::assistant::ConversationSummary;

const AssistantArea stationArea { "Station" };

// A file name from a conversation title: letters, digits, spaces and a few marks, and never empty.
juce::String fileNameFor(const std::string& title)
{
    auto name = juce::File::createLegalFileName(juce::String::fromUTF8(title.c_str())).trim();
    if (name.length() > 60)
        name = name.substring(0, 60).trimEnd();
    return name.isEmpty() ? juce::String("conversation") : name;
}
}

void MainComponent::recordConversationTurn(const char* role, const juce::String& text)
{
    if (text.trim().isEmpty())
        return;

    if (! projectSession.isValid())
    {
        transportBar.setStatusText("No project is open, so this conversation is not being saved.");
        return;
    }

    // A conversation belongs to one project: after switching projects the next message starts a new one.
    if (conversationOpen && conversationProjectId != projectSession.getProjectId())
        conversationOpen = false;
    if (! conversationOpen)
    {
        currentConversation = ConversationLedger::create();
        conversationProjectId = projectSession.getProjectId();
        conversationOpen = true;
    }

    if (! ConversationLedger::append(currentConversation, role, text.toStdString()))
        return;

    VfsAssistantFiles files(projectSession);
    ConversationStore store(files, stationArea);
    std::string error;
    if (! store.save(currentConversation, error))
        transportBar.setStatusText("This conversation could not be saved: " + juce::String(error));

    if (conversationManager != nullptr)
        conversationManager->refresh();
}

void MainComponent::startNewConversation()
{
    if (aiCompletionInFlight || (assistant != nullptr && assistant->isRunning()))
    {
        transportBar.setStatusText("Wait for the assistant to finish, or stop it, before starting a new conversation.");
        return;
    }

    conversationOpen = false;
    aiPanel.clearTranscript();
    if (assistant != nullptr)
        assistant->clearConversation();
    transportBar.setStatusText("New conversation. The last one stays in Chats.");
}

void MainComponent::openSavedConversation(const std::string& id)
{
    if (aiCompletionInFlight || (assistant != nullptr && assistant->isRunning()))
    {
        transportBar.setStatusText("Wait for the assistant to finish, or stop it, before opening another conversation.");
        return;
    }

    VfsAssistantFiles files(projectSession);
    ConversationStore store(files, stationArea);
    Conversation loaded;
    std::string error;
    if (! store.load(id, loaded, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversation not opened",
                                               "This conversation could not be opened:\n" + juce::String(error));
        if (conversationManager != nullptr)
            conversationManager->refresh();
        return;
    }

    std::vector<std::pair<bool, juce::String>> shown;
    std::vector<std::pair<std::string, std::string>> remembered;
    for (const auto& block : loaded.blocks)
    {
        shown.emplace_back(block.role == "user", juce::String::fromUTF8(block.content.c_str()));
        remembered.emplace_back(block.role, block.content);
    }

    currentConversation = std::move(loaded);
    conversationOpen = true;
    conversationProjectId = projectSession.getProjectId();
    aiPanel.showConversation(shown);
    ensureAssistant().restoreConversation(remembered);
    transportBar.setStatusText("Opened: " + juce::String::fromUTF8(currentConversation.title.c_str()));

    if (conversationManager != nullptr)
        if (auto* window = conversationManager->findParentComponentOfClass<juce::DialogWindow>())
            window->closeButtonPressed();
}

void MainComponent::archiveOrRestoreSavedConversation(const ConversationSummary& summary)
{
    VfsAssistantFiles files(projectSession);
    ConversationStore store(files, stationArea);
    std::string error;
    const bool ok = summary.archived ? store.unarchive(summary.id, error) : store.archive(summary.id, error);
    if (! ok)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, summary.archived ? "Not restored" : "Not archived",
                                               juce::String(error));
        return;
    }
    transportBar.setStatusText(summary.archived ? "Conversation restored." : "Conversation archived.");

    // Putting away the conversation that is on screen starts a fresh one, so nothing is recorded into an archived file.
    if (! summary.archived && conversationOpen && currentConversation.id == summary.id
        && ! (aiCompletionInFlight || (assistant != nullptr && assistant->isRunning())))
        startNewConversation();
}

void MainComponent::exportSavedConversation(const ConversationSummary& summary)
{
    VfsAssistantFiles files(projectSession);
    ConversationStore store(files, stationArea);
    auto conversation = std::make_shared<Conversation>();
    std::string error;
    if (! (summary.archived ? store.loadArchived(summary.id, *conversation, error) : store.load(summary.id, *conversation, error)))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Not exported", juce::String(error));
        return;
    }

    exportChooser = std::make_shared<juce::FileChooser>(
        "Export conversation",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(fileNameFor(conversation->title) + ".md"),
        "*.md;*.json");
    auto chooser = exportChooser;
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [safeThis = juce::Component::SafePointer<MainComponent>(this), chooser, conversation](const juce::FileChooser& picked)
                         {
                             const auto target = picked.getResult();
                             if (target == juce::File() || safeThis == nullptr)
                                 return;

                             // .json keeps the verifiable record exactly as stored; anything else is a readable document.
                             const auto text = target.hasFileExtension("json") ? ConversationLedger::toJson(*conversation)
                                                                                 : ConversationLedger::toMarkdown(*conversation);
                             const bool written = target.replaceWithText(juce::String::fromUTF8(text.c_str()));
                             safeThis->transportBar.setStatusText(written ? "Conversation exported to " + target.getFileName() + "."
                                                                          : juce::String("The conversation could not be exported."));
                         });
}

void MainComponent::deleteSavedConversation(const ConversationSummary& summary)
{
    auto options = juce::MessageBoxOptions()
                       .withIconType(juce::MessageBoxIconType::WarningIcon)
                       .withTitle("Delete conversation")
                       .withMessage("Delete \"" + juce::String::fromUTF8(summary.title.c_str()) + "\" for good? This cannot be undone.")
                       .withButton("Delete")
                       .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [safeThis = juce::Component::SafePointer<MainComponent>(this), summary](int result)
    {
        if (safeThis == nullptr || result != 1)
            return;

        VfsAssistantFiles files(safeThis->projectSession);
        ConversationStore store(files, stationArea);
        std::string error;
        if (! store.remove(summary.id, error))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Not deleted", juce::String(error));
            return;
        }
        safeThis->transportBar.setStatusText("Conversation deleted.");

        if (safeThis->conversationOpen && safeThis->currentConversation.id == summary.id)
            safeThis->conversationOpen = false;   // what was on screen is gone: the next message starts a new one
        if (safeThis->conversationManager != nullptr)
            safeThis->conversationManager->refresh();
    });
}

void MainComponent::showConversationManager()
{
    if (! projectSession.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "No project open",
                                               "Conversations are saved inside the project, so open or create a project first.");
        return;
    }

    if (conversationManager != nullptr)
    {
        if (auto* window = conversationManager->findParentComponentOfClass<juce::DialogWindow>())
            window->toFront(true);
        conversationManager->refresh();
        return;
    }

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    ConversationManager::Actions actions;
    actions.list = [safeThis](bool archived)
    {
        if (safeThis == nullptr)
            return std::vector<ConversationSummary>();
        VfsAssistantFiles files(safeThis->projectSession);
        ConversationStore store(files, stationArea);
        return archived ? store.listArchived() : store.list();
    };
    actions.open = [safeThis](const ConversationSummary& s) { if (safeThis != nullptr) safeThis->openSavedConversation(s.id); };
    actions.archiveOrRestore = [safeThis](const ConversationSummary& s) { if (safeThis != nullptr) safeThis->archiveOrRestoreSavedConversation(s); };
    actions.exportIt = [safeThis](const ConversationSummary& s) { if (safeThis != nullptr) safeThis->exportSavedConversation(s); };
    actions.remove = [safeThis](const ConversationSummary& s) { if (safeThis != nullptr) safeThis->deleteSavedConversation(s); };

    auto* content = new ConversationManager(std::move(actions));
    conversationManager = content;

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content);
    options.dialogTitle = "Assistant conversations";
    options.dialogBackgroundColour = juce::Colour(0xff141a24);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
