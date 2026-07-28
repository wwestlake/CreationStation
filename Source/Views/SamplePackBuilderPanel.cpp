#include "SamplePackBuilderPanel.h"

SamplePackBuilderPanel::SamplePackBuilderPanel()
{
    titleLabel.setText("Sample Pack Builder", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8f0f7));
    addAndMakeVisible(titleLabel);

    inputLabel.setText("Raw captures folder", juce::dontSendNotification);
    inputLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(inputLabel);

    inputPathEditor.setReadOnly(true);
    inputPathEditor.setTextToShowWhenEmpty("Choose a folder of scattered single-note recordings...",
                                           juce::Colour(0xff6a7890));
    addAndMakeVisible(inputPathEditor);

    chooseInputButton.onClick = [this] { chooseInputFolder(); };
    addAndMakeVisible(chooseInputButton);

    outputLabel.setText("Export folder", juce::dontSendNotification);
    outputLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(outputLabel);

    outputPathEditor.setReadOnly(true);
    outputPathEditor.setTextToShowWhenEmpty("Choose where the sample pack should be exported...",
                                            juce::Colour(0xff6a7890));
    addAndMakeVisible(outputPathEditor);

    chooseOutputButton.onClick = [this] { chooseOutputFolder(); };
    addAndMakeVisible(chooseOutputButton);

    buildButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e3629));
    buildButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff67e8a5));
    buildButton.onClick = [this] { startBuild(); };
    addAndMakeVisible(buildButton);

    logEditor.setMultiLine(true);
    logEditor.setReadOnly(true);
    logEditor.setScrollbarsShown(true);
    logEditor.setCaretVisible(false);
    logEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1218));
    logEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffc9d6e6));
    addAndMakeVisible(logEditor);

    setSize(640, 520);
}

SamplePackBuilderPanel::~SamplePackBuilderPanel()
{
    // The build runs on a detached background thread that only ever touches this component
    // through a SafePointer-guarded callAsync - safe to let it finish on its own rather than
    // blocking the UI thread here to join it.
    if (buildThread.joinable())
        buildThread.detach();
}

void SamplePackBuilderPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151c28));
}

void SamplePackBuilderPanel::resized()
{
    auto area = getLocalBounds().reduced(16);

    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(12);

    inputLabel.setBounds(area.removeFromTop(18));
    auto inputRow = area.removeFromTop(28);
    chooseInputButton.setBounds(inputRow.removeFromRight(90));
    inputRow.removeFromRight(8);
    inputPathEditor.setBounds(inputRow);
    area.removeFromTop(10);

    outputLabel.setBounds(area.removeFromTop(18));
    auto outputRow = area.removeFromTop(28);
    chooseOutputButton.setBounds(outputRow.removeFromRight(90));
    outputRow.removeFromRight(8);
    outputPathEditor.setBounds(outputRow);
    area.removeFromTop(14);

    buildButton.setBounds(area.removeFromTop(34).removeFromLeft(200));
    area.removeFromTop(12);

    logEditor.setBounds(area);
}

void SamplePackBuilderPanel::chooseInputFolder()
{
    activeChooser = std::make_unique<juce::FileChooser>("Choose a folder of raw single-note captures",
                                                        juce::File::getSpecialLocation(juce::File::userMusicDirectory));

    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                               [this](const juce::FileChooser& chooser)
                               {
                                   auto folder = chooser.getResult();
                                   activeChooser.reset();

                                   if (folder.isDirectory())
                                       inputPathEditor.setText(folder.getFullPathName(), juce::dontSendNotification);
                               });
}

void SamplePackBuilderPanel::chooseOutputFolder()
{
    activeChooser = std::make_unique<juce::FileChooser>("Choose where to export the sample pack",
                                                        juce::File::getSpecialLocation(juce::File::userMusicDirectory));

    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                               [this](const juce::FileChooser& chooser)
                               {
                                   auto folder = chooser.getResult();
                                   activeChooser.reset();

                                   if (folder.isDirectory())
                                       outputPathEditor.setText(folder.getFullPathName(), juce::dontSendNotification);
                               });
}

void SamplePackBuilderPanel::setControlsEnabled(bool enabled)
{
    chooseInputButton.setEnabled(enabled);
    chooseOutputButton.setEnabled(enabled);
    buildButton.setEnabled(enabled);
}

void SamplePackBuilderPanel::appendLogLine(const juce::String& line)
{
    logEditor.moveCaretToEnd();
    logEditor.insertTextAtCaret(line + "\n");
}

void SamplePackBuilderPanel::startBuild()
{
    if (buildInProgress)
        return;

    auto inputFolder = juce::File(inputPathEditor.getText());
    auto outputFolder = juce::File(outputPathEditor.getText());

    if (! inputFolder.isDirectory())
    {
        appendLogLine("Choose a valid input folder first.");
        return;
    }

    if (outputPathEditor.getText().isEmpty())
    {
        appendLogLine("Choose an export folder first.");
        return;
    }

    if (buildThread.joinable())
        buildThread.detach();

    logEditor.clear();
    appendLogLine("Building sample pack from " + inputFolder.getFullPathName() + " ...");
    buildInProgress = true;
    setControlsEnabled(false);

    juce::Component::SafePointer<SamplePackBuilderPanel> safeThis(this);

    buildThread = std::thread([safeThis, inputFolder, outputFolder]
    {
        auto progress = [safeThis](const juce::String& line)
        {
            juce::MessageManager::callAsync([safeThis, line]
            {
                if (safeThis != nullptr)
                    safeThis->appendLogLine(line);
            });
        };

        auto result = SamplePackBuilderEngine::build(inputFolder, outputFolder, progress);

        juce::MessageManager::callAsync([safeThis, result]
        {
            if (safeThis != nullptr)
                safeThis->onBuildFinished(result);
        });
    });
}

void SamplePackBuilderPanel::onBuildFinished(SamplePackBuilderEngine::BuildResult result)
{
    buildInProgress = false;
    setControlsEnabled(true);

    if (! result.success)
    {
        appendLogLine("Build failed: " + result.errorMessage);
        return;
    }

    auto capturedCount = 0;
    auto gapFilledCount = 0;
    for (int note = 0; note < 128; ++note)
    {
        if (result.notesCaptured[(size_t) note])
            ++capturedCount;
        else if (result.notesExported[(size_t) note])
            ++gapFilledCount;
    }

    appendLogLine("");
    appendLogLine("Done: " + juce::String(capturedCount) + " notes captured directly, "
                   + juce::String(gapFilledCount) + " notes gap-filled, "
                   + juce::String(capturedCount + gapFilledCount) + "/128 exported.");
}
