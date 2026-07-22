#include "AiPanel.h"

namespace
{
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
        bool isUserMessage() const noexcept { return isUser; }

        void setMessage(bool user, const juce::String& badge, const juce::String& content)
        {
            isUser = user;
            badgeText = badge;
            bodyText = content;
            repaint();
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

            auto textArea = bounds.reduced(14.0f, 10.0f);
            juce::TextLayout layout;
            layout.createLayout(buildMarkdownAttributedString(badgeText, bodyText, isUser), textArea.getWidth());
            layout.draw(g, textArea);
        }

    private:
        bool isUser = false;
        juce::String badgeText;
        juce::String bodyText;
    };

} // namespace

class ChatTranscriptComponent final : public juce::Component
{
public:
    int addMessage(bool user, const juce::String& badge, const juce::String& content)
    {
        auto* bubble = new ChatBubbleComponent();
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

    providerLabel.setText("Provider", juce::dontSendNotification);
    providerLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    addAndMakeVisible(providerLabel);

    providerComboBox.addItem("OpenAI", 1);
    providerComboBox.addItem("Ollama", 2);
    providerComboBox.addListener(this);
    addAndMakeVisible(providerComboBox);

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
    promptEditor.setText("Describe the sound or change you want.");
    promptEditor.addListener(this);
    addAndMakeVisible(promptEditor);

    sendButton.onClick = [this]
    {
        auto prompt = getPromptText().trim();
        if (prompt.isEmpty())
            return;

        auto submittedPrompt = buildSubmissionPrompt();

        appendUserMessage(prompt);
        pendingAssistantBubbleIndex = transcriptContent->addMessage(false, "Virtual Engineer", "Thinking...");
        refreshChatLayout();
        scrollChatToBottom();

        promptEditor.clear();
        refreshPromptHeight();

        if (onPromptSubmitted)
            onPromptSubmitted(submittedPrompt);
    };
    addAndMakeVisible(sendButton);

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

void AiPanel::setSelectedProvider(const juce::String& providerName)
{
    updatingComboBoxes = true;
    auto isOllama = providerName.toLowerCase().contains("ollama");
    providerComboBox.setSelectedId(isOllama ? 2 : 1, juce::dontSendNotification);
    updatingComboBoxes = false;
}

juce::String AiPanel::getSelectedProvider() const
{
    auto providerName = providerComboBox.getText().trim();
    return providerName.isNotEmpty() ? providerName : "OpenAI";
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
    providerLabel.setVisible(! collapsed);
    providerComboBox.setVisible(! collapsed);
    modelLabel.setVisible(! collapsed);
    modelComboBox.setVisible(! collapsed);
    accessLabelTitle.setVisible(! collapsed);
    accessComboBox.setVisible(! collapsed);
    promptLabel.setVisible(! collapsed);
    transcriptViewport.setVisible(! collapsed);
    promptEditor.setVisible(! collapsed);
    sendButton.setVisible(! collapsed);
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

void AiPanel::refreshPromptHeight()
{
    auto textHeight = promptEditor.getTextHeight();
    auto estimated = juce::jlimit(72, 220, textHeight + 26);
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
    else if (comboBoxThatHasChanged == &providerComboBox)
    {
        if (onProviderChanged)
            onProviderChanged(getSelectedProvider());
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

    auto providerArea = controlsRow.removeFromLeft(110);
    providerLabel.setBounds(providerArea.removeFromTop(16));
    providerComboBox.setBounds(providerArea.reduced(0, 2));
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

    auto promptHeight = juce::jlimit(72, 220, promptEditorHeight > 0 ? promptEditorHeight : 96);
    auto transcriptAreaHeight = juce::jmax(160, area.getHeight() - promptHeight - 44);
    auto transcriptArea = area.removeFromTop(transcriptAreaHeight);
    transcriptViewport.setBounds(transcriptArea);

    area.removeFromTop(6);

    auto promptArea = area.removeFromTop(promptHeight);
    auto promptButtonArea = promptArea.removeFromRight(100);
    sendButton.setBounds(promptButtonArea.reduced(0, 2));
    promptEditor.setBounds(promptArea.reduced(0, 0));
    footerHintLabel.setBounds(area);

    refreshChatLayout();
    scrollChatToBottom();
}
