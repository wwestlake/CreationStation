#include "AiPanel.h"

namespace
{
// The message box starts one line high and grows with what is typed, up to about eight lines.
constexpr int kMinPromptHeight = 34;
constexpr int kMaxPromptHeight = 180;

    struct MarkdownRenderStyle
    {
        juce::Font bodyFont { 14.0f };
        juce::Font boldFont { 14.0f, juce::Font::bold };
        juce::Font heading1 { 19.0f, juce::Font::bold };
        juce::Font heading2 { 17.0f, juce::Font::bold };
        juce::Font heading3 { 15.0f, juce::Font::bold };
        juce::Font codeFont;
        juce::Colour bodyColour { 0xffd8e0ee };
        juce::Colour headingColour { 0xfff0f5ff };
        juce::Colour accentColour { 0xff8fd3ff };
        juce::Colour codeColour { 0xffcbb8ff };
    };

    MarkdownRenderStyle makeRenderStyle()
    {
        MarkdownRenderStyle style;
        style.codeFont.setTypefaceName(juce::Font::getDefaultMonospacedFontName());
        style.codeFont.setHeight(13.0f);
        return style;
    }

    static juce::String stripLeadingMarkdownMarks(juce::String text)
    {
        return text.trimStart().trimCharactersAtStart("#").trimStart();
    }

    static void appendInlineMarkdown(juce::AttributedString& out,
                                     const juce::String& text,
                                     const MarkdownRenderStyle& style,
                                     bool userMessage)
    {
        juce::String buffer;
        bool bold = false;
        bool code = false;
        bool math = false;

        auto flush = [&]
        {
            if (buffer.isEmpty())
                return;

            auto font = code || math ? style.codeFont : style.bodyFont;
            if (bold && ! code && ! math)
                font = font.boldened();

            auto colour = style.bodyColour;
            if (code)
                colour = style.codeColour;
            else if (math)
                colour = style.accentColour;
            else if (bold)
                colour = style.headingColour;
            else if (userMessage)
                colour = juce::Colour(0xfff2f7ff);

            out.append(buffer, font, colour);
            buffer.clear();
        };

        for (int index = 0; index < text.length(); ++index)
        {
            auto ch = text[index];

            if (! code && index + 1 < text.length() && text[index] == '*' && text[index + 1] == '*')
            {
                flush();
                bold = ! bold;
                ++index;
                continue;
            }

            if (! math && ch == '`')
            {
                flush();
                code = ! code;
                continue;
            }

            if (! code && ch == '$')
            {
                flush();
                math = ! math;
                continue;
            }

            buffer << ch;
        }

        flush();
    }

    static juce::AttributedString buildMarkdownAttributedString(const juce::String& badgeText,
                                                                const juce::String& bodyText,
                                                                bool userMessage)
    {
        auto style = makeRenderStyle();
        juce::AttributedString text;
        text.setJustification(juce::Justification::topLeft);
        text.setWordWrap(juce::AttributedString::byWord);
        text.setLineSpacing(1.18f);

        if (badgeText.isNotEmpty())
        {
            auto badgeFont = style.bodyFont.boldened();
            badgeFont.setHeight(12.0f);
            text.append(badgeText, badgeFont, userMessage ? juce::Colour(0xff9cd4ff) : style.accentColour);
            text.append("\n", style.bodyFont, style.bodyColour);
        }

        auto lines = juce::StringArray::fromLines(bodyText);
        bool inCodeBlock = false;

        for (int index = 0; index < lines.size(); ++index)
        {
            auto line = lines[index];
            auto trimmed = line.trimStart();

            if (trimmed.startsWith("```"))
            {
                inCodeBlock = ! inCodeBlock;
                if (index + 1 < lines.size())
                    text.append("\n", style.bodyFont, style.bodyColour);
                continue;
            }

            if (inCodeBlock)
            {
                auto codeLine = line;
                if (codeLine.isEmpty())
                    codeLine = " ";
                text.append(codeLine, style.codeFont, style.codeColour);
                if (index + 1 < lines.size())
                    text.append("\n", style.codeFont, style.codeColour);
                continue;
            }

            auto headingLevel = 0;
            while (headingLevel < trimmed.length() && trimmed[headingLevel] == '#')
                ++headingLevel;

            if (headingLevel > 0 && headingLevel <= 3
                && (headingLevel == trimmed.length()
                    || juce::CharacterFunctions::isWhitespace(trimmed[headingLevel])))
            {
                auto headingText = stripLeadingMarkdownMarks(trimmed);
                auto font = headingLevel == 1 ? style.heading1
                                               : headingLevel == 2 ? style.heading2
                                                                   : style.heading3;
                text.append(headingText, font, style.headingColour);
                if (index + 1 < lines.size())
                    text.append("\n", style.bodyFont, style.bodyColour);
                continue;
            }

            if (trimmed.startsWith("- ") || trimmed.startsWith("* "))
            {
                auto bulletText = "- " + trimmed.fromFirstOccurrenceOf(" ", false, false).trimStart();
                appendInlineMarkdown(text, bulletText, style, userMessage);
            }
            else
            {
                appendInlineMarkdown(text, line, style, userMessage);
            }

            if (index + 1 < lines.size())
                text.append("\n", style.bodyFont, style.bodyColour);
        }

        if (text.getText().isEmpty())
            text.append(" ", style.bodyFont, style.bodyColour);

        return text;
    }

    class ChatBubbleComponent final : public juce::Component
    {
    public:
        ChatBubbleComponent()
        {
            setWantsKeyboardFocus(true);
            setMouseCursor(juce::MouseCursor::IBeamCursor);
        }

        // Called when the user starts selecting in this bubble, so the transcript can clear the others.
        std::function<void(ChatBubbleComponent*)> onSelectionStarted;

        bool isUserMessage() const noexcept { return isUser; }

        void setMessage(bool user, const juce::String& badge, const juce::String& content)
        {
            isUser = user;
            badgeText = badge;
            bodyText = content;
            selection = {};
            repaint();
        }

        void clearSelection()
        {
            if (! selection.isEmpty())
            {
                selection = {};
                repaint();
            }
        }

        int getPreferredHeight(int width) const
        {
            auto textWidth = juce::jmax(60, width - 28);
            juce::TextLayout layout;
            layout.createLayout(buildMarkdownAttributedString(badgeText, bodyText, isUser), (float) textWidth);
            return juce::roundToInt(layout.getHeight()) + 26;
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(2.0f);
            auto bubbleColour = isUser ? juce::Colour(0xff23466d) : juce::Colour(0xff171d27);
            auto outlineColour = isUser ? juce::Colour(0xff5da5ff) : juce::Colour(0xff2c394c);
            auto fillAlpha = isUser ? 1.0f : 0.98f;

            g.setColour(bubbleColour.withAlpha(fillAlpha));
            g.fillRoundedRectangle(bounds, 16.0f);
            g.setColour(outlineColour.withAlpha(0.9f));
            g.drawRoundedRectangle(bounds, 16.0f, 1.0f);

            juce::TextLayout layout;
            auto textArea = makeLayout(layout);

            if (! selection.isEmpty())
            {
                g.setColour(juce::Colour(0xff3d78c4).withAlpha(0.75f));
                forEachGlyph(layout, textArea, [&](int index, juce::Rectangle<float> box)
                {
                    if (selection.contains(index))
                        g.fillRect(box);
                    return true;
                });
            }

            layout.draw(g, textArea);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            grabKeyboardFocus();

            if (event.mods.isPopupMenu())
            {
                showContextMenu();
                return;
            }

            if (onSelectionStarted)
                onSelectionStarted(this);

            dragAnchor = characterIndexAt(event.position);
            selection = {};
            repaint();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
                return;
            const auto here = characterIndexAt(event.position);
            selection = juce::Range<int>::between(dragAnchor, here);
            repaint();
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            const auto text = shownText();
            auto index = juce::jlimit(0, juce::jmax(0, text.length() - 1), characterIndexAt(event.position));
            auto isWordChar = [&](int i) { return juce::CharacterFunctions::isLetterOrDigit(text[i]) || text[i] == '_'; };
            auto start = index;
            auto end = index;
            while (start > 0 && isWordChar(start - 1))
                --start;
            while (end < text.length() && isWordChar(end))
                ++end;
            selection = { start, end };
            repaint();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key.getModifiers().isCommandDown())
            {
                const auto letter = juce::CharacterFunctions::toUpperCase((juce::juce_wchar) key.getKeyCode());
                if (letter == 'C')
                {
                    copySelection();
                    return true;
                }
                if (letter == 'A')
                {
                    selection = { 0, shownText().length() };
                    repaint();
                    return true;
                }
            }
            return false;
        }

    private:
        bool isUser = false;
        juce::String badgeText;
        juce::String bodyText;
        juce::Range<int> selection;
        int dragAnchor = 0;

        juce::String shownText() const
        {
            return buildMarkdownAttributedString(badgeText, bodyText, isUser).getText();
        }

        juce::Rectangle<float> makeLayout(juce::TextLayout& layout) const
        {
            auto textArea = getLocalBounds().toFloat().reduced(2.0f).reduced(14.0f, 10.0f);
            layout.createLayout(buildMarkdownAttributedString(badgeText, bodyText, isUser), textArea.getWidth());
            return textArea;
        }

        // Calls fn(characterIndex, boxInComponentSpace) for every glyph; stops if fn returns false.
        template <typename Fn>
        static void forEachGlyph(const juce::TextLayout& layout, juce::Rectangle<float> textArea, Fn&& fn)
        {
            for (int l = 0; l < layout.getNumLines(); ++l)
            {
                const auto& line = layout.getLine(l);
                const auto lineBounds = line.getLineBounds();
                for (const auto* run : line.runs)
                {
                    for (int i = 0; i < run->glyphs.size(); ++i)
                    {
                        const auto& glyph = run->glyphs.getReference(i);
                        const auto index = run->stringRange.getStart() + i;
                        const juce::Rectangle<float> box(textArea.getX() + line.lineOrigin.x + glyph.anchor.x,
                                                        textArea.getY() + lineBounds.getY(),
                                                        glyph.width, lineBounds.getHeight());
                        if (! fn(index, box))
                            return;
                    }
                }
            }
        }

        int characterIndexAt(juce::Point<float> point) const
        {
            juce::TextLayout layout;
            const auto textArea = makeLayout(layout);
            const auto total = shownText().length();
            if (layout.getNumLines() == 0)
                return 0;

            // Pick the line under the point (clamped to the first or last line), then the nearest glyph edge on it.
            int chosenLine = layout.getNumLines() - 1;
            for (int l = 0; l < layout.getNumLines(); ++l)
            {
                const auto b = layout.getLine(l).getLineBounds();
                if (point.y < textArea.getY() + b.getBottom())
                {
                    chosenLine = l;
                    break;
                }
            }

            const auto& line = layout.getLine(chosenLine);
            int result = line.stringRange.getEnd();
            bool found = false;
            for (const auto* run : line.runs)
            {
                for (int i = 0; i < run->glyphs.size() && ! found; ++i)
                {
                    const auto& glyph = run->glyphs.getReference(i);
                    const auto left = textArea.getX() + line.lineOrigin.x + glyph.anchor.x;
                    if (point.x < left + glyph.width * 0.5f)
                    {
                        result = run->stringRange.getStart() + i;
                        found = true;
                    }
                }
                if (found)
                    break;
            }
            return juce::jlimit(0, total, result);
        }

        void copySelection() const
        {
            const auto text = shownText();
            const auto range = selection.isEmpty() ? juce::Range<int>(0, text.length()) : selection;
            juce::SystemClipboard::copyTextToClipboard(text.substring(range.getStart(), range.getEnd()));
        }

        void showContextMenu()
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Copy", ! selection.isEmpty());
            menu.addItem(2, "Copy message");
            menu.addItem(3, "Select all");
            juce::Component::SafePointer<ChatBubbleComponent> safe(this);
            menu.showMenuAsync({}, [safe](int result)
            {
                if (safe == nullptr)
                    return;
                if (result == 1)
                    safe->copySelection();
                else if (result == 2)
                    juce::SystemClipboard::copyTextToClipboard(safe->bodyText);
                else if (result == 3)
                {
                    safe->selection = { 0, safe->shownText().length() };
                    safe->repaint();
                }
            });
        }
    };

} // namespace

class ChatTranscriptComponent final : public juce::Component
{
public:
    int addMessage(bool user, const juce::String& badge, const juce::String& content)
    {
        auto* bubble = new ChatBubbleComponent();
        bubble->onSelectionStarted = [this](ChatBubbleComponent* active)
        {
            for (auto* other : bubbles)
                if (other != active)
                    other->clearSelection();
        };
        bubble->setMessage(user, badge, content);
        addAndMakeVisible(bubble);
        bubbles.add(bubble);
        layoutMessages();
        return bubbles.size() - 1;
    }

    void updateMessage(int index, bool user, const juce::String& badge, const juce::String& content)
    {
        if (! juce::isPositiveAndBelow(index, bubbles.size()))
            return;

            bubbles[index]->setMessage(user, badge, content);
        layoutMessages();
    }

    void clear()
    {
        bubbles.clear(true);
        layoutMessages();
    }

    int getBubbleCount() const noexcept
    {
        return bubbles.size();
    }

    bool hasBubbleAt(int index) const noexcept
    {
        return juce::isPositiveAndBelow(index, bubbles.size());
    }

    void resized() override
    {
        layoutMessages();
    }

    int getContentHeightForWidth(int width) const
    {
        auto contentWidth = juce::jmax(120, width);
        auto y = 14;

        for (const auto* bubble : bubbles)
        {
            auto bubbleWidth = juce::jmin(contentWidth - 24, juce::roundToInt(contentWidth * 0.88f));
            y += bubble->getPreferredHeight(bubbleWidth) + 10;
        }

        return y + 10;
    }

private:
    juce::OwnedArray<ChatBubbleComponent> bubbles;

    void layoutMessages()
    {
        auto width = getWidth();
        if (width <= 0)
            return;

        auto contentWidth = width;
        auto y = 14;
        auto bubbleWidth = juce::jmax(220, juce::roundToInt((float) contentWidth * 0.88f));
        bubbleWidth = juce::jmin(bubbleWidth, contentWidth - 24);

        for (auto* bubble : bubbles)
        {
            if (bubble == nullptr)
                continue;

            auto height = bubble->getPreferredHeight(bubbleWidth);
            auto x = bubble->isUserMessage() ? juce::jmax(12, contentWidth - bubbleWidth - 12)
                                             : 12;

            bubble->setBounds(x, y, bubbleWidth, height);
            y += height + 10;
        }
    }
};

AiPanel::~AiPanel() = default;

AiPanel::AiPanel()
{
    setName("AI");

    headerLabel.setText("Virtual Engineer", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    subtitleLabel.setText("Chat first, tools later. The app keeps the plumbing hidden.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(subtitleLabel);

    modeLabelTitle.setText("Mode", juce::dontSendNotification);
    modeLabelTitle.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(modeLabelTitle);

    auto configureModeButton = [this](juce::TextButton& button, GuidanceMode mode)
    {
        button.setClickingTogglesState(true);
        button.onClick = [this, mode]
        {
            setGuidanceMode(mode);
        };
        addAndMakeVisible(button);
    };

    configureModeButton(normalModeButton, GuidanceMode::normal);
    configureModeButton(learnModeButton, GuidanceMode::learn);
    configureModeButton(researchModeButton, GuidanceMode::research);

    normalModeButton.setTooltip("Normal mode - direct answers");
    learnModeButton.setTooltip("Learn mode - guided, teaching-style answers");
    researchModeButton.setTooltip("Research mode - deeper, more thorough answers");

    accountLabel.setText("Account", juce::dontSendNotification);
    accountLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(accountLabel);

    accountComboBox.setTextWhenNoChoicesAvailable("No suite AI accounts configured");
    accountComboBox.addListener(this);
    addAndMakeVisible(accountComboBox);

    modelLabel.setText("Model", juce::dontSendNotification);
    modelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(modelLabel);

    modelComboBox.setEditableText(true);
    modelComboBox.setTextWhenNothingSelected("Select or type a model");
    modelComboBox.setTextWhenNoChoicesAvailable("No models loaded");
    modelComboBox.addListener(this);
    addAndMakeVisible(modelComboBox);

    accessLabelTitle.setText("Access", juce::dontSendNotification);
    accessLabelTitle.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(accessLabelTitle);

    accessComboBox.addItem("Ask first", 1);
    accessComboBox.addItem("App only", 2);
    accessComboBox.addItem("Files", 3);
    accessComboBox.addItem("Full access", 4);
    accessComboBox.addListener(this);
    addAndMakeVisible(accessComboBox);

    promptLabel.setText("Message", juce::dontSendNotification);
    promptLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(promptLabel);

    transcriptContent = std::make_unique<ChatTranscriptComponent>();
    transcriptViewport.setViewedComponent(transcriptContent.get(), false);
    transcriptViewport.setScrollBarsShown(true, false);
    transcriptViewport.setScrollBarThickness(10);
    addAndMakeVisible(transcriptViewport);

    promptEditor.setMultiLine(true, true);
    promptEditor.setReturnKeyStartsNewLine(true);
    promptEditor.setScrollbarsShown(true);
    // Room at the right for the send arrow, which sits inside the box. The box starts one line high
    // (the mode's hint shows while it is empty) and grows as you type.
    promptEditor.setBorder(juce::BorderSize<int>(4, 6, 4, 38));
    promptEditor.onSend = [this] { sendButton.triggerClick(); };
    promptEditor.addListener(this);
    addAndMakeVisible(promptEditor);

    sendButton.onClick = [this]
    {
        auto prompt = getPromptText().trim();
        if (prompt.isEmpty())
            return;

        auto submittedPrompt = buildSubmissionPrompt();
        lastQuestion = prompt;

        appendUserMessage(prompt);
        pendingAssistantBubbleIndex = transcriptContent->addMessage(false, "Virtual Engineer", "Thinking...");
        refreshChatLayout();
        scrollChatToBottom();

        promptEditor.clear();
        refreshPromptHeight();

        if (onPromptSubmitted)
            onPromptSubmitted(submittedPrompt);
    };
    sendButton.setTooltip("Send your message to the assistant");
    addAndMakeVisible(sendButton);

    enterSendsToggle.setTooltip("On: Enter sends and Shift+Enter starts a new line. Off: Enter starts a new line and Ctrl+Enter sends.");
    enterSendsToggle.onClick = [this]
    {
        setEnterSendsMessage(enterSendsToggle.getToggleState());
        if (onEnterSendsChanged)
            onEnterSendsChanged(promptEditor.enterSends);
    };
    addAndMakeVisible(enterSendsToggle);

    collapseButton.setTooltip("Collapse or expand the assistant sidebar");
    collapseButton.onClick = [this]
    {
        setCollapsed(! collapsed);
        if (onCollapsedChanged)
            onCollapsedChanged(collapsed);
    };
    addAndMakeVisible(collapseButton);

    footerHintLabel.setText("Markdown formatting is supported. Permission stays conservative by default.", juce::dontSendNotification);
    footerHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f90a8));
    addAndMakeVisible(footerHintLabel);

    refreshModeUi();
    refreshAccessUi();
    refreshPromptHeight();
    transcriptContent->addMessage(false, "Virtual Engineer", "I'm ready when you are. Ask me to build, explain, or research.");
    refreshChatLayout();
    scrollChatToBottom();
    setCollapsed(false);
}

juce::String AiPanel::modeLabel(GuidanceMode mode)
{
    switch (mode)
    {
        case GuidanceMode::normal: return "Normal";
        case GuidanceMode::learn: return "Learn";
        case GuidanceMode::research: return "Research";
    }

    return "Normal";
}

juce::String AiPanel::modeDescription(GuidanceMode mode)
{
    switch (mode)
    {
        case GuidanceMode::normal:
            return "Direct help for building, debugging, and designing.";
        case GuidanceMode::learn:
            return "Virtual Engineer mode. One step at a time, with plain explanations.";
        case GuidanceMode::research:
            return "Verify facts, compare options, and call out uncertainty.";
    }

    return "Direct help for building, debugging, and designing.";
}

juce::String AiPanel::modePromptPrefix(GuidanceMode mode)
{
    switch (mode)
    {
        case GuidanceMode::normal:
            return {};
        case GuidanceMode::learn:
            return "Learn mode: act like a patient coach. Keep guidance practical, step-by-step, and easy to follow.\n\n";
        case GuidanceMode::research:
            return "Research mode: verify claims, note uncertainty, and prefer grounded answers.\n\n";
    }

    return {};
}

juce::String AiPanel::accessLabel(AccessLevel level)
{
    switch (level)
    {
        case AccessLevel::askFirst: return "Ask first";
        case AccessLevel::appOnly: return "App only";
        case AccessLevel::fileChanges: return "Files";
        case AccessLevel::fullAccess: return "Full access";
    }

    return "Ask first";
}

juce::String AiPanel::accessDescription(AccessLevel level)
{
    switch (level)
    {
        case AccessLevel::askFirst:
            return "Ask before anything that touches files, settings, or external services.";
        case AccessLevel::appOnly:
            return "Allow internal app actions, but ask before file or network changes.";
        case AccessLevel::fileChanges:
            return "Allow file edits and project changes with guardrails.";
        case AccessLevel::fullAccess:
            return "Use with care: the AI may act more freely across allowed tools.";
    }

    return "Ask before anything that touches files, settings, or external services.";
}

juce::String AiPanel::accessPromptPrefix(AccessLevel level)
{
    switch (level)
    {
        case AccessLevel::askFirst:
            return "Permission policy: ask before any action that could modify files, settings, external services, or user data.\n\n";
        case AccessLevel::appOnly:
            return "Permission policy: internal app actions may proceed, but ask before touching files, network calls, or user data.\n\n";
        case AccessLevel::fileChanges:
            return "Permission policy: file and project changes are allowed when needed, but still ask before anything external or destructive.\n\n";
        case AccessLevel::fullAccess:
            return "Permission policy: full access is enabled for this session. Be careful and explicit about side effects.\n\n";
    }

    return {};
}

void AiPanel::setGuidanceMode(GuidanceMode newMode)
{
    if (guidanceMode == newMode)
        return;

    guidanceMode = newMode;
    refreshModeUi();

    if (onModeChanged)
        onModeChanged(guidanceMode);
}

void AiPanel::setAccessLevel(AccessLevel newLevel)
{
    if (accessLevel == newLevel)
        return;

    accessLevel = newLevel;
    refreshAccessUi();

    if (onAccessChanged)
        onAccessChanged(accessLevel);
}

void AiPanel::setAvailableModels(const juce::StringArray& modelIds, const juce::String& statusText)
{
    updatingComboBoxes = true;

    availableModels = modelIds;
    auto previousSelection = modelComboBox.getText().trim();

    modelComboBox.clear(juce::dontSendNotification);
    for (int index = 0; index < modelIds.size(); ++index)
        modelComboBox.addItem(modelIds[index], index + 1);

    if (previousSelection.isNotEmpty() && modelIds.contains(previousSelection, false))
        modelComboBox.setText(previousSelection, juce::dontSendNotification);
    else if (modelIds.size() > 0)
        modelComboBox.setText(modelIds[0], juce::dontSendNotification);

    footerHintLabel.setText(statusText.isNotEmpty() ? statusText
                                                    : "Markdown formatting is supported. Permission stays conservative by default.",
                              juce::dontSendNotification);

    updatingComboBoxes = false;
    refreshPromptHeight();
}

void AiPanel::setAvailableAccounts(const juce::Array<AccountEntry>& accounts, const juce::String& selectedAccountId)
{
    updatingComboBoxes = true;

    availableAccounts = accounts;
    accountComboBox.clear(juce::dontSendNotification);
    for (int index = 0; index < availableAccounts.size(); ++index)
        accountComboBox.addItem(availableAccounts[index].displayName, index + 1);

    auto selectedIndex = -1;
    for (int index = 0; index < availableAccounts.size(); ++index)
        if (availableAccounts[index].accountId == selectedAccountId)
            selectedIndex = index;
    if (selectedIndex < 0 && availableAccounts.size() > 0)
        selectedIndex = 0;

    accountComboBox.setSelectedItemIndex(selectedIndex, juce::dontSendNotification);
    updatingComboBoxes = false;
}

juce::String AiPanel::getSelectedAccountId() const
{
    const auto index = accountComboBox.getSelectedItemIndex();
    if (! juce::isPositiveAndBelow(index, availableAccounts.size()))
        return {};
    return availableAccounts[index].accountId;
}

void AiPanel::setSelectedModel(const juce::String& modelName)
{
    updatingComboBoxes = true;
    modelComboBox.setText(modelName, juce::dontSendNotification);
    updatingComboBoxes = false;
}

juce::String AiPanel::getSelectedModel() const
{
    return modelComboBox.getText().trim();
}

void AiPanel::refreshModeUi()
{
    normalModeButton.setToggleState(guidanceMode == GuidanceMode::normal, juce::dontSendNotification);
    learnModeButton.setToggleState(guidanceMode == GuidanceMode::learn, juce::dontSendNotification);
    researchModeButton.setToggleState(guidanceMode == GuidanceMode::research, juce::dontSendNotification);

    subtitleLabel.setText(modeDescription(guidanceMode), juce::dontSendNotification);
    promptEditor.setTextToShowWhenEmpty(modeDescription(guidanceMode), juce::Colour(0xff6d7d91));
    repaint();
}

void AiPanel::refreshAccessUi()
{
    accessComboBox.setSelectedId(static_cast<int>(accessLevel) + 1, juce::dontSendNotification);
    repaint();
}

void AiPanel::setContextPacket(const CreationStationContextEngine::ContextPacket& packet)
{
    latestContextSummary = packet.summary;
}

void AiPanel::setTaskPlan(const CreationStationTaskPlanner::TaskPlan& plan)
{
    currentPlan = plan;
    latestPlanSummary = CreationStationTaskPlanner::describePlan(plan);
}

void AiPanel::setAssistantResponse(const juce::String& responseText)
{
    if (pendingAssistantBubbleIndex >= 0 && transcriptContent->hasBubbleAt(pendingAssistantBubbleIndex))
    {
        transcriptContent->updateMessage(pendingAssistantBubbleIndex, false, "Virtual Engineer", responseText);
    }
    else
    {
        pendingAssistantBubbleIndex = transcriptContent->addMessage(false, "Virtual Engineer", responseText);
    }

    refreshChatLayout();
    scrollChatToBottom();
}

void AiPanel::appendUserMessage(const juce::String& promptText)
{
    transcriptContent->addMessage(true, "You", promptText);
    refreshChatLayout();
    scrollChatToBottom();
}

juce::String AiPanel::getPromptText() const
{
    return promptEditor.getText();
}

juce::String AiPanel::buildSubmissionPrompt() const
{
    auto prompt = promptEditor.getText().trim();
    auto prefix = modePromptPrefix(guidanceMode) + accessPromptPrefix(accessLevel);

    if (prompt.isEmpty())
        return prefix + "Help me with the current creative task.";

    return prefix + prompt;
}

void AiPanel::setCollapsed(bool shouldCollapse)
{
    collapsed = shouldCollapse;
    collapseButton.setButtonText(collapsed ? "AI" : "Hide");

    headerLabel.setVisible(! collapsed);
    subtitleLabel.setVisible(! collapsed);
    modeLabelTitle.setVisible(! collapsed);
    normalModeButton.setVisible(! collapsed);
    learnModeButton.setVisible(! collapsed);
    researchModeButton.setVisible(! collapsed);
    accountLabel.setVisible(! collapsed);
    accountComboBox.setVisible(! collapsed);
    modelLabel.setVisible(! collapsed);
    modelComboBox.setVisible(! collapsed);
    accessLabelTitle.setVisible(! collapsed);
    accessComboBox.setVisible(! collapsed);
    promptLabel.setVisible(! collapsed);
    transcriptViewport.setVisible(! collapsed);
    promptEditor.setVisible(! collapsed);
    sendButton.setVisible(! collapsed);
    enterSendsToggle.setVisible(! collapsed);
    footerHintLabel.setVisible(! collapsed);

    repaint();
    resized();
}

void AiPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141820));
    g.setColour(juce::Colour(0xff263140));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 14.0f, 1.0f);
}

void AiPanel::setSendButtonIcon(station_ui::SendArrowButton::Icon icon, const juce::String& tooltip)
{
    sendButton.setIcon(icon);
    sendButton.setTooltip(tooltip);
}

void AiPanel::setEnterSendsMessage(bool shouldSend)
{
    promptEditor.enterSends = shouldSend;
    enterSendsToggle.setToggleState(shouldSend, juce::dontSendNotification);
}

void AiPanel::refreshPromptHeight()
{
    auto textHeight = promptEditor.getTextHeight();
    auto estimated = juce::jlimit(kMinPromptHeight, kMaxPromptHeight, textHeight + 12);
    if (promptEditorHeight != estimated)
    {
        promptEditorHeight = estimated;
        resized();
    }
}

void AiPanel::refreshChatLayout()
{
    if (transcriptContent == nullptr)
        return;

    auto width = transcriptViewport.getWidth();
    if (width <= 0)
        return;

    auto contentWidth = juce::jmax(120, width - 12);
    auto totalHeight = transcriptContent->getContentHeightForWidth(contentWidth);
    transcriptContent->setSize(contentWidth, totalHeight);

    if (transcriptViewport.getViewPositionY() + transcriptViewport.getHeight() >= transcriptContent->getHeight() - 24)
        scrollChatToBottom();
}

void AiPanel::scrollChatToBottom()
{
    if (transcriptContent == nullptr)
        return;

    auto viewHeight = transcriptViewport.getHeight();
    auto contentHeight = transcriptContent->getHeight();
    transcriptViewport.setViewPosition(0, juce::jmax(0, contentHeight - viewHeight));
}

void AiPanel::textEditorTextChanged(juce::TextEditor&)
{
    refreshPromptHeight();
}

void AiPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (updatingComboBoxes)
        return;

    if (comboBoxThatHasChanged == &modelComboBox)
    {
        if (onModelChanged)
            onModelChanged(modelComboBox.getText().trim());
    }
    else if (comboBoxThatHasChanged == &accountComboBox)
    {
        if (onAccountChanged)
            onAccountChanged(getSelectedAccountId());
    }
    else if (comboBoxThatHasChanged == &accessComboBox)
    {
        auto selectedId = accessComboBox.getSelectedId();
        auto level = AccessLevel::askFirst;
        switch (selectedId)
        {
            case 1: level = AccessLevel::askFirst; break;
            case 2: level = AccessLevel::appOnly; break;
            case 3: level = AccessLevel::fileChanges; break;
            case 4: level = AccessLevel::fullAccess; break;
            default: break;
        }

        setAccessLevel(level);
    }
}

void AiPanel::resized()
{
    auto area = getLocalBounds().reduced(18);

    if (collapsed)
    {
        collapseButton.setBounds(area.removeFromTop(34).removeFromLeft(84));
        return;
    }

    auto titleRow = area.removeFromTop(40);
    headerLabel.setBounds(titleRow.removeFromLeft(200));
    collapseButton.setBounds(titleRow.removeFromRight(90));
    subtitleLabel.setBounds(titleRow);

    area.removeFromTop(6);

    auto controlsRow = area.removeFromTop(48);
    auto modeWidth = juce::jmin(controlsRow.getWidth() / 2, 330);
    auto modeArea = controlsRow.removeFromLeft(modeWidth);
    modeLabelTitle.setBounds(modeArea.removeFromTop(16));
    auto modeButtons = modeArea.reduced(0, 2);
    normalModeButton.setBounds(modeButtons.removeFromLeft(100));
    modeButtons.removeFromLeft(8);
    learnModeButton.setBounds(modeButtons.removeFromLeft(100));
    modeButtons.removeFromLeft(8);
    researchModeButton.setBounds(modeButtons.removeFromLeft(120));

    auto accountArea = controlsRow.removeFromLeft(150);
    accountLabel.setBounds(accountArea.removeFromTop(16));
    accountComboBox.setBounds(accountArea.reduced(0, 2));
    controlsRow.removeFromLeft(10);

    auto modelArea = controlsRow.removeFromLeft(170);
    modelLabel.setBounds(modelArea.removeFromTop(16));
    modelComboBox.setBounds(modelArea.reduced(0, 2));
    controlsRow.removeFromLeft(10);
    auto accessArea = controlsRow.removeFromLeft(150);
    accessLabelTitle.setBounds(accessArea.removeFromTop(16));
    accessComboBox.setBounds(accessArea.reduced(0, 2));
    controlsRow.removeFromLeft(10);

    area.removeFromTop(4);
    promptLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    auto promptHeight = juce::jlimit(kMinPromptHeight, kMaxPromptHeight, promptEditorHeight > 0 ? promptEditorHeight : kMinPromptHeight);
    auto transcriptAreaHeight = juce::jmax(160, area.getHeight() - promptHeight - 44);
    auto transcriptArea = area.removeFromTop(transcriptAreaHeight);
    transcriptViewport.setBounds(transcriptArea);

    area.removeFromTop(6);

    auto promptArea = area.removeFromTop(promptHeight);
    promptEditor.setBounds(promptArea);
    // The send arrow sits inside the box, at the bottom right.
    sendButton.setBounds(promptArea.getRight() - 34, promptArea.getBottom() - 32, 28, 28);
    auto footerRow = area.removeFromTop(20);
    enterSendsToggle.setBounds(footerRow.removeFromRight(110));
    footerHintLabel.setBounds(footerRow);

    refreshChatLayout();
    scrollChatToBottom();
}
