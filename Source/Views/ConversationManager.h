#pragma once

#include <JuceHeader.h>

#include <creation/assistant/ConversationLedger.h>

#include <functional>
#include <vector>

// The window that lists the assistant's saved conversations and manages them: open one to continue it, archive it (put it
// away without losing it) or restore it, export it to a file, or delete it for good. What each action does is up to the
// host (it owns the storage); this component only shows the list and passes on what was chosen.
class ConversationManager final : public juce::Component,
                                  private juce::ListBoxModel
{
public:
    using Summary = creation::assistant::ConversationSummary;

    struct Actions
    {
        std::function<std::vector<Summary>(bool archived)> list;
        std::function<void(const Summary&)> open;
        std::function<void(const Summary&)> archiveOrRestore;   // archives an active one, restores an archived one
        std::function<void(const Summary&)> exportIt;
        std::function<void(const Summary&)> remove;
    };

    explicit ConversationManager(Actions actionsToUse);

    // Reads the list again (the host calls this after an action finishes, including the ones that wait for the user).
    void refresh();

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override { updateButtons(); }

    const Summary* selected() const;
    void updateButtons();

    Actions actions;
    std::vector<Summary> rows;

    juce::Label heading;
    juce::ToggleButton showArchived { "Show archived" };
    juce::ListBox list { {}, this };
    juce::Label emptyNote;
    juce::TextButton openButton { "Open" };
    juce::TextButton archiveButton { "Archive" };
    juce::TextButton exportButton { "Export..." };
    juce::TextButton deleteButton { "Delete..." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConversationManager)
};
