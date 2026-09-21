#include "ConversationManager.h"

namespace
{
const juce::Colour panelColour(0xff141a24);
const juce::Colour rowColour(0xff1b2331);
const juce::Colour rowSelectedColour(0xff2b3d5c);
const juce::Colour textColour(0xffe6edf7);
const juce::Colour dimColour(0xff93a1b8);
const juce::Colour alteredColour(0xffff8a80);

// "2026-09-21 14:05" in the person's own time zone, from the ISO time the ledger stores.
juce::String shortDate(const std::string& iso)
{
    const auto time = juce::Time::fromISO8601(juce::String(iso));
    return time.toMilliseconds() == 0 ? juce::String() : time.formatted("%Y-%m-%d %H:%M");
}
}

ConversationManager::ConversationManager(Actions actionsToUse) : actions(std::move(actionsToUse))
{
    heading.setText("Conversations with the assistant", juce::dontSendNotification);
    heading.setFont(juce::Font(17.0f).boldened());
    heading.setColour(juce::Label::textColourId, textColour);
    addAndMakeVisible(heading);

    showArchived.setColour(juce::ToggleButton::textColourId, dimColour);
    showArchived.setTooltip("Archived conversations are put away, not deleted. Show them to open, restore, export or delete them.");
    showArchived.onClick = [this] { refresh(); };
    addAndMakeVisible(showArchived);

    list.setRowHeight(46);
    list.setColour(juce::ListBox::backgroundColourId, panelColour);
    addAndMakeVisible(list);

    emptyNote.setJustificationType(juce::Justification::centred);
    emptyNote.setColour(juce::Label::textColourId, dimColour);
    addAndMakeVisible(emptyNote);

    openButton.setTooltip("Show this conversation and continue it");
    openButton.onClick = [this] { if (auto* s = selected()) if (actions.open) actions.open(*s); };
    archiveButton.onClick = [this] { if (auto* s = selected()) if (actions.archiveOrRestore) { actions.archiveOrRestore(*s); refresh(); } };
    exportButton.setTooltip("Save this conversation to a file (Markdown to read, or JSON to keep the verifiable record)");
    exportButton.onClick = [this] { if (auto* s = selected()) if (actions.exportIt) actions.exportIt(*s); };
    deleteButton.setTooltip("Delete this conversation for good");
    deleteButton.onClick = [this] { if (auto* s = selected()) if (actions.remove) actions.remove(*s); };
    for (auto* button : { &openButton, &archiveButton, &exportButton, &deleteButton })
        addAndMakeVisible(button);

    setSize(560, 460);
    refresh();
}

void ConversationManager::refresh()
{
    const auto keep = selected() != nullptr ? selected()->id : std::string();
    rows = actions.list ? actions.list(showArchived.getToggleState()) : std::vector<Summary>();
    list.updateContent();

    int reselect = rows.empty() ? -1 : 0;
    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].id == keep)
            reselect = (int) i;
    if (reselect >= 0)
        list.selectRow(reselect);
    else
        list.deselectAllRows();
    list.repaint();

    emptyNote.setText(rows.empty() ? juce::String(showArchived.getToggleState() ? "Nothing is archived." : "No conversations yet. They are saved as you talk with the assistant.") : juce::String(),
                      juce::dontSendNotification);
    updateButtons();
}

const ConversationManager::Summary* ConversationManager::selected() const
{
    const auto row = list.getSelectedRow();
    return juce::isPositiveAndBelow(row, (int) rows.size()) ? &rows[(size_t) row] : nullptr;
}

void ConversationManager::updateButtons()
{
    const auto* s = selected();
    // A conversation that failed its integrity check can be exported (as evidence) and deleted, but never opened or moved.
    openButton.setEnabled(s != nullptr && s->intact && ! s->archived);
    archiveButton.setButtonText(s != nullptr && s->archived ? "Restore" : "Archive");
    archiveButton.setEnabled(s != nullptr && s->intact);
    exportButton.setEnabled(s != nullptr && s->intact);
    deleteButton.setEnabled(s != nullptr);
}

void ConversationManager::paint(juce::Graphics& g)
{
    g.fillAll(panelColour);
}

void ConversationManager::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto top = area.removeFromTop(30);
    showArchived.setBounds(top.removeFromRight(130));
    heading.setBounds(top);
    area.removeFromTop(8);

    auto buttons = area.removeFromBottom(34);
    area.removeFromBottom(10);
    const auto width = (buttons.getWidth() - 3 * 8) / 4;
    openButton.setBounds(buttons.removeFromLeft(width));
    buttons.removeFromLeft(8);
    archiveButton.setBounds(buttons.removeFromLeft(width));
    buttons.removeFromLeft(8);
    exportButton.setBounds(buttons.removeFromLeft(width));
    buttons.removeFromLeft(8);
    deleteButton.setBounds(buttons);

    list.setBounds(area);
    emptyNote.setBounds(area.reduced(20));
}

void ConversationManager::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool isSelected)
{
    if (! juce::isPositiveAndBelow(row, (int) rows.size()))
        return;
    const auto& s = rows[(size_t) row];

    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(2, 2);
    g.setColour(isSelected ? rowSelectedColour : rowColour);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

    auto inner = bounds.reduced(10, 4);
    g.setColour(s.intact ? textColour : alteredColour);
    g.setFont(s.intact ? juce::Font(14.5f) : juce::Font(14.5f).boldened());
    g.drawText(juce::String::fromUTF8(s.title.c_str()), inner.removeFromTop(inner.getHeight() / 2 + 2), juce::Justification::centredLeft, true);

    juce::String detail;
    if (s.intact)
    {
        detail << shortDate(s.updatedAt) << "   " << s.blockCount << (s.blockCount == 1 ? " message" : " messages");
        if (s.archived)
            detail << "   archived";
    }
    else
        detail = "Failed its integrity check: it was changed outside the assistant. It can be deleted but not opened.";
    g.setColour(s.intact ? dimColour : alteredColour);
    g.setFont(juce::Font(12.0f));
    g.drawText(detail, inner, juce::Justification::centredLeft, true);
}

void ConversationManager::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow(row, (int) rows.size()) && rows[(size_t) row].intact && ! rows[(size_t) row].archived && actions.open)
        actions.open(rows[(size_t) row]);
}
