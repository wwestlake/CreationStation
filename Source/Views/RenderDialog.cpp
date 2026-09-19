#include "RenderDialog.h"

namespace
{
const juce::Colour kLabel(0xff9aafc8);
const juce::Colour kText(0xffe6edf7);
const juce::Colour kWarning(0xffe0a35c);
const juce::Colour kError(0xffe0665f);

juce::String formatClock(double seconds)
{
    const auto total = (int) std::floor(juce::jmax(0.0, seconds));
    return juce::String(total / 60) + ":" + juce::String(total % 60).paddedLeft('0', 2);
}
}

RenderDialog::RenderDialog(const RenderRequest& initial, double projectLength, double deviceRate, bool projectHasMidiClips)
    : projectLengthSeconds(projectLength), deviceSampleRate(deviceRate), hasMidiClips(projectHasMidiClips)
{
    auto styleLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, kLabel);
        label.setFont(juce::Font(12.5f));
        addAndMakeVisible(label);
    };

    styleLabel(sourceLabel, "What to render");
    sourceValueLabel.setText("Master mix: every unmuted track, exactly as you hear it", juce::dontSendNotification);
    sourceValueLabel.setColour(juce::Label::textColourId, kText);
    sourceValueLabel.setFont(juce::Font(13.5f));
    addAndMakeVisible(sourceValueLabel);

    styleLabel(rangeLabel, "Range");
    rangeSelector.addItem("Entire project (0:00 to " + formatClock(projectLengthSeconds) + ")", 1);
    rangeSelector.addItem("Custom start and end", 2);
    rangeSelector.setSelectedId(initial.range == RenderRequest::Range::custom ? 2 : 1, juce::dontSendNotification);
    rangeSelector.onChange = [this] { updateEnabledState(); };
    addAndMakeVisible(rangeSelector);

    styleLabel(startLabel, "Start (seconds)");
    styleLabel(endLabel, "End (seconds)");
    styleLabel(tailLabel, "Tail (seconds)");
    for (auto* editor : { &startEditor, &endEditor, &tailEditor })
    {
        editor->setInputRestrictions(8, "0123456789.");
        addAndMakeVisible(*editor);
    }
    startEditor.setText(juce::String(initial.customStartSeconds, 2), juce::dontSendNotification);
    endEditor.setText(juce::String(initial.customEndSeconds > initial.customStartSeconds ? initial.customEndSeconds : projectLengthSeconds, 2),
                      juce::dontSendNotification);
    tailEditor.setText(juce::String(initial.tailSeconds, 1), juce::dontSendNotification);
    tailEditor.setTooltip("Extra time after the end so reverbs and delays can finish ringing out");

    styleLabel(destinationLabel, "Save to");
    destinationSelector.addItem("The project (as a render asset)", 1);
    destinationSelector.addItem("A file on disk", 2);
    destinationSelector.setSelectedId(initial.destination == RenderRequest::Destination::file ? 2 : 1, juce::dontSendNotification);
    destinationSelector.onChange = [this] { updateEnabledState(); };
    addAndMakeVisible(destinationSelector);

    styleLabel(nameLabel, "Name  ($project and $date are filled in)");
    nameEditor.setText(initial.name, juce::dontSendNotification);
    addAndMakeVisible(nameEditor);

    styleLabel(bitDepthLabel, "Bit depth");
    bitDepthSelector.addItem("16-bit", 16);
    bitDepthSelector.addItem("24-bit", 24);
    bitDepthSelector.addItem("32-bit float", 32);
    bitDepthSelector.setSelectedId(initial.bitsPerSample, juce::dontSendNotification);
    bitDepthSelector.onChange = [this] { updateEnabledState(); };
    addAndMakeVisible(bitDepthSelector);

    styleLabel(sampleRateLabel, "Sample rate");
    sampleRateSelector.addItem("Same as the audio device (" + juce::String((int) deviceSampleRate) + " Hz)", 1);
    sampleRateSelector.addItem("44100 Hz", 44100);
    sampleRateSelector.addItem("48000 Hz", 48000);
    sampleRateSelector.addItem("88200 Hz", 88200);
    sampleRateSelector.addItem("96000 Hz", 96000);
    sampleRateSelector.setSelectedId(initial.sampleRate <= 0.0 ? 1 : (int) std::lround(initial.sampleRate), juce::dontSendNotification);
    if (sampleRateSelector.getSelectedId() == 0)
        sampleRateSelector.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(sampleRateSelector);

    styleLabel(normalizeLabel, "Normalize");
    normalizeSelector.addItem("Off", 1);
    normalizeSelector.addItem("Peak to -1 dB", 2);
    normalizeSelector.addItem("Peak to -0.3 dB", 3);
    normalizeSelector.setSelectedId(initial.normalize == RenderRequest::Normalize::peakMinus1 ? 2
                                    : initial.normalize == RenderRequest::Normalize::peakMinus03 ? 3 : 1,
                                    juce::dontSendNotification);
    addAndMakeVisible(normalizeSelector);

    styleLabel(ditherLabel, "Dither");
    ditherSelector.addItem("Automatic (16-bit only)", 1);
    ditherSelector.addItem("Off", 2);
    ditherSelector.setSelectedId(initial.dither ? 1 : 2, juce::dontSendNotification);
    addAndMakeVisible(ditherSelector);

    placeOnTrackToggle.setToggleState(initial.placeOnNewTrack, juce::dontSendNotification);
    placeOnTrackToggle.setColour(juce::ToggleButton::textColourId, kText);
    addAndMakeVisible(placeOnTrackToggle);

    messageLabel.setFont(juce::Font(12.5f));
    messageLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(messageLabel);
    if (hasMidiClips)
        showMessage("This project has MIDI clips. MIDI instrument tracks are not included in a render yet, "
                    "so they will be silent in the result.", false);

    cancelButton.onClick = [this] { if (onCancel) onCancel(); };
    addAndMakeVisible(cancelButton);

    renderButton.onClick = [this]
    {
        RenderRequest request;
        if (readRequest(request) && onRender)
            onRender(request);
    };
    addAndMakeVisible(renderButton);

    updateEnabledState();
    setSize(500, 548);
}

void RenderDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10151d));
}

void RenderDialog::resized()
{
    auto area = getLocalBounds().reduced(18);

    auto rowWithLabel = [&area](juce::Label& label, juce::Component& control, int controlHeight = 26)
    {
        label.setBounds(area.removeFromTop(18));
        control.setBounds(area.removeFromTop(controlHeight));
        area.removeFromTop(10);
    };

    sourceLabel.setBounds(area.removeFromTop(18));
    sourceValueLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(10);

    rowWithLabel(rangeLabel, rangeSelector);

    {
        auto row = area.removeFromTop(18 + 26);
        const auto third = (row.getWidth() - 20) / 3;
        auto place = [&row, third](juce::Label& label, juce::TextEditor& editor)
        {
            auto column = row.removeFromLeft(third);
            label.setBounds(column.removeFromTop(18));
            editor.setBounds(column);
            row.removeFromLeft(10);
        };
        place(startLabel, startEditor);
        place(endLabel, endEditor);
        place(tailLabel, tailEditor);
        area.removeFromTop(10);
    }

    rowWithLabel(destinationLabel, destinationSelector);
    rowWithLabel(nameLabel, nameEditor);

    auto twoColumns = [&area](juce::Label& leftLabel, juce::Component& leftControl, juce::Label& rightLabel, juce::Component& rightControl)
    {
        auto row = area.removeFromTop(18 + 26);
        const auto half = (row.getWidth() - 10) / 2;
        auto left = row.removeFromLeft(half);
        row.removeFromLeft(10);
        leftLabel.setBounds(left.removeFromTop(18));
        leftControl.setBounds(left);
        rightLabel.setBounds(row.removeFromTop(18));
        rightControl.setBounds(row);
        area.removeFromTop(10);
    };
    twoColumns(bitDepthLabel, bitDepthSelector, sampleRateLabel, sampleRateSelector);
    twoColumns(normalizeLabel, normalizeSelector, ditherLabel, ditherSelector);

    placeOnTrackToggle.setBounds(area.removeFromTop(26));
    area.removeFromTop(6);

    auto buttons = area.removeFromBottom(34);
    renderButton.setBounds(buttons.removeFromRight(110));
    buttons.removeFromRight(10);
    cancelButton.setBounds(buttons.removeFromRight(90));

    messageLabel.setBounds(area);
}

void RenderDialog::showMessage(const juce::String& text, bool isError)
{
    messageLabel.setColour(juce::Label::textColourId, isError ? kError : kWarning);
    messageLabel.setText(text, juce::dontSendNotification);
}

void RenderDialog::updateEnabledState()
{
    const auto custom = rangeSelector.getSelectedId() == 2;
    startEditor.setEnabled(custom);
    endEditor.setEnabled(custom);

    const auto toProject = destinationSelector.getSelectedId() == 1;
    placeOnTrackToggle.setEnabled(toProject);
    if (! toProject)
        placeOnTrackToggle.setToggleState(false, juce::dontSendNotification);

    ditherSelector.setEnabled(bitDepthSelector.getSelectedId() == 16);
}

bool RenderDialog::readRequest(RenderRequest& request)
{
    request.range = rangeSelector.getSelectedId() == 2 ? RenderRequest::Range::custom : RenderRequest::Range::entireProject;
    request.customStartSeconds = startEditor.getText().getDoubleValue();
    request.customEndSeconds = endEditor.getText().getDoubleValue();
    request.tailSeconds = tailEditor.getText().getDoubleValue();
    request.destination = destinationSelector.getSelectedId() == 2 ? RenderRequest::Destination::file : RenderRequest::Destination::project;
    request.name = nameEditor.getText().trim();
    request.bitsPerSample = bitDepthSelector.getSelectedId();
    const auto rateId = sampleRateSelector.getSelectedId();
    request.sampleRate = rateId == 1 ? 0.0 : (double) rateId;
    request.normalize = normalizeSelector.getSelectedId() == 2 ? RenderRequest::Normalize::peakMinus1
                      : normalizeSelector.getSelectedId() == 3 ? RenderRequest::Normalize::peakMinus03
                                                               : RenderRequest::Normalize::off;
    request.dither = ditherSelector.getSelectedId() == 1;
    request.placeOnNewTrack = placeOnTrackToggle.getToggleState() && placeOnTrackToggle.isEnabled();

    if (request.name.isEmpty())
    {
        showMessage("Enter a name for the render.", true);
        nameEditor.grabKeyboardFocus();
        return false;
    }

    if (request.range == RenderRequest::Range::custom)
    {
        if (request.customStartSeconds < 0.0 || request.customEndSeconds <= request.customStartSeconds)
        {
            showMessage("The end has to come after the start.", true);
            endEditor.grabKeyboardFocus();
            return false;
        }
    }

    if (request.tailSeconds < 0.0 || request.tailSeconds > 120.0)
    {
        showMessage("Tail can be from 0 to 120 seconds.", true);
        tailEditor.grabKeyboardFocus();
        return false;
    }

    return true;
}
