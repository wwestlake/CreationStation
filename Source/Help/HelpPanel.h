#pragma once

#include "HelpLibrary.h"

#include <JuceHeader.h>

namespace cs::help
{
// The help browser for people who read the help themselves: a search box, the list of topics (or search results), the
// selected topic rendered as text, and links to related topics. Uses the same Library the Virtual Engineer reads.
class HelpPanel final : public juce::Component,
                        private juce::ListBoxModel,
                        private juce::TextEditor::Listener
{
public:
    explicit HelpPanel(const Library& libraryIn);

    // Shows the topic that explains a feature (a help ID), or the overview if none does.
    void showHelpId(const juce::String& helpId);
    void showTopic(const juce::String& topicId);
    void setSearchText(const juce::String& text);

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void textEditorTextChanged(juce::TextEditor&) override;

    void refreshList();
    void renderTopic(const Topic& topic);
    void jumpToBlock(const juce::String& blockId);

    const Library& library;
    juce::TextEditor search;
    juce::ListBox list { {}, this };
    juce::TextEditor body;
    juce::Label relatedLabel;
    juce::OwnedArray<juce::TextButton> relatedButtons;
    juce::Component relatedHolder;

    std::vector<const Topic*> rows;
    std::vector<juce::String> rowNotes;
    const Topic* current = nullptr;
    bool updatingSelection = false;
    std::vector<std::pair<juce::String, int>> blockOffsets;
};
}
