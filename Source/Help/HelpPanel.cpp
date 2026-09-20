#include "HelpPanel.h"

#include <algorithm>

namespace cs::help
{
namespace
{
const juce::Colour bg(0xff11151c), panelBg(0xff171c25), text(0xffe4e8ee), muted(0xff9aa5b4), accent(0xff5fb0ff);

juce::String labelFor(const Topic& t)
{
    return t.type.substring(0, 1).toUpperCase() + t.type.substring(1);
}
}

HelpPanel::HelpPanel(const Library& libraryIn) : library(libraryIn)
{
    search.setTextToShowWhenEmpty("Search help (for example: import a video)", muted);
    search.setColour(juce::TextEditor::backgroundColourId, panelBg);
    search.setColour(juce::TextEditor::textColourId, text);
    search.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2b3442));
    search.addListener(this);
    addAndMakeVisible(search);

    list.setRowHeight(44);
    list.setColour(juce::ListBox::backgroundColourId, panelBg);
    addAndMakeVisible(list);

    body.setMultiLine(true, true);
    body.setReadOnly(true);
    body.setCaretVisible(false);
    body.setScrollbarsShown(true);
    body.setColour(juce::TextEditor::backgroundColourId, bg);
    body.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2b3442));
    addAndMakeVisible(body);

    relatedLabel.setText("Related topics", juce::dontSendNotification);
    relatedLabel.setColour(juce::Label::textColourId, muted);
    relatedLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(relatedLabel);
    addAndMakeVisible(relatedHolder);

    refreshList();
    if (auto* overview = library.findTopic("djehuti.station.overview"))
        showTopic(overview->id);
    else if (! rows.empty())
        showTopic(rows.front()->id);
}

void HelpPanel::paint(juce::Graphics& g)
{
    g.fillAll(bg);
}

void HelpPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto left = area.removeFromLeft(juce::jmin(300, area.getWidth() / 3));
    search.setBounds(left.removeFromTop(28));
    left.removeFromTop(6);
    list.setBounds(left);
    area.removeFromLeft(8);

    auto related = area.removeFromBottom(relatedButtons.isEmpty() ? 0 : 62);
    if (! relatedButtons.isEmpty())
    {
        relatedLabel.setBounds(related.removeFromTop(18));
        relatedHolder.setBounds(related);
        int x = 0;
        for (auto* b : relatedButtons)
        {
            const int w = juce::jmin(240, juce::GlyphArrangement::getStringWidthInt(juce::FontOptions(13.0f), b->getButtonText()) + 24);
            b->setBounds(x, 0, w, related.getHeight());
            x += w + 6;
        }
    }
    else
    {
        relatedLabel.setBounds({});
        relatedHolder.setBounds({});
    }
    body.setBounds(area);
}

int HelpPanel::getNumRows() { return (int) rows.size(); }

void HelpPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    juce::ignoreUnused(height);
    if (! juce::isPositiveAndBelow(row, (int) rows.size()))
        return;
    if (selected)
        g.fillAll(juce::Colour(0xff24405f));
    const auto& t = *rows[(size_t) row];
    g.setColour(text);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(t.title, 10, 4, width - 16, 18, juce::Justification::centredLeft, true);
    g.setColour(muted);
    g.setFont(juce::FontOptions(12.0f));
    const auto note = rowNotes[(size_t) row];
    g.drawText(labelFor(t) + (note.isNotEmpty() ? "  -  " + note : juce::String()), 10, 22, width - 16, 16,
               juce::Justification::centredLeft, true);
}

void HelpPanel::selectedRowsChanged(int row)
{
    if (updatingSelection || ! juce::isPositiveAndBelow(row, (int) rows.size()))
        return;
    renderTopic(*rows[(size_t) row]);
}

void HelpPanel::textEditorTextChanged(juce::TextEditor&)
{
    refreshList();
}

void HelpPanel::setSearchText(const juce::String& textIn)
{
    search.setText(textIn, true);
}

void HelpPanel::refreshList()
{
    rows.clear();
    rowNotes.clear();
    const auto q = search.getText().trim();
    if (q.isEmpty())
    {
        for (const auto& t : library.topics())
        {
            rows.push_back(&t);
            rowNotes.emplace_back();
        }
    }
    else
    {
        for (const auto& h : library.search(q, 40))
        {
            rows.push_back(h.topic);
            rowNotes.push_back(h.snippet);
        }
    }
    list.updateContent();
    list.repaint();
    if (! rows.empty() && q.isNotEmpty())
        list.selectRow(0);
}

void HelpPanel::showHelpId(const juce::String& helpId)
{
    search.setText({}, false);
    refreshList();

    const auto* t = library.primaryTopicFor(helpId);
    if (t == nullptr)
        t = library.findTopic("djehuti.station.overview");
    if (t == nullptr)
        return;

    juce::String anchor;
    for (const auto& c : t->contexts)
        if (c.helpId == helpId)
        {
            anchor = c.anchorBlockId;
            break;
        }
    showTopic(t->id);
    if (anchor.isNotEmpty())
        jumpToBlock(anchor);
}

void HelpPanel::showTopic(const juce::String& topicId)
{
    const auto* t = library.findTopic(topicId);
    if (t == nullptr)
        return;

    auto it = std::find(rows.begin(), rows.end(), t);
    if (it == rows.end())
    {
        search.setText({}, false);
        refreshList();
        it = std::find(rows.begin(), rows.end(), t);
    }
    if (it != rows.end())
    {
        updatingSelection = true;
        list.selectRow((int) std::distance(rows.begin(), it));
        updatingSelection = false;
    }
    renderTopic(*t);
}

void HelpPanel::renderTopic(const Topic& topic)
{
    current = &topic;
    blockOffsets.clear();
    body.clear();

    auto add = [this](const juce::String& s, float size, bool bold, juce::Colour c)
    {
        body.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
        body.setColour(juce::TextEditor::textColourId, c);
        body.moveCaretToEnd();
        body.insertTextAtCaret(s);
    };

    add(topic.title + "\n", 22.0f, true, text);
    add(labelFor(topic) + (topic.status == "draft" ? "  -  draft, not yet checked by a person" : juce::String()) + "\n\n",
        12.0f, false, muted);
    add(topic.summary + "\n\n", 15.0f, false, text);

    for (const auto& b : topic.blocks)
    {
        blockOffsets.emplace_back(b.id, body.getText().length());
        if (b.kind == "steps")
        {
            add(b.title + "\n", 16.0f, true, accent);
            int n = 1;
            for (const auto& s : b.steps)
            {
                add(juce::String(n++) + ".  " + s.instruction + "\n", 14.0f, false, text);
                if (s.expectedResult.isNotEmpty())
                    add("      Result: " + s.expectedResult + "\n", 13.0f, false, muted);
            }
            add("\n", 10.0f, false, text);
        }
        else if (b.kind == "troubleshooting")
        {
            add(b.title + "\n", 16.0f, true, accent);
            if (! b.symptoms.isEmpty())
                add("You may see: " + b.symptoms.joinIntoString("; ") + "\n", 13.0f, false, muted);
            add(b.resolution + "\n", 14.0f, false, text);
            if (b.cause.isNotEmpty())
                add("Why: " + b.cause + "\n", 13.0f, false, muted);
            add("\n", 10.0f, false, text);
        }
        else if (b.kind == "definition")
        {
            add(b.term + "  ", 14.0f, true, accent);
            add(b.definition + "\n\n", 14.0f, false, text);
        }
        else if (b.kind == "note" || b.kind == "warning")
        {
            add(b.kind == "warning" ? "Warning: " : "Note: ", 14.0f, true,
                b.kind == "warning" ? juce::Colour(0xffffb454) : accent);
            add(b.text + "\n\n", 14.0f, false, text);
        }
        else
            add(b.text + "\n\n", 14.0f, false, text);
    }

    body.setCaretPosition(0);
    body.scrollEditorToPositionCaret(0, 0);

    relatedButtons.clear();
    for (const auto& r : topic.relations)
    {
        const auto* t = library.findTopic(r.topicId);
        if (t == nullptr)
            continue;
        auto* b = relatedButtons.add(new juce::TextButton(t->title));
        b->setTooltip(r.type);
        const auto id = t->id;
        // Showing a topic rebuilds these buttons, so do it after this click has finished.
        b->onClick = [safe = juce::Component::SafePointer<HelpPanel>(this), id]
        {
            juce::MessageManager::callAsync([safe, id] { if (safe != nullptr) safe->showTopic(id); });
        };
        relatedHolder.addAndMakeVisible(b);
    }
    resized();
}

void HelpPanel::jumpToBlock(const juce::String& blockId)
{
    for (const auto& [id, offset] : blockOffsets)
        if (id == blockId)
        {
            body.setCaretPosition(offset);
            body.scrollEditorToPositionCaret(0, 0);
            return;
        }
}
}
