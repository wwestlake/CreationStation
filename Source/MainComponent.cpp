#include <creation/assets/ProjectManifest.h>
#include <creation/assets/AssetTypes.h>
#include "MainComponent.h"
#include "Branding.h"
#include "Patch/PatchModel.h"
#include <creation/assets/ProjectContainerService.h>
#include <creation/assets/ProjectWorkspaceService.h>
#include <creation/ui/CreationSuiteLogos.h>
#include <creation/services/SuiteAiProviderRuntime.h>
#include "Tutorial/TutorialScriptCompiler.h"
#include <creation/services/SuiteAiSettings.h>
#include <creation/ui/ControlSurfaceActionIds.h>
#include <creation/ui/CreationSuiteLogos.h>
#include <thread>

namespace
{
juce::Array<creation::assets::AssetDescriptor> filterFoleyAudioAssets(const juce::Array<creation::assets::AssetDescriptor>& projectAssets)
{
    juce::Array<creation::assets::AssetDescriptor> foleyAssets;

    for (const auto& asset : projectAssets)
    {
        if (asset.kind == creation::assets::AssetKind::audio)
            foleyAssets.add(asset);
    }

    return foleyAssets;
}

juce::String trimProjectLabelPrefix(const juce::String& label)
{
    constexpr auto prefix = "Project:";
    auto trimmed = label.trim();
    if (trimmed.startsWithIgnoreCase(prefix))
        trimmed = trimmed.fromFirstOccurrenceOf(prefix, false, false).trim();
    return trimmed;
}

juce::String makeDisplayProjectLabel(const juce::String& rawName)
{
    auto trimmed = trimProjectLabelPrefix(rawName);
    return trimmed.isNotEmpty() ? trimmed : "No project open";
}

void drawHeaderActionIcon(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          const juce::String& iconName,
                          juce::Colour colour)
{
    auto centre = bounds.getCentre();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    g.setColour(colour);

    if (iconName == "plus-circle" || iconName == "plus")
    {
        auto circleBounds = juce::Rectangle<float>(centre.x - size * 0.42f, centre.y - size * 0.42f, size * 0.84f, size * 0.84f);
        g.setColour(colour.withAlpha(0.20f));
        g.fillEllipse(circleBounds);
        g.setColour(colour);
        g.drawEllipse(circleBounds, 2.0f);

        auto armLength = size * 0.22f;
        g.drawLine(centre.x - armLength, centre.y, centre.x + armLength, centre.y, 2.4f);
        g.drawLine(centre.x, centre.y - armLength, centre.x, centre.y + armLength, 2.4f);
        return;
    }

    if (iconName == "folder")
    {
        juce::Path folder;
        folder.startNewSubPath(bounds.getX() + size * 0.10f, bounds.getY() + size * 0.34f);
        folder.lineTo(bounds.getX() + size * 0.30f, bounds.getY() + size * 0.34f);
        folder.lineTo(bounds.getX() + size * 0.40f, bounds.getY() + size * 0.18f);
        folder.lineTo(bounds.getRight() - size * 0.08f, bounds.getY() + size * 0.18f);
        folder.lineTo(bounds.getRight() - size * 0.08f, bounds.getBottom() - size * 0.14f);
        folder.lineTo(bounds.getX() + size * 0.10f, bounds.getBottom() - size * 0.14f);
        folder.closeSubPath();
        g.strokePath(folder, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    if (iconName == "speaker")
    {
        juce::Path speaker;
        speaker.startNewSubPath(bounds.getX() + size * 0.12f, centre.y - size * 0.14f);
        speaker.lineTo(bounds.getX() + size * 0.28f, centre.y - size * 0.14f);
        speaker.lineTo(bounds.getX() + size * 0.44f, centre.y - size * 0.32f);
        speaker.lineTo(bounds.getX() + size * 0.44f, centre.y + size * 0.32f);
        speaker.lineTo(bounds.getX() + size * 0.28f, centre.y + size * 0.14f);
        speaker.lineTo(bounds.getX() + size * 0.12f, centre.y + size * 0.14f);
        speaker.closeSubPath();
        g.strokePath(speaker, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (auto radiusScale : { 0.20f, 0.34f })
        {
            juce::Path wave;
            auto radius = size * radiusScale;
            wave.addCentredArc(bounds.getX() + size * 0.46f,
                               centre.y,
                               radius,
                               radius,
                               0.0f,
                               -juce::MathConstants<float>::pi * 0.35f,
                               juce::MathConstants<float>::pi * 0.35f,
                               true);
            g.strokePath(wave, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        return;
    }

    if (iconName == "gear")
    {
        const auto radius = size * 0.24f;
        juce::Path gearPath;
        const int numTeeth = 8;
        for (int i = 0; i < numTeeth; ++i)
        {
            const float angle = i * juce::MathConstants<float>::twoPi / numTeeth;
            const float outerR = radius * 1.35f;
            const float innerR = radius * 0.88f;

            juce::Path tooth;
            tooth.addRectangle(-radius * 0.16f, -outerR, radius * 0.32f, outerR - innerR);
            juce::AffineTransform transform = juce::AffineTransform::rotation(angle).translated(centre.x, centre.y);
            gearPath.addPath(tooth, transform);
        }

        gearPath.addEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.fillPath(gearPath);

        juce::Path holePath;
        holePath.addEllipse(centre.x - radius * 0.40f, centre.y - radius * 0.40f, radius * 0.80f, radius * 0.80f);
        g.setColour(juce::Colour(0xff17222c));
        g.fillPath(holePath);
        return;
    }

    if (iconName == "chevron-down")
    {
        juce::Path chevron;
        chevron.startNewSubPath(centre.x - size * 0.20f, centre.y - size * 0.08f);
        chevron.lineTo(centre.x, centre.y + size * 0.12f);
        chevron.lineTo(centre.x + size * 0.20f, centre.y - size * 0.08f);
        g.strokePath(chevron, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
}

AiProviderSettings makeAiProviderSettings(const creation::services::SuiteAiResolvedRuntimeSettings& runtimeSettings)
{
    AiProviderSettings settings;
    settings.providerDisplayName = runtimeSettings.providerDisplayName.isNotEmpty()
                                       ? runtimeSettings.providerDisplayName
                                       : creation::services::SuiteAiProviderRuntime::resolveProfile(runtimeSettings.providerId).displayName;
    settings.providerId = runtimeSettings.providerId;
    settings.baseUrl = runtimeSettings.baseUrl;
    settings.modelName = runtimeSettings.modelName;
    settings.apiKey = runtimeSettings.apiKey;
    return settings;
}

CreationSuiteHeaderBar::ProfileData makeHeaderProfile(const DesktopAuthSession::SessionData& session)
{
    CreationSuiteHeaderBar::ProfileData profile;
    profile.displayName = session.user.displayName.isNotEmpty() ? session.user.displayName : session.user.email;

    auto tierId = branding::getBestPatreonTierId(session.user.entitlements);
    auto tierName = branding::getPatreonTierDisplayName(tierId);
    profile.detailText = tierName.isNotEmpty() ? session.user.email + "  |  " + tierName
                                               : session.user.email;
    profile.badgeImage = branding::createPatreonBadgeImage(tierId, 36);
    return profile;
}

class ManagedDocumentWindow final : public juce::DocumentWindow
{
public:
    ManagedDocumentWindow(const juce::String& title,
                          juce::Colour backgroundColour,
                          int requiredButtons,
                          std::function<void()> onCloseCallback)
        : juce::DocumentWindow(title, backgroundColour, requiredButtons),
          onClose(std::move(onCloseCallback))
    {
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        auto closeCallback = onClose;
        juce::MessageManager::callAsync([closeCallback]() mutable
        {
            if (closeCallback)
                closeCallback();
        });
    }

private:
    std::function<void()> onClose;
};

// Right-click a transport button -> Learn MIDI Binding. Opens armed by default (Any Device) so
// the common case is just "wiggle the hardware and it's bound" - manual entry is the fallback.
class MidiLearnPanel final : public juce::Component,
                             private juce::Timer
{
public:
    struct ExistingBinding
    {
        bool found = false;
        juce::String deviceLabel;
        int channel = 0;
        int number = 0;
        bool isController = false;
    };

    MidiLearnPanel(WorkstationAudioEngine& engineRef, juce::String targetIdIn, const juce::String& displayLabel,
                   const ExistingBinding& existing)
        : engine(engineRef), targetId(std::move(targetIdIn))
    {
        titleLabel.setText("Learn MIDI Binding: " + displayLabel, juce::dontSendNotification);
        titleLabel.setFont(juce::Font(16.0f).boldened());
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        instructionsLabel.setText(
            "Move a knob or press a button on your controller now - it will be captured "
            "automatically and saved right away. Or, if you already know the values, type them "
            "in below and click \"Save Typed Values\" instead.",
            juce::dontSendNotification);
        instructionsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        instructionsLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(instructionsLabel);

        if (existing.found)
        {
            currentBindingLabel.setText("Currently bound to: " + existing.deviceLabel + ", channel "
                                        + juce::String(existing.channel) + ", "
                                        + (existing.isController ? "CC " : "Note ") + juce::String(existing.number),
                                        juce::dontSendNotification);
            currentBindingLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
            addAndMakeVisible(currentBindingLabel);
        }

        deviceLabel.setText("Listen to", juce::dontSendNotification);
        deviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        addAndMakeVisible(deviceLabel);

        deviceCombo.addItem("Any Device", 1);
        auto itemId = 2;
        for (const auto& device : juce::MidiInput::getAvailableDevices())
        {
            deviceCombo.addItem(device.name, itemId);
            deviceIdsByItemId[itemId] = device.identifier;
            ++itemId;
        }
        deviceCombo.setSelectedId(1, juce::dontSendNotification);
        deviceCombo.onChange = [this] { armLearn(); };
        deviceCombo.setTooltip("Restrict listening to one device, or Any Device to accept from all");
        addAndMakeVisible(deviceCombo);

        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5f93ff));
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(statusLabel);

        manualHintLabel.setText("Manual entry (only needed if auto-capture above doesn't work):",
                                juce::dontSendNotification);
        manualHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff71839b));
        addAndMakeVisible(manualHintLabel);

        channelLabel.setText("Channel (1-16)", juce::dontSendNotification);
        channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        addAndMakeVisible(channelLabel);
        channelEditor.setInputRestrictions(2, "0123456789");
        addAndMakeVisible(channelEditor);

        numberLabel.setText("Note/CC # (0-127)", juce::dontSendNotification);
        numberLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        addAndMakeVisible(numberLabel);
        numberEditor.setInputRestrictions(3, "0123456789");
        addAndMakeVisible(numberEditor);

        if (existing.found)
        {
            channelEditor.setText(juce::String(existing.channel), juce::dontSendNotification);
            numberEditor.setText(juce::String(existing.number), juce::dontSendNotification);
        }

        isCCToggle.setButtonText("The number above is a CC (Control Change), not a Note");
        isCCToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaebbd0));
        isCCToggle.setToggleState(existing.isController, juce::dontSendNotification);
        isCCToggle.setTooltip("Check this if the number above is a CC (Control Change) number, not a Note number");
        addAndMakeVisible(isCCToggle);

        applyManualButton.setButtonText("Save Typed Values");
        applyManualButton.onClick = [this] { applyManualEntry(); };
        applyManualButton.setTooltip("Save the channel/number typed in above as the binding");
        addAndMakeVisible(applyManualButton);

        closeButton.setButtonText("Close");
        closeButton.onClick = [this] { if (onCancelled) onCancelled(); };
        closeButton.setTooltip("Close this dialog");
        addAndMakeVisible(closeButton);

        armLearn();
        startTimer(60);
    }

    ~MidiLearnPanel() override
    {
        engine.cancelMidiLearn();
    }

    // Fired every time something is captured/saved - the panel stays open afterward so the user
    // can see what was captured and try again if it's wrong. Only the Close button (or the
    // window's own close control) actually dismisses the dialog.
    std::function<void(juce::String deviceId, int channel, int number, bool isCC)> onLearned;
    std::function<void()> onCancelled;

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        titleLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(6);

        instructionsLabel.setBounds(area.removeFromTop(56));
        area.removeFromTop(8);

        if (currentBindingLabel.isVisible())
        {
            currentBindingLabel.setBounds(area.removeFromTop(20));
            area.removeFromTop(8);
        }

        auto deviceRow = area.removeFromTop(28);
        deviceLabel.setBounds(deviceRow.removeFromLeft(80));
        deviceCombo.setBounds(deviceRow);
        area.removeFromTop(8);

        statusLabel.setBounds(area.removeFromTop(36));
        area.removeFromTop(12);

        manualHintLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(6);

        auto manualRow = area.removeFromTop(28);
        channelLabel.setBounds(manualRow.removeFromLeft(90));
        channelEditor.setBounds(manualRow.removeFromLeft(50));
        manualRow.removeFromLeft(10);
        numberLabel.setBounds(manualRow.removeFromLeft(110));
        numberEditor.setBounds(manualRow.removeFromLeft(50));
        area.removeFromTop(8);

        isCCToggle.setBounds(area.removeFromTop(24));
        area.removeFromTop(10);

        auto buttonRow = area.removeFromTop(32);
        applyManualButton.setBounds(buttonRow.removeFromLeft(160));
        buttonRow.removeFromLeft(10);
        closeButton.setBounds(buttonRow.removeFromLeft(100));
    }

private:
    void armLearn()
    {
        auto selectedId = deviceCombo.getSelectedId();
        juce::String deviceIdFilter;
        if (selectedId > 1)
        {
            auto it = deviceIdsByItemId.find(selectedId);
            if (it != deviceIdsByItemId.end())
                deviceIdFilter = it->second;
        }
        engine.armMidiLearn(deviceIdFilter);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5f93ff));
        statusLabel.setText("Listening - move or press the control now...", juce::dontSendNotification);
    }

    void applyManualEntry()
    {
        auto channel = channelEditor.getText().getIntValue();
        auto number = numberEditor.getText().getIntValue();
        if (channel < 1 || channel > 16 || number < 0 || number > 127)
        {
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb0a8));
            statusLabel.setText("Enter a channel 1-16 and a number 0-127.", juce::dontSendNotification);
            return;
        }

        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7fe8a0));
        statusLabel.setText("Saved: channel " + juce::String(channel) + ", "
                            + (isCCToggle.getToggleState() ? "CC " : "Note ") + juce::String(number),
                            juce::dontSendNotification);

        if (onLearned)
            onLearned({}, channel, number, isCCToggle.getToggleState());
    }

    void timerCallback() override
    {
        WorkstationAudioEngine::MidiLearnResult result;
        if (engine.takeMidiLearnResult(result))
        {
            channelEditor.setText(juce::String(result.channel), juce::dontSendNotification);
            numberEditor.setText(juce::String(result.number), juce::dontSendNotification);
            isCCToggle.setToggleState(result.isController, juce::dontSendNotification);

            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7fe8a0));
            statusLabel.setText("Captured & saved: channel " + juce::String(result.channel) + ", "
                                + (result.isController ? "CC " : "Note ") + juce::String(result.number),
                                juce::dontSendNotification);

            if (onLearned)
                onLearned(result.deviceId, result.channel, result.number, result.isController);
        }
    }

    WorkstationAudioEngine& engine;
    juce::String targetId;
    std::map<int, juce::String> deviceIdsByItemId;

    juce::Label titleLabel, instructionsLabel, currentBindingLabel, deviceLabel, statusLabel,
               manualHintLabel, channelLabel, numberLabel;
    juce::ComboBox deviceCombo;
    juce::TextEditor channelEditor, numberEditor;
    juce::ToggleButton isCCToggle;
    juce::TextButton applyManualButton, closeButton;
};

bool writeWavFile(const juce::File& destination,
                  const juce::AudioBuffer<float>& buffer,
                  double sampleRate,
                  juce::String& errorMessage)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        errorMessage = "There is no audio to export.";
        return false;
    }

    destination.getParentDirectory().createDirectory();

    juce::WavAudioFormat wavFormat;
    auto outputStream = std::unique_ptr<juce::FileOutputStream>(destination.createOutputStream());
    if (outputStream == nullptr)
    {
        errorMessage = "Could not open the export file for writing.";
        return false;
    }

    auto writer = std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(outputStream.get(),
                                                                                      sampleRate,
                                                                                      (unsigned int) buffer.getNumChannels(),
                                                                                      24,
                                                                                      {},
                                                                                      0));
    if (writer == nullptr)
    {
        errorMessage = "Could not create a WAV writer for this export.";
        return false;
    }

    outputStream.release();
    if (! writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        errorMessage = "Could not write the WAV export.";
        return false;
    }

    return true;
}

juce::String workspaceModeName(MainComponent::WorkspaceMode mode)
{
    switch (mode)
    {
        case MainComponent::WorkspaceMode::tracker: return "Tracker";
        case MainComponent::WorkspaceMode::arrange: return "Foley";
        case MainComponent::WorkspaceMode::signal: return "Signal";
        case MainComponent::WorkspaceMode::library: return "Library";
        case MainComponent::WorkspaceMode::mix: return "Layers";
        case MainComponent::WorkspaceMode::plugins: return "Plugins";
        case MainComponent::WorkspaceMode::node: return "Patch";
        case MainComponent::WorkspaceMode::code: return "Script";
        case MainComponent::WorkspaceMode::record: return "Capture";
        case MainComponent::WorkspaceMode::score: return "Score";
        case MainComponent::WorkspaceMode::settings: return "Settings";
        case MainComponent::WorkspaceMode::sampler: return "Sampler";
    }

    return "Tracker";
}

constexpr int workspaceModeCount = 12;

int workspaceModeIndex(MainComponent::WorkspaceMode mode)
{
    return juce::jlimit(0, workspaceModeCount - 1, static_cast<int>(mode));
}

juce::String makeRecordingTimestamp()
{
    return juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
}

void addTextEntry(juce::ZipFile::Builder& builder, const juce::String& path, const juce::String& text)
{
    auto normalizedPath = path.replaceCharacter('\\', '/').trimCharactersAtStart("/");
    builder.addEntry(new juce::MemoryInputStream(text.toRawUTF8(), text.getNumBytesAsUTF8(), true),
                     9,
                     normalizedPath,
                     juce::Time::getCurrentTime());
}

double midiToFrequency(int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

juce::AudioBuffer<float> renderScorePreviewBuffer(const ScorePanel::PlaybackRequest& request, double sampleRate)
{
    auto sortedNotes = request.notes;
    std::sort(sortedNotes.begin(), sortedNotes.end(), [](const ScorePanel::NoteEvent& a, const ScorePanel::NoteEvent& b)
    {
        if (a.measure != b.measure)
            return a.measure < b.measure;

        if (a.beat != b.beat)
            return a.beat < b.beat;

        return a.midiNote < b.midiNote;
    });

    auto bpm = juce::jmax(40, request.tempoBpm);
    auto secondsPerBeat = 60.0 / static_cast<double>(bpm);
    auto totalDurationSeconds = 1.0;

    for (const auto& note : sortedNotes)
    {
        if (note.isRest)
            continue;

        auto startBeat = static_cast<double>(note.measure * 4) + static_cast<double>(note.beat - 1.0f);
        auto noteEnd = startBeat + juce::jmax(0.25f, note.durationBeats);
        totalDurationSeconds = juce::jmax(totalDurationSeconds, (noteEnd * secondsPerBeat) + 0.4);
    }

    auto totalSamples = juce::jmax(1, juce::roundToInt(totalDurationSeconds * sampleRate));
    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();

    for (const auto& note : sortedNotes)
    {
        if (note.isRest)
            continue;

        auto startBeat = static_cast<double>(note.measure * 4) + static_cast<double>(note.beat - 1.0f);
        auto startSeconds = startBeat * secondsPerBeat;
        auto noteDurationSeconds = secondsPerBeat * juce::jmax(0.20f, note.durationBeats * 0.92f);
        auto attackSeconds = 0.01;
        auto releaseSeconds = juce::jmin(0.12, noteDurationSeconds * 0.35);
        auto sustainSeconds = juce::jmax(0.02, noteDurationSeconds - attackSeconds - releaseSeconds);
        auto frequency = midiToFrequency(note.midiNote);

        auto startSample = juce::jlimit(0, totalSamples - 1, juce::roundToInt(startSeconds * sampleRate));
        auto noteSamples = juce::jmax(1, juce::roundToInt(noteDurationSeconds * sampleRate));
        auto attackSamples = juce::jmax(1, juce::roundToInt(attackSeconds * sampleRate));
        auto sustainSamples = juce::jmax(1, juce::roundToInt(sustainSeconds * sampleRate));
        auto releaseSamples = juce::jmax(1, noteSamples - attackSamples - sustainSamples);
        auto phase = 0.0;
        auto phaseDelta = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        for (int sample = 0; sample < noteSamples && (startSample + sample) < totalSamples; ++sample)
        {
            auto envelope = 1.0f;

            if (sample < attackSamples)
                envelope = static_cast<float>(sample) / static_cast<float>(attackSamples);
            else if (sample >= attackSamples + sustainSamples)
                envelope = 1.0f - (static_cast<float>(sample - attackSamples - sustainSamples) / static_cast<float>(juce::jmax(1, releaseSamples)));

            envelope = juce::jlimit(0.0f, 1.0f, envelope);

            auto body = 0.70 * std::sin(phase);
            auto shimmer = 0.20 * std::sin(phase * 2.0);
            auto air = 0.10 * std::sin(phase * 3.0);
            auto sampleValue = static_cast<float>((body + shimmer + air) * 0.18 * envelope);
            auto targetSample = startSample + sample;

            buffer.addSample(0, targetSample, sampleValue);
            buffer.addSample(1, targetSample, sampleValue);
            phase += phaseDelta;
        }
    }

    buffer.applyGain(0.8f);
    return buffer;
}

bool isAdminRole(const juce::String& role)
{
    auto normalized = role.trim().toLowerCase();
    return normalized == "admin" || normalized == "administrator";
}
}

MainComponent::ViewModeBar::ViewModeBar()
{
    titleLabel.setText("Creative Modes", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    auto setupButton = [this](juce::TextButton& button, WorkspaceMode mode)
    {
        button.setClickingTogglesState(true);
        button.onClick = [this, mode]
        {
            setActiveMode(mode);
            if (onModeSelected)
                onModeSelected(mode);
        };
        addAndMakeVisible(button);
    };

    setupButton(trackerButton, WorkspaceMode::tracker);
    setupButton(samplerButton, WorkspaceMode::sampler);
    setupButton(arrangeButton, WorkspaceMode::arrange);
    setupButton(signalButton, WorkspaceMode::signal);
    setupButton(libraryButton, WorkspaceMode::library);
    setupButton(mixButton, WorkspaceMode::mix);
    setupButton(pluginsButton, WorkspaceMode::plugins);
    setupButton(nodeButton, WorkspaceMode::node);
    setupButton(codeButton, WorkspaceMode::code);
    setupButton(recordButton, WorkspaceMode::record);
    setupButton(scoreButton, WorkspaceMode::score);
    setupButton(settingsButton, WorkspaceMode::settings);

    trackerButton.setTooltip("Tracker - arrange and edit tracks");
    samplerButton.setTooltip("Sampler - build and manage pitch-mapped sample packs");
    arrangeButton.setTooltip("Foley - arrange sound effects and foley clips");
    signalButton.setTooltip("Signal Lab - sound design and synthesis");
    libraryButton.setTooltip("Content library - browse and manage assets");
    mixButton.setTooltip("Mixer - adjust levels, pan, and sends");
    pluginsButton.setTooltip("Plugin browser - find and load VST plugins");
    nodeButton.setTooltip("Node graph - patch signal routing visually");
    codeButton.setTooltip("CEL script editor - write and validate CEL patches");
    recordButton.setTooltip("Recording workspace - capture new takes");
    scoreButton.setTooltip("Score view - notation and piano roll editing");
    settingsButton.setTooltip("App settings - project, audio, MIDI, and AI configuration");
    popOutButton.setTooltip("Pop the current workspace out into its own window");

    popOutButton.onClick = [this]
    {
        if (onPopOutRequested)
            onPopOutRequested();
    };
    addAndMakeVisible(popOutButton);

    setActiveMode(WorkspaceMode::tracker);
}

void MainComponent::ViewModeBar::setActiveMode(WorkspaceMode newMode)
{
    activeMode = newMode;
    trackerButton.setToggleState(activeMode == WorkspaceMode::tracker, juce::dontSendNotification);
    samplerButton.setToggleState(activeMode == WorkspaceMode::sampler, juce::dontSendNotification);
    arrangeButton.setToggleState(activeMode == WorkspaceMode::arrange, juce::dontSendNotification);
    signalButton.setToggleState(activeMode == WorkspaceMode::signal, juce::dontSendNotification);
    libraryButton.setToggleState(activeMode == WorkspaceMode::library, juce::dontSendNotification);
    mixButton.setToggleState(activeMode == WorkspaceMode::mix, juce::dontSendNotification);
    pluginsButton.setToggleState(activeMode == WorkspaceMode::plugins, juce::dontSendNotification);
    nodeButton.setToggleState(activeMode == WorkspaceMode::node, juce::dontSendNotification);
    codeButton.setToggleState(activeMode == WorkspaceMode::code, juce::dontSendNotification);
    recordButton.setToggleState(activeMode == WorkspaceMode::record, juce::dontSendNotification);
    scoreButton.setToggleState(activeMode == WorkspaceMode::score, juce::dontSendNotification);
    settingsButton.setToggleState(activeMode == WorkspaceMode::settings, juce::dontSendNotification);
    repaint();
}

void MainComponent::ViewModeBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff263140));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 1.0f,
               1.0f);

    g.setColour(juce::Colour(0xff8ea0b7));
    g.setFont(juce::Font(13.0f));
    g.drawText(workspaceModeName(activeMode) + " active", getLocalBounds().reduced(12, 0), juce::Justification::centredRight, true);
}

void MainComponent::ViewModeBar::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    titleLabel.setBounds(area.removeFromLeft(140));
    area.removeFromLeft(8);
    popOutButton.setBounds(area.removeFromRight(92));
    area.removeFromRight(8);
    auto buttonWidth = 78;
    trackerButton.setBounds(area.removeFromLeft(buttonWidth));
    samplerButton.setBounds(area.removeFromLeft(buttonWidth));
    arrangeButton.setBounds(area.removeFromLeft(buttonWidth));
    signalButton.setBounds(area.removeFromLeft(buttonWidth));
    libraryButton.setBounds(area.removeFromLeft(buttonWidth));
    mixButton.setBounds(area.removeFromLeft(buttonWidth));
    pluginsButton.setBounds(area.removeFromLeft(buttonWidth));
    nodeButton.setBounds(area.removeFromLeft(buttonWidth));
    codeButton.setBounds(area.removeFromLeft(buttonWidth));
    recordButton.setBounds(area.removeFromLeft(buttonWidth));
    scoreButton.setBounds(area.removeFromLeft(buttonWidth));
    settingsButton.setBounds(area.removeFromLeft(buttonWidth));
}

MainComponent::PluginRackBar::PluginRackBar()
{
    setName("Plugin Rack");
    titleLabel.setText("Master Insert", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    contextLabel.setText("Master", juce::dontSendNotification);
    contextLabel.setJustificationType(juce::Justification::centredLeft);
    contextLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(contextLabel);

    pluginNameLabel.setText("No plugin loaded", juce::dontSendNotification);
    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);
    pluginNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));

    catalogLabel.setText("No VST folders configured.", juce::dontSendNotification);
    catalogLabel.setJustificationType(juce::Justification::centredLeft);
    catalogLabel.setColour(juce::Label::textColourId, juce::Colour(0xff71839b));

    bypassButton.onClick = [this]
    {
        if (onBypassChanged)
            onBypassChanged(bypassButton.getToggleState());
    };
    bypassButton.setTooltip("Bypass this plugin");
    addAndMakeVisible(bypassButton);

    pathsButton.onClick = [this]
    {
        if (onManagePluginPaths)
            onManagePluginPaths();
    };
    pathsButton.setTooltip("Manage VST plugin search folders");
    addAndMakeVisible(pathsButton);

    openEditorButton.onClick = [this]
    {
        if (onOpenPluginEditor)
            onOpenPluginEditor();
    };
    openEditorButton.setTooltip("Open this plugin's own UI window");
    addAndMakeVisible(openEditorButton);

    fxStackButton.onClick = [this]
    {
        if (onOpenFxStack)
            onOpenFxStack();
    };
    fxStackButton.setTooltip("Open the full FX stack for this track");
    addAndMakeVisible(fxStackButton);

    loadButton.onClick = [this]
    {
        if (onLoadPlugin)
            onLoadPlugin();
    };
    loadButton.setTooltip("Load a VST3 plugin into this slot");
    addAndMakeVisible(loadButton);

    unloadButton.onClick = [this]
    {
        if (onUnloadPlugin)
            onUnloadPlugin();
    };
    unloadButton.setTooltip("Remove the loaded plugin from this slot");
    addAndMakeVisible(unloadButton);

    pathsButton.setVisible(false);
    loadButton.setVisible(false);
}

void MainComponent::PluginRackBar::setContextMaster()
{
    context = Context::master;
    selectedTrackIndex = -1;
    titleLabel.setText("Master Insert", juce::dontSendNotification);
    contextLabel.setText("Master", juce::dontSendNotification);
    fxStackButton.setEnabled(false);
}

void MainComponent::PluginRackBar::setContextTrack(int trackIndex, const juce::String& trackName)
{
    context = Context::track;
    selectedTrackIndex = trackIndex;
    titleLabel.setText("Track Insert", juce::dontSendNotification);
    contextLabel.setText(trackName.isNotEmpty() ? ("Track " + juce::String(trackIndex + 1) + " - " + trackName)
                                                : ("Track " + juce::String(trackIndex + 1)),
                         juce::dontSendNotification);
    fxStackButton.setEnabled(true);
}

void MainComponent::PluginRackBar::setPluginName(const juce::String& name)
{
    pluginNameLabel.setText(name.isNotEmpty() ? "Loaded: " + name : "No plugin loaded", juce::dontSendNotification);
    hasPlugin = name.isNotEmpty();
    openEditorButton.setEnabled(hasPlugin);
    bypassButton.setEnabled(hasPlugin);
    unloadButton.setEnabled(hasPlugin);
    fxStackButton.setEnabled(context == Context::track);
}

void MainComponent::PluginRackBar::setCatalogSummary(const juce::String& summary)
{
    catalogLabel.setText(summary, juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setBypassed(bool shouldBypass)
{
    bypassButton.setToggleState(shouldBypass, juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setHasPlugin(bool shouldHavePlugin)
{
    hasPlugin = shouldHavePlugin;
    if (! hasPlugin)
    {
        openEditorButton.setEnabled(false);
        bypassButton.setEnabled(false);
        unloadButton.setEnabled(false);
    }
    fxStackButton.setEnabled(context == Context::track);
}

void MainComponent::PluginRackBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff273243));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void MainComponent::PluginRackBar::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    titleLabel.setBounds(area.removeFromLeft(150));
    contextLabel.setBounds(area.removeFromLeft(220));
    area.removeFromLeft(360); // reserved - no longer shows the plugin name/catalog status text
    unloadButton.setBounds(area.removeFromRight(90));
    openEditorButton.setBounds(area.removeFromRight(110));
    fxStackButton.setBounds(area.removeFromRight(100).reduced(4, 0));
    bypassButton.setBounds(area.removeFromRight(100));
}

MainComponent::FxStackPanel::FxStackPanel()
{
    titleLabel.setText("Track FX Stack", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    trackLabel.setText("No track selected", juce::dontSendNotification);
    trackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb2cc));
    addAndMakeVisible(trackLabel);

    pluginList.setRowHeight(34);
    pluginList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d141d));
    addAndMakeVisible(pluginList);

    removeButton.onClick = [this] { if (onRemovePlugin) onRemovePlugin(getSelectedSlot()); };
    upButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onMovePlugin && slot > 0)
            onMovePlugin(slot, slot - 1);
    };
    downButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onMovePlugin && slot >= 0 && slot < pluginNames.size() - 1)
            onMovePlugin(slot, slot + 1);
    };
    bypassButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onBypassChanged && juce::isPositiveAndBelow(slot, pluginBypassStates.size()))
            onBypassChanged(slot, ! pluginBypassStates[slot]);
    };
    openButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onOpenPluginEditor && slot >= 0)
            onOpenPluginEditor(slot);
    };

    removeButton.setTooltip("Remove the selected plugin from the stack");
    upButton.setTooltip("Move the selected plugin up in the chain");
    downButton.setTooltip("Move the selected plugin down in the chain");
    bypassButton.setTooltip("Bypass the selected plugin");
    openButton.setTooltip("Open the selected plugin's own UI window");

    for (auto* button : { &removeButton, &upButton, &downButton, &bypassButton, &openButton })
        addAndMakeVisible(button);

    catalogLabel.setText("Available Plugins", juce::dontSendNotification);
    catalogLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb2cc));
    addAndMakeVisible(catalogLabel);

    searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colour(0xff6d7d91));
    searchBox.onTextChange = [this]
    {
        catalogBrowser.setFilterText(searchBox.getText());
    };
    addAndMakeVisible(searchBox);

    catalogBrowser.onEntryChosen = [this](const VstPluginCatalog::Entry& entry)
    {
        if (onAddPlugin)
            onAddPlugin(entry);
    };
    addAndMakeVisible(catalogBrowser);

    addButton.onClick = [this] { addSelectedCatalogEntry(); };
    insertButton.onClick = [this] { insertSelectedCatalogEntry(); };
    rescanButton.onClick = [this] { if (onRescanRequested) onRescanRequested(); };

    addButton.setTooltip("Add the selected plugin to the end of the stack");
    insertButton.setTooltip("Insert the selected plugin at the current position");
    rescanButton.setTooltip("Rescan VST folders for plugins");

    for (auto* button : { &addButton, &insertButton, &rescanButton })
        addAndMakeVisible(button);

    refreshButtonState();
}

void MainComponent::FxStackPanel::setTrackName(const juce::String& name)
{
    trackLabel.setText(name.isNotEmpty() ? name : "Selected track", juce::dontSendNotification);
}

void MainComponent::FxStackPanel::setPlugins(const juce::StringArray& names, const juce::Array<bool>& bypassStates)
{
    const auto oldSelection = getSelectedSlot();
    pluginNames = names;
    pluginBypassStates = bypassStates;
    pluginList.updateContent();
    if (pluginNames.isEmpty())
        pluginList.deselectAllRows();
    else
        pluginList.selectRow(juce::jlimit(0, pluginNames.size() - 1, oldSelection), false, true);
    refreshButtonState();
    repaint();
}

void MainComponent::FxStackPanel::setCatalog(const juce::Array<VstPluginCatalog::Entry>& entries)
{
    catalogBrowser.setPlugins(entries);
}

void MainComponent::FxStackPanel::addSelectedCatalogEntry()
{
    if (auto* entry = catalogBrowser.getSelectedEntry())
        if (onAddPlugin)
            onAddPlugin(*entry);
}

void MainComponent::FxStackPanel::insertSelectedCatalogEntry()
{
    const auto slot = getSelectedSlot();
    if (auto* entry = catalogBrowser.getSelectedEntry())
        if (onInsertPlugin && slot >= 0)
            onInsertPlugin(slot, *entry);
}

int MainComponent::FxStackPanel::getSelectedSlot() const noexcept
{
    return pluginList.getSelectedRow();
}

void MainComponent::FxStackPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101820));
    g.setColour(juce::Colour(0xff27364a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void MainComponent::FxStackPanel::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(28));
    trackLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    auto leftColumn = area.removeFromLeft(area.getWidth() / 2 - 8);
    area.removeFromLeft(16);
    auto rightColumn = area;

    auto leftButtonRow = leftColumn.removeFromBottom(38);
    removeButton.setBounds(leftButtonRow.removeFromLeft(90).reduced(3));
    upButton.setBounds(leftButtonRow.removeFromLeft(70).reduced(3));
    downButton.setBounds(leftButtonRow.removeFromLeft(76).reduced(3));
    bypassButton.setBounds(leftButtonRow.removeFromLeft(90).reduced(3));
    openButton.setBounds(leftButtonRow.removeFromLeft(100).reduced(3));
    pluginList.setBounds(leftColumn.reduced(0, 8));

    auto rightButtonRow = rightColumn.removeFromBottom(38);
    addButton.setBounds(rightButtonRow.removeFromLeft(86).reduced(3));
    insertButton.setBounds(rightButtonRow.removeFromLeft(86).reduced(3));
    rescanButton.setBounds(rightButtonRow.removeFromLeft(86).reduced(3));

    catalogLabel.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(2);
    searchBox.setBounds(rightColumn.removeFromTop(28));
    rightColumn.removeFromTop(6);
    catalogBrowser.setBounds(rightColumn);
}

int MainComponent::FxStackPanel::getNumRows()
{
    return pluginNames.size();
}

void MainComponent::FxStackPanel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto row = juce::Rectangle<int>(0, 0, width, height).reduced(5, 3);
    g.setColour(rowIsSelected ? juce::Colour(0xff1f5f86) : juce::Colour(0xff172332));
    g.fillRoundedRectangle(row.toFloat(), 6.0f);

    const auto bypassed = juce::isPositiveAndBelow(rowNumber, pluginBypassStates.size()) && pluginBypassStates[rowNumber];
    g.setColour(bypassed ? juce::Colour(0xffff7d7d) : juce::Colour(0xff6fe7ff));
    g.fillEllipse((float) row.getX() + 10.0f, (float) row.getCentreY() - 4.0f, 8.0f, 8.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(juce::String(rowNumber + 1).paddedLeft('0', 2) + "  " + pluginNames[rowNumber],
               row.reduced(28, 0),
               juce::Justification::centredLeft,
               true);

    if (bypassed)
    {
        g.setColour(juce::Colour(0xffffa0a0));
        g.setFont(juce::Font(12.0f));
        g.drawText("bypassed", row.reduced(8), juce::Justification::centredRight, true);
    }
}

void MainComponent::FxStackPanel::selectedRowsChanged(int)
{
    refreshButtonState();
}

void MainComponent::FxStackPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (onOpenPluginEditor && juce::isPositiveAndBelow(row, pluginNames.size()))
        onOpenPluginEditor(row);
}

void MainComponent::FxStackPanel::refreshButtonState()
{
    const auto slot = getSelectedSlot();
    const auto hasSelection = juce::isPositiveAndBelow(slot, pluginNames.size());
    insertButton.setEnabled(hasSelection);
    removeButton.setEnabled(hasSelection);
    upButton.setEnabled(hasSelection && slot > 0);
    downButton.setEnabled(hasSelection && slot < pluginNames.size() - 1);
    bypassButton.setEnabled(hasSelection);
    openButton.setEnabled(hasSelection);
}

MainComponent::MainComponent()
    : MainComponent(StartupProgressCallback{})
{
}

MainComponent::MainComponent(StartupProgressCallback startupProgressCallback)
{
    auto reportStartup = [&startupProgressCallback](const juce::String& statusText, float progress)
    {
        if (startupProgressCallback)
            startupProgressCallback(statusText, juce::jlimit(0.0f, 1.0f, progress));
    };

    reportStartup("Preparing application shell...", 0.08f);
    setWantsKeyboardFocus(true);
    transportBar.setAppTitle("Creation Station");
    transportBar.setLogoImage(creation::ui::getSuiteLogoImage(creation::ui::SuiteLogoId::station));

    appManifest = CreationStationAppManifest::createDefault(
        juce::JUCEApplicationBase::getInstance() != nullptr
            ? juce::JUCEApplicationBase::getInstance()->getApplicationVersion()
            : "0.5.1");

    reportStartup("Opening audio engine...", 0.18f);
    deviceManager.initialise(32, 2, nullptr, true, {}, nullptr);
    engine.attachToDevice(deviceManager);
    engine.setPlaying(false);
    transportBar.setPlaybackVisualState(false, false);

    reportStartup("Building the studio surface...", 0.28f);
    setSize(1400, 900);
    addAndMakeVisible(authGateView);
    transportBarSafe = &transportBar;
    pluginRackBarSafe = &pluginRackBar;
    mixerPanelSafe = &mixerPanel;
    authGateView.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    authGateView.onLogoutRequested = [this]
    {
        authSession.clearSession();
        authenticated = false;
        refreshAuthState();
    };
    authSession.onStatusChanged = [this](const juce::String& text)
    {
        authGateView.setStatusText(text);
        transportBar.setStatusText(text);
    };
    authSession.onError = [this](const juce::String& text)
    {
        authGateView.setStatusText(text);
        transportBar.setStatusText(text);
        authGateView.setBusy(false);
        transportBar.signInButton.setEnabled(true);
    };
    authSession.onBusyChanged = [this](bool shouldBeBusy)
    {
        authGateView.setBusy(shouldBeBusy);
        transportBar.signInButton.setEnabled(! shouldBeBusy);
    };
    authSession.onAuthenticated = [this](const DesktopAuthSession::SessionData& session)
    {
        authenticated = true;
        transportBar.setProfile(makeHeaderProfile(session));
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Signed in. Loading your workspace...");
        refreshAuthState();
    };
    authSession.onSessionCleared = [this]
    {
        authenticated = false;
        appContextSyncInProgress = false;
        transportBar.clearProfile();
        authGateView.setAccountText("Not signed in yet.");
        authGateView.setStatusText("Session cleared.");
        refreshAuthState();
    };
    suiteShellController.configure({ "Creation Station",
                                     creation::assets::SuiteAppDomain::station,
                                     juce::Colour(0xff15181d) },
                                   [this](const juce::String& status)
                                   {
                                       transportBar.setStatusText(status);
                                       if (status.containsIgnoreCase("saved suite-wide"))
                                       {
                                           juce::String suiteSettingsError;
                                           suiteSettings = suiteSettingsStore.load(suiteSettingsError);
                                           if (suiteSettingsError.isNotEmpty())
                                               transportBar.setStatusText(suiteSettingsError);
                                       }
                                       if (status.containsIgnoreCase("AI routing"))
                                           loadSuiteAiProviderSettings();
                                   });
    suiteShellController.onProjectOpenRequested = [this](const juce::File& projectDirectory)
    {
        openProject(projectDirectory);
    };
    transportBar.onProjectMenuRequested = [this]
    {
        suiteShellController.showProjectBrowser();
    };
    transportBar.onAudioRequested = [this]
    {
        showAudioSettings();
    };
    transportBar.onSuiteRequested = [this]
    {
        suiteShellController.showSuiteSettings();
    };
    transportBar.onTourRequested = [this]
    {
        showTour();
    };
    transportBar.onLearnMidiRequested = [this](const juce::String& targetId, const juce::String& displayLabel)
    {
        showMidiLearnDialog(targetId, displayLabel);
    };
    if (authSession.hasValidSession())
    {
        authenticated = true;
        const auto& session = authSession.getSession();
        transportBar.setProfile(makeHeaderProfile(session));
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Restored your saved login.");
    }

    reportStartup("Loading storage and settings...", 0.40f);
    juce::String storageError;
    if (! true)
    {
        auto startupStorageMessage = storageError.isNotEmpty()
                                       ? storageError
                                       : "Storage is not configured yet. The studio can still open. Set it up later when you want to save projects or manage content.";
        transportBar.setStatusText(startupStorageMessage);
    }

    loadAppSettings();
    {
        juce::String suiteSettingsError;
        suiteSettings = suiteSettingsStore.load(suiteSettingsError);
        if (suiteSettingsError.isNotEmpty())
            transportBar.setStatusText(suiteSettingsError);
    }
    reportStartup("Applying audio device settings...", 0.50f);
    applySelectedAudioDeviceSettings();

    reportStartup("Scanning VST plugin folders...", 0.60f);
    rescanVstCatalog();

    reportStartup("Opening project workspace...", 0.72f);
    auto loadedAutoloadProject = false;
    if (suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        juce::String activeError;
        loadedAutoloadProject = ensureProjectSessionActive(activeError);
        if (! loadedAutoloadProject && activeError.isNotEmpty())
            transportBar.setStatusText(activeError);
    }
    transportBar.setProjectLabel("Project: " + (projectSession.isValid() ? projectSession.getManifest().projectName : juce::String("No project open")));
    settingsPanel.setStoragePath(suiteSettings.suiteVfsRoot.isNotEmpty() ? juce::File(suiteSettings.suiteVfsRoot).getFullPathName() : "");
    if (projectSession.isValid())
        settingsPanel.setProjectMetadata(projectSession.getManifest());
    settingsPanel.setAutoloadEnabled(true);
    settingsPanel.setAiProviderSettings(aiProviderSettings);
    aiPanel.setSelectedProvider(aiProviderSettings.providerDisplayName);
    aiPanel.setSelectedModel(aiProviderSettings.modelName);
    refreshAiModelCatalog();

    if (! suiteSettings.suiteVfsRoot.isNotEmpty() && storageError.isEmpty())
        contentPanel.setStatusText("Storage is not configured yet. You can keep using the studio and set up saving/content later.");

    if (loadedAutoloadProject)
    {
        reportStartup("Restoring project tracks and clips...", 0.80f);
        loadSessionFromDisk();
    }

    reportStartup("Loading control-surface maps...", 0.86f);
    juce::String controlSurfaceError;
    if (! controlSurfaceMappings.loadFromFile(suiteSettingsStore.getSuiteConfigDirectory().getChildFile("control-surface-mappings.json"), controlSurfaceError)
        || controlSurfaceMappings.getProfiles().isEmpty())
    {
        controlSurfaceMappings = ControlSurfaceMappingStore::createDefaultLibrary();
        controlSurfaceMappings.saveToFile(suiteSettingsStore.getSuiteConfigDirectory().getChildFile("control-surface-mappings.json"), controlSurfaceError);
    }
    auto* activePreset = controlSurfaceMappings.findProfileById(controlSurfaceMappings.getActivePresetId());
    transportBar.setMidiStatusText("Control preset: " + (activePreset != nullptr ? activePreset->displayName
                                                                                 : controlSurfaceMappings.getActivePresetId()));
    midiSurface.setControlSurfaceMappings(controlSurfaceMappings);
    midiSurface.setEngineForMidiLearn(engine);

    viewModeBar.onModeSelected = [this](WorkspaceMode mode)
    {
        setWorkspaceMode(mode);
    };
    viewModeBar.onPopOutRequested = [this]
    {
        popOutActiveWorkspace();
    };

    reportStartup("Creating studio panels...", 0.92f);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(viewModeBar);
    addAndMakeVisible(pluginRackBar);
    addAndMakeVisible(trackerPanel);
    addAndMakeVisible(samplePackBuilderPanel);
    addAndMakeVisible(arrangeView);
    addAndMakeVisible(signalLabPanel);
    addAndMakeVisible(contentPanel);
    addAndMakeVisible(mixerPanel);
    addAndMakeVisible(pluginsPanel);
    addAndMakeVisible(graphPanel);
    addAndMakeVisible(dslPanel);
    addAndMakeVisible(recordView);
    addAndMakeVisible(scorePanel);
    addAndMakeVisible(aiPanel);
    addAndMakeVisible(settingsPanel);
    poppedWorkspacePlaceholder.setJustificationType(juce::Justification::centred);
    poppedWorkspacePlaceholder.setFont(juce::Font(20.0f).boldened());
    poppedWorkspacePlaceholder.setColour(juce::Label::textColourId, juce::Colour(0xffc7d7ef));
    poppedWorkspacePlaceholder.setColour(juce::Label::backgroundColourId, juce::Colour(0xff121a25));
    addChildComponent(poppedWorkspacePlaceholder);
    addChildComponent(tourOverlay);

    pluginRackBar.setContextMaster();
    refreshPluginsPanel();
    trackerPanel.setTimelineModel(&timelineModel);
    trackerPanel.setTrackCount(engine.getTrackCount());
    arrangeView.setTotalTrackCount(engine.getTrackCount());
    arrangeView.setVisibleTrackCount(0);
    recordView.setTrackCount(engine.getTrackCount());
    armedTracks.resize((size_t) engine.getTrackCount(), false);
    monitoredTracks.resize((size_t) engine.getTrackCount(), false);
    recordView.onTrackArmChanged = [this](int trackIndex, bool shouldArm)
    {
        if (juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
        {
            armedTracks[(size_t) trackIndex] = shouldArm;
            engine.setTrackRecordingArmed(trackIndex, shouldArm);
            trackerPanel.setTrackArmed(trackIndex, shouldArm);
            saveSessionToDisk();
        }
    };

    refreshAuthState();

    transportBar.onPlay = [this]
    {
        engine.stopAssetPreview();
        if (! prepareTrackerPlayback())
            return;

        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        transportStartTimelineSeconds = timelineModel.getTransportSeconds();
        engine.setPlaybackPositionSeconds(transportStartTimelineSeconds);
        engine.setPlaying(true);
        midiSurface.setTransportState(true, false);
        transportBar.setPlaybackVisualState(true, false);
    };
    transportBar.onPause = [this]
    {
        engine.stopAssetPreview();
        engine.setPlaying(false);
        transportBar.setPlaybackVisualState(false, false);
        midiSurface.setTransportState(false, false);
        transportBar.setPlaybackVisualState(false, false);
    };
    transportBar.onStop = [this]
    {
        stopRecordingSession();
        engine.stopAssetPreview();
        engine.setPlaying(false);
        midiSurface.setTransportState(false, false);
        transportBar.setPlaybackVisualState(false, false);
    };
    transportBar.onRecord = [this]
    {
        engine.stopAssetPreview();
        if (engine.isRecording() || engine.isMidiRecording())
            stopRecordingSession();
        else if (startRecordingSession())
        {
            refreshTrackerPlaybackClips();
            transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            transportStartTimelineSeconds = timelineModel.getTransportSeconds();
            engine.setPlaying(true);
            midiSurface.setTransportState(true, true);
            transportBar.setPlaybackVisualState(true, true);
        }
    };
    transportBar.onRewind = [this]
    {
        auto previousSeconds = timelineModel.getPreviousBoundarySeconds(timelineModel.getTransportSeconds());
        timelineModel.setTransportSeconds(previousSeconds);
        transportStartTimelineSeconds = previousSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        engine.setPlaybackPositionSeconds(previousSeconds);
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
        transportBar.setStatusText("Transport: previous boundary");
    };
    transportBar.onFastForward = [this]
    {
        auto nextSeconds = timelineModel.getNextBoundarySeconds(timelineModel.getTransportSeconds());
        timelineModel.setTransportSeconds(nextSeconds);
        transportStartTimelineSeconds = nextSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        engine.setPlaybackPositionSeconds(nextSeconds);
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
        transportBar.setStatusText("Transport: next boundary");
    };
    transportBar.onLoopChanged = [this](bool loopEnabled)
    {
        timelineModel.setLoopEnabled(loopEnabled);
        transportBar.setStatusText(loopEnabled ? "Transport: loop on" : "Transport: loop off");
    };
    transportBar.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    transportBar.onOpenProfilePageRequested = [this]
    {
        suiteShellController.openSuiteProfile();
    };
    transportBar.onLogoutRequested = [this]
    {
        authSession.clearSession();
    };

    auto refreshInsertRack = [this]
    {
        if (pluginRackBar.isTrackContext())
        {
            auto trackIndex = pluginRackBar.getTrackIndex();
            pluginRackBar.setPluginName(engine.getTrackPluginName(trackIndex));
            pluginRackBar.setBypassed(engine.isTrackPluginBypassed(trackIndex));
        }
        else
        {
            pluginRackBar.setPluginName(engine.getMasterPluginName());
            pluginRackBar.setBypassed(engine.isMasterPluginBypassed());
        }
    };

    auto refreshVisibleBank = [this, refreshInsertRack]
    {
        auto bankOffset = mixerPanel.getBankOffset();
        auto visibleCount = mixerPanel.getVisibleChannelCount();
        auto selectedTrackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() : -1;

        midiSurface.setBankOffset(bankOffset);
        mixerPanel.setSelectedChannel(selectedTrackIndex);

        for (int slot = 0; slot < visibleCount; ++slot)
        {
            auto trackIndex = bankOffset + slot;
            if (trackIndex >= engine.getTrackCount())
                continue;

            auto name = engine.getTrackName(trackIndex);
            auto gain = engine.getTrackGain(trackIndex);
            auto pan = engine.getTrackPan(trackIndex);
            auto muted = engine.isTrackMuted(trackIndex);
            auto soloed = engine.isTrackSoloed(trackIndex);
            auto pluginName = engine.getTrackPluginName(trackIndex);
            auto pluginBypassed = engine.isTrackPluginBypassed(trackIndex);

            mixerPanel.setChannelName(trackIndex, name);
            mixerPanel.setChannelInsertName(trackIndex, pluginName.isNotEmpty() ? ("FX: " + pluginName) : "FX: none");
            mixerPanel.setChannelInsertBypassed(trackIndex, pluginBypassed);
            mixerPanel.setChannelGain(trackIndex, gain);
            mixerPanel.setChannelPan(trackIndex, pan);
            mixerPanel.setChannelMuted(trackIndex, muted);
            mixerPanel.setChannelSoloed(trackIndex, soloed);

            midiSurface.setChannelName(trackIndex, name);
            midiSurface.setChannelGain(trackIndex, gain);
            midiSurface.setChannelPan(trackIndex, pan);
            midiSurface.setChannelMuted(trackIndex, muted);
            midiSurface.setChannelSoloed(trackIndex, soloed);
            trackerPanel.setTrackLevel(trackIndex, engine.getTrackLevel(trackIndex));
        }

        auto masterGain = engine.getMasterGain();
        mixerPanel.setMasterGain(masterGain);
        midiSurface.setMasterFaderValue(masterGain);
        midiSurface.refreshVisibleWindow();
        refreshInsertRack();
    };

    auto selectTrack = [this, refreshVisibleBank](int trackIndex)
    {
        if (engine.getTrackCount() == 0)
            return;

        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        auto visibleStart = mixerPanel.getBankOffset();
        auto visibleEnd = visibleStart + mixerPanel.getVisibleChannelCount();
        if (trackIndex < visibleStart || trackIndex >= visibleEnd)
            mixerPanel.setBankOffset((trackIndex / mixerPanel.getVisibleChannelCount()) * mixerPanel.getVisibleChannelCount());

        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
        trackerPanel.setSelectedTrack(trackIndex);
        arrangeView.setSelectedTrack(trackIndex);
        refreshVisibleBank();
    };

    trackerPanel.onTrackSelected = [selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    trackerPanel.onTrackFxRequested = [this, selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
        showFxStackWindow();
    };

    trackerPanel.onAddTrackRequested = [this]
    {
        addTrack();
    };

    trackerPanel.onRemoveTrackRequested = [this](int trackIndex)
    {
        removeTrack(trackIndex);
    };

    trackerPanel.onTrackNameChanged = [this](int trackIndex, const juce::String& name)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackName(trackIndex, name);
        timelineModel.setTrackName(trackIndex, name);
        syncTrackViews();
        midiSurface.refreshVisibleWindow();
        saveSessionToDisk();
    };

    trackerPanel.onTrackKindChanged = [this](int trackIndex, cs::TrackKind kind)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        timelineModel.setTrackKind(trackIndex, kind);
        trackerPanel.setTrackKind(trackIndex, kind);
        arrangeView.setTrackKind(trackIndex, kind);
        engine.setTrackIsMidiKind(trackIndex, kind == cs::TrackKind::midi);
        engine.setTrackIsAutomationKind(trackIndex, kind == cs::TrackKind::automation);

        if (kind == cs::TrackKind::automation)
        {
            timelineModel.ensureAutomationClip(trackIndex);
            pushAutomationDataToEngine(trackIndex);
            trackerPanel.repaint();
        }

        saveSessionToDisk();
    };

    trackerPanel.onAutomationTargetRequested = [this](int trackIndex)
    {
        showAutomationTargetPicker(trackIndex);
    };

    trackerPanel.onMoveToFolderRequested = [this](int trackIndex)
    {
        showMoveToFolderPicker(trackIndex);
    };

    trackerPanel.onTrackReorderRequested = [this](int trackIndex, int destinationIndex)
    {
        if (performTrackMove(trackIndex, destinationIndex))
        {
            syncTrackViews();
            saveSessionToDisk();
        }
    };

    trackerPanel.onAutomationPointChanged = [this](int trackIndex)
    {
        pushAutomationDataToEngine(trackIndex);
    };

    trackerPanel.onAutomationDataCommitted = [this](int trackIndex)
    {
        pushAutomationDataToEngine(trackIndex);
        saveSessionToDisk();
    };

    trackerPanel.onAutomationRecordModeChanged = [this](int trackIndex, cs::AutomationRecordMode mode)
    {
        timelineModel.setAutomationRecordMode(trackIndex, mode);
        saveSessionToDisk();
    };

    trackerPanel.onAutomationRecordingRateChanged = [this](int trackIndex, int pointsPerSecond)
    {
        timelineModel.setAutomationRecordingRate(trackIndex, pointsPerSecond);
        saveSessionToDisk();
    };

    trackerPanel.onTrackArmChanged = [this](int trackIndex, bool shouldArm)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
            return;

        armedTracks[(size_t) trackIndex] = shouldArm;

        // An automation track has no audio input of its own - arming it means "record manual
        // control changes into this lane while playing," not "record audio," so it shouldn't
        // also flip the engine's audio-input recording-armed state.
        if (timelineModel.getTrackKind(trackIndex) != cs::TrackKind::automation)
            engine.setTrackRecordingArmed(trackIndex, shouldArm);

        recordView.setTrackCount(engine.getTrackCount());
        syncTrackViews();
        saveSessionToDisk();
    };

    trackerPanel.onTrackMuteChanged = [this](int trackIndex, bool muted)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackMuted(trackIndex, muted);
        mixerPanel.setChannelMuted(trackIndex, muted);
        midiSurface.setChannelMuted(trackIndex, muted);
        saveSessionToDisk();
    };

    trackerPanel.onTrackSoloChanged = [this](int trackIndex, bool soloed)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackSoloed(trackIndex, soloed);
        mixerPanel.setChannelSoloed(trackIndex, soloed);
        midiSurface.setChannelSoloed(trackIndex, soloed);
        saveSessionToDisk();
    };

    trackerPanel.onTrackMonitorChanged = [this](int trackIndex, bool monitored)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) monitoredTracks.size()))
            return;

        monitoredTracks[(size_t) trackIndex] = monitored;
        engine.setTrackMonitoringEnabled(trackIndex, monitored);
        trackerPanel.setTrackMonitored(trackIndex, monitored);
        saveSessionToDisk();
    };

    trackerPanel.onTrackStereoChanged = [this](int trackIndex, bool stereo)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackStereoEnabled(trackIndex, stereo);
        timelineModel.setTrackChannelMode(trackIndex, stereo ? cs::TrackChannelMode::stereo
                                                             : cs::TrackChannelMode::mono);
        trackerPanel.setTrackStereo(trackIndex, stereo);
        saveSessionToDisk();
    };

    trackerPanel.onTrackGainChanged = [this](int trackIndex, float gain)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackGain(trackIndex, gain);
        recordAutomationWriteIfArmed(trackIndex, cs::AutomationTargetKind::trackVolume, gain);
        mixerPanel.setChannelGain(trackIndex, gain);
        midiSurface.setChannelGain(trackIndex, gain);
        saveSessionToDisk();
    };

    trackerPanel.onTrackInputChanged = [this](int trackIndex, int inputChannel)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::midi)
        {
            // inputChannel here is 0 = Omni (all channels), 1-16 = a specific MIDI channel -
            // a separate axis from audio input routing, which doesn't apply to a MIDI track.
            engine.setTrackMidiInputChannel(trackIndex, inputChannel);
            trackerPanel.setTrackInput(trackIndex, inputChannel);
            saveSessionToDisk();
            return;
        }

        auto resolvedChannel = studioIOModel.getChannelForInputIndex(inputChannel);
        engine.setTrackInputChannel(trackIndex, resolvedChannel);
        trackerPanel.setTrackInput(trackIndex, inputChannel);
        saveSessionToDisk();
    };

    trackerPanel.onZoomOutRequested = [this]
    {
        timelineModel.zoomOut();
        trackerPanel.refreshTimelineView();
    };

    trackerPanel.onZoomInRequested = [this]
    {
        timelineModel.zoomIn();
        trackerPanel.refreshTimelineView();
    };

    trackerPanel.onPlayheadPositionChanged = [this](double seconds)
    {
        timelineModel.setTransportSeconds(seconds);
        engine.setPlaybackPositionSeconds(seconds);
        transportStartTimelineSeconds = seconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onLoopRegionChanged = [this](double startSeconds, double endSeconds)
    {
        timelineModel.setLoopRegion(startSeconds, endSeconds);
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onLoopRegionCleared = [this]
    {
        timelineModel.clearLoopRegion();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onTimelineSnapChanged = [this](bool enabled)
    {
        timelineModel.setTimelineSnapEnabled(enabled);
        saveSessionToDisk();
    };

    trackerPanel.onTimelineGridChanged = [this](double gridBeats)
    {
        timelineModel.setTimelineGridBeats(gridBeats);
        saveSessionToDisk();
    };

    trackerPanel.onMarkerAddRequested = [this]
    {
        timelineModel.addMarker(timelineModel.getTransportSeconds());
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onMarkerClicked = [this](const juce::String& markerId)
    {
        for (const auto& marker : timelineModel.getMarkers())
        {
            if (marker.id == markerId)
            {
                timelineModel.setTransportSeconds(marker.seconds);
                engine.setPlaybackPositionSeconds(marker.seconds);
                transportStartTimelineSeconds = marker.seconds;
                transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
                trackerPanel.refreshTimelineView();
                break;
            }
        }
    };

    trackerPanel.onClipMoved = [this](int clipIndex, int trackIndex, double startSeconds)
    {
        if (! clipDragUndoCaptured)
        {
            pushTimelineUndoState();
            clipDragUndoCaptured = true;
        }

        if (! timelineModel.moveClip(clipIndex, trackIndex, startSeconds))
            return;

        trackerPanel.setSelectedTrack(trackIndex);
        arrangeView.setSelectedTrack(trackIndex);
        if (juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        {
            pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
            mixerPanel.setSelectedChannel(trackIndex);
        }
        trackerPanel.refreshTimelineView();
    };

    trackerPanel.onClipMoveCommitted = [this]
    {
        clipDragUndoCaptured = false;
        refreshTrackerPlaybackClips();
        saveSessionToDisk();
    };

    trackerPanel.onClipSelected = [this](int clipIndex)
    {
        selectedClipIndex = clipIndex;
        trackerPanel.setSelectedClip(clipIndex);
    };

    trackerPanel.onClipRenameRequested = [this](int clipIndex)
    {
        renameClip(clipIndex);
    };

    trackerPanel.onClipSplitRequested = [this](int clipIndex, double splitSeconds)
    {
        splitClipAt(clipIndex, splitSeconds);
    };

    trackerPanel.onClipDuplicateRequested = [this](int clipIndex)
    {
        duplicateClip(clipIndex);
    };

    trackerPanel.onClipDeleteRequested = [this](int clipIndex)
    {
        deleteClip(clipIndex);
    };

    trackerPanel.onClipEditRequested = [this](int clipIndex)
    {
        showMidiEditorWindow(clipIndex);
    };

    trackerPanel.onEmptyMidiClipRequested = [this](int trackIndex, double seconds)
    {
        auto defaultBeats = 4.0;
        auto durationSeconds = timelineModel.beatToSeconds(defaultBeats);

        juce::String errorMessage;
        auto clipIndex = timelineModel.addClip(cs::ClipKind::midi,
                                               trackIndex,
                                               "MIDI",
                                               "",
                                               "midi-editor",
                                               juce::File(),
                                               seconds,
                                               durationSeconds,
                                               errorMessage);
        if (clipIndex < 0)
        {
            transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not create MIDI clip.");
            return;
        }

        projectDirty = true;
        trackerPanel.refreshTimelineView();
        showMidiEditorWindow(clipIndex);
    };
    trackerPanel.onAudioFilesDropped = [this](const juce::StringArray& files, int trackIndex, double startSeconds)
    {
        importAudioFilesToTracker(files, trackIndex, startSeconds);
    };

    trackerPanel.onTempoChanged = [this](double bpm)
    {
        timelineModel.setTempo(bpm, timelineModel.getTimeSignatureNumerator(), timelineModel.getTimeSignatureDenominator());
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        trackerPanel.setTimingInfo(timelineModel.getTempoBpm(),
                                   timelineModel.getTimeSignatureNumerator(),
                                   timelineModel.getTimeSignatureDenominator(),
                                   timelineModel.getMusicalKey());
        trackerPanel.refreshTimelineView();
        projectDirty = true;
        saveSessionToDisk();
    };

    trackerPanel.onTimeSignatureChanged = [this](int numerator, int denominator)
    {
        timelineModel.setTempo(timelineModel.getTempoBpm(), numerator, denominator);
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        trackerPanel.setTimingInfo(timelineModel.getTempoBpm(),
                                   timelineModel.getTimeSignatureNumerator(),
                                   timelineModel.getTimeSignatureDenominator(),
                                   timelineModel.getMusicalKey());
        trackerPanel.refreshTimelineView();
        projectDirty = true;
        saveSessionToDisk();
    };

    trackerPanel.onKeyChanged = [this](const juce::String& key)
    {
        timelineModel.setMusicalKey(key);
        trackerPanel.refreshTimelineView();
        projectDirty = true;
        saveSessionToDisk();
    };

    trackerPanel.onPitchPipeTriggered = [this](double noteHz)
    {
        constexpr double sampleRate = 44100.0;
        constexpr double durationSeconds = 2.0;
        const int numSamples = (int) (sampleRate * durationSeconds);

        juce::AudioBuffer<float> toneBuffer(1, numSamples);
        toneBuffer.clear();
        auto* samples = toneBuffer.getWritePointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            double t = (double) i / sampleRate;
            double attack = juce::jmin(1.0, t / 0.03);
            double release = juce::jmax(0.0, 1.0 - ((t - 1.5) / 0.5));
            double env = attack * (t > 1.5 ? release : 1.0);

            double wave = 0.70 * std::sin(2.0 * juce::MathConstants<double>::pi * noteHz * t)
                        + 0.22 * std::sin(4.0 * juce::MathConstants<double>::pi * noteHz * t)
                        + 0.08 * std::sin(6.0 * juce::MathConstants<double>::pi * noteHz * t);
            samples[i] = (float) (wave * env * 0.4);
        }

        juce::String error;
        if (engine.previewGeneratedBuffer(toneBuffer, sampleRate, error))
            transportBar.setStatusText("Pitch Pipe: " + juce::String(juce::roundToInt(noteHz)) + " Hz tone played");
        else if (error.isNotEmpty())
            transportBar.setStatusText("Pitch Pipe error: " + error);
    };

    arrangeView.onTrackSelected = [selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    arrangeView.onAddTrackRequested = [this]
    {
        addTrack();
    };

    arrangeView.onRemoveTrackRequested = [this](int trackIndex)
    {
        removeTrack(trackIndex);
    };

    arrangeView.onImportAssetRequested = [this]
    {
        importProjectSounds();
    };

    arrangeView.onAssetPreviewRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        if (! projectSession.isValid())
            return;

        juce::String errorMessage;
        creation::assets::MaterializedAssetLease lease;
        if (! projectSession.materializeEntry(suiteSettings, asset.logicalPath,
                                              creation::assets::MaterializationAccess::readOnly,
                                              lease, errorMessage))
            return;

        WorkstationAudioEngine::PreviewSettings settings;
        settings.startNormalized = arrangeView.getTrimStart();
        settings.endNormalized = arrangeView.getTrimEnd();
        settings.gainDecibels = arrangeView.getGainDecibels();
        settings.fadeInNormalized = arrangeView.getFadeInNormalized();
        settings.fadeOutNormalized = arrangeView.getFadeOutNormalized();
        settings.reverse = arrangeView.isReverseEnabled();
        settings.normalize = arrangeView.isNormalizeEnabled();

        if (engine.previewAssetFile(lease.materializedFile, settings, errorMessage))
            transportBar.setStatusText("Previewing: " + asset.displayName);
    };

    arrangeView.onArrangementChanged = [this]
    {
        refreshFoleyArrangement();
        saveSessionToDisk();
    };

    signalLabPanel.onPreviewRequested = [this](const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& suggestedName)
    {
        juce::String errorMessage;
        if (engine.previewGeneratedBuffer(buffer, sampleRate, errorMessage))
            transportBar.setStatusText("Previewing signal: " + suggestedName);
        else if (errorMessage.isNotEmpty())
            transportBar.setStatusText(errorMessage);
    };

    signalLabPanel.onRenderRequested = [this](const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& suggestedName)
    {
        juce::String projectError;
        if (! ensureProjectSessionActive(projectError))
        {
            transportBar.setStatusText(projectError.isNotEmpty() ? projectError : "Could not initialize project for rendered sounds.");
            return;
        }

        juce::String errorMessage;
        auto renderedFile = juce::File();
        if (renderedFile.existsAsFile())
        {
            if (engine.getTrackCount() == 0)
                addTrack();

            auto targetTrack = trackerPanel.getSelectedTrack();
            if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
                targetTrack = 0;

            creation::assets::AssetDescriptor renderedAsset;
            juce::String clipError;
            auto clipIndex = placeAudioAssetOnTracker(renderedAsset,
                                                      targetTrack,
                                                      timelineModel.getTransportSeconds(),
                                                      "signal",
                                                      clipError);
            if (clipIndex < 0)
            {
                transportBar.setStatusText(clipError.isNotEmpty() ? clipError
                                                                  : "Rendered sound, but could not place it on the Tracker.");
                refreshProjectAssets();
                refreshContentLibrary();
                saveSessionToDisk();
                return;
            }

            refreshProjectAssets();
            refreshContentLibrary();
            trackerPanel.setSelectedTrack(targetTrack);
            trackerPanel.refreshTimelineView();
            setWorkspaceMode(WorkspaceMode::tracker);
            saveSessionToDisk(true);
            transportBar.setStatusText("Rendered signal to Tracker: " + renderedFile.getFileName());
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    signalLabPanel.onPatchExportRequested = [this](const juce::String& patchJson, const juce::String& suggestedName)
    {
        if (! projectSession.isValid())
        {
            juce::String projectError;
            if (! creation::assets::ProjectWorkspaceService::createProject(suiteSettings, creation::assets::SuiteAppDomain::station, "New Project", "1.0.0", "1.0.0", projectSession, projectError))
            {
                transportBar.setStatusText("Could not create a project for patch export.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        }

        juce::String errorMessage;
        auto patchFile = juce::File();
        if (patchFile.existsAsFile())
        {
            refreshContentLibrary();
            saveSessionToDisk();
            transportBar.setStatusText("Exported project sound file: " + patchFile.getFileName());
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    signalLabPanel.onPatchSaveToLibraryRequested = [this](const juce::String& patchJson, const juce::String& suggestedName)
    {
        if (! projectSession.isValid())
        {
            juce::String projectError;
            if (! creation::assets::ProjectWorkspaceService::createProject(suiteSettings, creation::assets::SuiteAppDomain::station, "New Project", "1.0.0", "1.0.0", projectSession, projectError))
            {
                transportBar.setStatusText("Could not create a project for this sound.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        }

        juce::String errorMessage;
        auto patchFile = juce::File();
        if (patchFile.existsAsFile())
        {
            refreshProjectAssets();
            refreshContentLibrary();
            saveSessionToDisk(true);
            transportBar.setStatusText("Saved project sound: " + patchFile.getFileName());
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    signalLabPanel.onPatchLoadRequested = [this]
    {
        auto startDirectory = suiteSettings.suiteVfsRoot.isNotEmpty()
            ? juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/User").getChildFile("Patches")
            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

        patchChooser = std::make_unique<juce::FileChooser>("Load a Creation Station sound",
                                                           startDirectory,
                                                           "*.cspatch");

        patchChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& chooser)
                                  {
                                      auto file = chooser.getResult();
                                      patchChooser.reset();

                                      if (! file.existsAsFile())
                                          return;

                                      juce::String errorMessage;
                                      cw::PatchDocument document;
                                      if (! cw::parsePatchDocumentJson(file.loadFileAsString(), document, errorMessage))
                                      {
                                          transportBar.setStatusText(errorMessage);
                                          return;
                                      }

                                      if (! signalLabPanel.loadPatchDocument(document, errorMessage))
                                      {
                                          transportBar.setStatusText(errorMessage);
                                          return;
                                      }

                                      transportBar.setStatusText("Loaded patch: " + file.getFileName());
                                      setWorkspaceMode(WorkspaceMode::signal);
                                  });
    };

    dslPanel.onSourceExportRequested = [this](const juce::String& sourceText, const juce::String& suggestedName)
    {
        auto startDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        celSourceChooser = std::make_unique<juce::FileChooser>("Export CEL source",
                                                                    startDirectory.getChildFile(suggestedName + ".cel"),
                                                                    "*.cel");

        celSourceChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                           [this, sourceText](const juce::FileChooser& chooser)
                                           {
                                               auto file = chooser.getResult();
                                               celSourceChooser.reset();

                                               if (file == juce::File())
                                                   return;

                                               if (file.replaceWithText(sourceText))
                                                   transportBar.setStatusText("Exported CEL source: " + file.getFileName());
                                               else
                                                   transportBar.setStatusText("Could not write " + file.getFileName());
                                           });
    };

    dslPanel.onSourceSaveToLibraryRequested = [this](const juce::String& sourceText, const juce::String& suggestedName)
    {
        if (! ensureStorageRootConfigured())
            return;

        auto celDirectory = juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/User").getChildFile("CEL");
        celDirectory.createDirectory();
        auto celFile = celDirectory.getChildFile(suggestedName + ".cel").getNonexistentSibling();

        if (celFile.replaceWithText(sourceText))
        {
            refreshContentLibrary();
            transportBar.setStatusText("Saved to your library: " + celFile.getFileName());
        }
        else
        {
            transportBar.setStatusText("Could not save " + celFile.getFileName());
        }
    };

    dslPanel.onSourceLoadRequested = [this]
    {
        auto startDirectory = suiteSettings.suiteVfsRoot.isNotEmpty()
            ? juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/User").getChildFile("CEL")
            : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

        celSourceChooser = std::make_unique<juce::FileChooser>("Load a CEL source file",
                                                                    startDirectory,
                                                                    "*.cel");

        celSourceChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                           [this](const juce::FileChooser& chooser)
                                           {
                                               auto file = chooser.getResult();
                                               celSourceChooser.reset();

                                               if (! file.existsAsFile())
                                                   return;

                                               dslPanel.loadSourceFromFile(file);
                                               transportBar.setStatusText("Loaded CEL source: " + file.getFileName());
                                               setWorkspaceMode(WorkspaceMode::code);
                                           });
    };

    contextEngine.onContextReady = [this](const CreationStationContextEngine::ContextPacket& packet)
    {
        pendingAiContextPacket = packet;
        pendingAiContextPacketValid = true;
        aiPanel.setContextPacket(packet);
        aiPanel.setTaskPlan(taskPlanner.buildPlan(packet.request.prompt, packet));
        transportBar.setStatusText("AI context packet and task plan ready.");
        if (pendingAiPrompt.isNotEmpty() && ! aiCompletionInFlight)
            launchAiCompletion(packet);
    };

    aiPanel.onModeChanged = [this](AiPanel::GuidanceMode mode)
    {
        juce::String status;
        switch (mode)
        {
            case AiPanel::GuidanceMode::normal: status = "AI mode: Normal."; break;
            case AiPanel::GuidanceMode::learn: status = "AI mode: Learn."; break;
            case AiPanel::GuidanceMode::research: status = "AI mode: Research."; break;
        }

        transportBar.setStatusText(status);
    };

    aiPanel.onAccessChanged = [this](AiPanel::AccessLevel level)
    {
        juce::String status = "AI access: ";
        switch (level)
        {
            case AiPanel::AccessLevel::askFirst: status += "Ask first"; break;
            case AiPanel::AccessLevel::appOnly: status += "App only"; break;
            case AiPanel::AccessLevel::fileChanges: status += "Files"; break;
            case AiPanel::AccessLevel::fullAccess: status += "Full access"; break;
        }

        transportBar.setStatusText(status + ".");
    };

    aiPanel.onModelChanged = [this](const juce::String& modelName)
    {
        aiProviderSettings.modelName = modelName.trim();
        settingsPanel.setAiProviderSettings(aiProviderSettings);
        saveAppSettings();
    };

    aiPanel.onProviderChanged = [this](const juce::String& providerName)
    {
        const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(providerName);
        aiProviderSettings.providerDisplayName = profile.displayName;
        aiProviderSettings.providerId = profile.providerId;
        if (creation::services::SuiteAiProviderRuntime::shouldReplaceBaseUrlOnProviderSwitch(aiProviderSettings.baseUrl, profile))
            aiProviderSettings.baseUrl = profile.defaultBaseUrl;
        aiProviderSettings.modelName = creation::services::SuiteAiProviderRuntime::defaultModelName(profile);
        aiPanel.setSelectedProvider(aiProviderSettings.providerDisplayName);
        aiPanel.setSelectedModel(aiProviderSettings.modelName);
        settingsPanel.setAiProviderSettings(aiProviderSettings);

        saveAppSettings();

        refreshAiModelCatalog();
        transportBar.setStatusText("AI provider: " + aiProviderSettings.providerDisplayName + ".");
    };

    aiPanel.onPromptSubmitted = [this](const juce::String& submittedPrompt)
    {
        refreshAiContextStore();
        pendingAiPrompt = submittedPrompt;

        CreationStationContextEngine::RetrievalRequest request;
        request.prompt = pendingAiPrompt;
        request.workspaceMode = workspaceModeName(activeMode).toLowerCase();
        request.projectName = projectSession.isValid() ? projectSession.getManifest().projectName : juce::String();
        request.maxItems = 6;

        contextEngine.submitRequest(request);
        transportBar.setStatusText("Building AI context packet...");
    };

    aiPanel.onExecuteNextStep = [this](const CreationStationTaskPlanner::TaskStep& step)
    {
        executeAiTaskStep(step);
    };
    aiPanel.onCollapsedChanged = [this](bool shouldCollapse)
    {
        aiSidebarCollapsed = shouldCollapse;
        resized();
    };

    settingsPanel.onNewProjectRequested = [this] { createNewProject(); };
    settingsPanel.onOpenProjectRequested = [this] { openProject(); };
    settingsPanel.onSaveProjectRequested = [this] { saveProject(); };
    settingsPanel.onRevealProjectFolderRequested = [this] { revealProjectFolder(); };
    settingsPanel.onChangeStorageRequested = [this] { suiteShellController.showSuiteSettings(); };
    settingsPanel.onProjectMetadataChanged = [this](const creation::assets::ProjectManifest& metadata)
    {
        juce::String errorMessage;
        if (! true)
        {
            transportBar.setStatusText(errorMessage);
            return;
        }

        settingsPanel.setProjectMetadata(projectSession.getManifest());
        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        saveSessionToDisk(true);

        if (! projectDirty)
            transportBar.setStatusText("Project metadata updated.");
    };
    settingsPanel.onOpenAudioRequested = [this]
    {
        showAudioSettings();
    };
    settingsPanel.onOpenDriverControlPanelRequested = [this]
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            if (device->hasControlPanel())
            {
                device->showControlPanel();
                refreshAudioDeviceSettingsView();
                refreshTrackInputSources();
                transportBar.setStatusText("Opened audio driver panel.");
                return;
            }
        }

        transportBar.setStatusText("The active audio driver does not expose a control panel.");
        refreshAudioDeviceSettingsView();
    };
    settingsPanel.onRefreshStudioInputsRequested = [this]
    {
        refreshAudioDeviceSettingsView();
        refreshTrackInputSources();
        transportBar.setStatusText("Studio inputs refreshed.");
    };
    settingsPanel.onAudioSystemChanged = [this](const juce::String& audioSystem)
    {
        setAudioSystem(audioSystem);
    };
    settingsPanel.onAudioInputDeviceChanged = [this](const juce::String& inputDeviceName)
    {
        setAudioInputDevice(inputDeviceName);
    };
    settingsPanel.onAudioOutputDeviceChanged = [this](const juce::String& outputDeviceName)
    {
        setAudioOutputDevice(outputDeviceName);
    };
    settingsPanel.onStudioInputNameChanged = [this](int inputIndex, const juce::String& inputName)
    {
        studioIOModel.setInputName(inputIndex, inputName);
        juce::Array<juce::String> trackerInputNames;
        for (const auto& name : studioIOModel.getNames())
            trackerInputNames.add(name);
        trackerPanel.setInputSources(trackerInputNames);
        syncTrackViews();
        saveAppSettings();
        transportBar.setStatusText("Studio input renamed.");
    };
    settingsPanel.onManageVstPathsRequested = [this]
    {
        configureVstSearchPaths();
    };
    settingsPanel.onManageControlSurfaceMappingsRequested = [this]
    {
        editControlSurfaceMappings();
    };
    settingsPanel.onRefreshMidiDevicesRequested = [this]
    {
        refreshMidiDeviceSettings();
    };
    settingsPanel.onMidiInputDeviceEnabledChanged = [this](const juce::String& deviceId, bool enabled)
    {
        deviceManager.setMidiInputDeviceEnabled(deviceId, enabled);

        if (enabled)
            disabledMidiInputDeviceIds.removeString(deviceId);
        else
            disabledMidiInputDeviceIds.addIfNotAlreadyThere(deviceId);

        saveAppSettings();
        refreshMidiDeviceSettings();
    };
    settingsPanel.onMidiInputDeviceRouteChanged = [this](const juce::String& deviceId, int trackIndexOrMinusOne)
    {
        // Only one track can own a given device at a time - clear it from whichever track had
        // it before applying the new choice.
        for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
            if (engine.getTrackMidiInputDeviceId(trackIndex) == deviceId)
                engine.setTrackMidiInputDeviceId(trackIndex, {});

        if (juce::isPositiveAndBelow(trackIndexOrMinusOne, engine.getTrackCount()))
            engine.setTrackMidiInputDeviceId(trackIndexOrMinusOne, deviceId);

        projectDirty = true;
        saveSessionToDisk();
        refreshMidiDeviceSettings();
    };
    settingsPanel.onAutoloadChanged = [this](bool enabled)
    {
        juce::ignoreUnused(enabled);
        autoloadLastProject = true;
        settingsPanel.setAutoloadEnabled(true);
        saveAppSettings();
    };
    settingsPanel.onAiProviderSettingsChanged = [this](const AiProviderSettings& settings)
    {
        aiProviderSettings = settings;
        aiPanel.setSelectedProvider(aiProviderSettings.providerDisplayName);
        aiPanel.setSelectedModel(aiProviderSettings.modelName);

        juce::String errorMessage;
        if (! saveSuiteAiProviderSettings(aiProviderSettings, errorMessage))
            transportBar.setStatusText(errorMessage);

        saveAppSettings();
        refreshAiModelCatalog();
    };
    settingsPanel.onRefreshAiModelsRequested = [this]
    {
        refreshAiModelCatalog();
    };

    contentPanel.onRefreshRequested = [this]
    {
        refreshContentLibrary();
    };

    contentPanel.onOpenContentFolderRequested = [this]
    {
        if (! ensureStorageRootConfigured())
            return;

        juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content").revealToUser();
    };

    contentPanel.onAdminPublishRequested = [this]
    {
        if (! authenticated || ! isAdminRole(authSession.getSession().user.role))
        {
            transportBar.setStatusText("Admin publishing is only available to admin accounts.");
            return;
        }

        if (! ensureStorageRootConfigured())
            return;

        contentUploadChooser = std::make_unique<juce::FileChooser>("Choose a content package to publish",
                                                                   juce::File(suiteSettings.suiteVfsRoot),
                                                                   "*.cspatch;*.cspack;*.zip;*.wav");

        contentUploadChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                          [this](const juce::FileChooser& chooser)
                                          {
                                              auto selectedFile = chooser.getResult();
                                              contentUploadChooser.reset();

                                              if (! selectedFile.existsAsFile())
                                                  return;

                                              auto* dialog = new juce::AlertWindow("Admin Publish Content",
                                                                                   "Enter content metadata for upload.",
                                                                                   juce::MessageBoxIconType::QuestionIcon);
                                              dialog->addTextEditor("name", selectedFile.getFileNameWithoutExtension());
                                              dialog->addTextEditor("type", selectedFile.hasFileExtension(".cspatch") ? "patch"
                                                                                     : selectedFile.hasFileExtension(".cspack") ? "pack"
                                                                                     : selectedFile.hasFileExtension(".wav") ? "sample-pack"
                                                                                     : "pack");
                                              dialog->addTextEditor("version", "0.1.0");
                                              dialog->addTextEditor("description", "Published from Creation Station.");
                                              dialog->addTextEditor("tags", "creation-station");
                                              dialog->addTextEditor("tier", "");
                                              dialog->addTextEditor("minAppVersion", "0.2.0");
                                              dialog->addButton("Publish", 1);
                                              dialog->addButton("Cancel", 0);

                                              auto safeThis = juce::Component::SafePointer<MainComponent>(this);
                                              dialog->enterModalState(true, juce::ModalCallbackFunction::create(
                                                  [safeThis, dialog, selectedFile](int result) mutable
                                                  {
                                                      std::unique_ptr<juce::AlertWindow> ownedDialog(dialog);
                                                      if (result != 1 || safeThis == nullptr)
                                                          return;

                                                      ContentApiClient::AdminUploadRequest request;
                                                      request.productSlug = "creation-station";
                                                      request.name = ownedDialog->getTextEditorContents("name").trim();
                                                      request.itemType = ownedDialog->getTextEditorContents("type").trim();
                                                      request.version = ownedDialog->getTextEditorContents("version").trim();
                                                      request.description = ownedDialog->getTextEditorContents("description").trim();
                                                      request.tags.addTokens(ownedDialog->getTextEditorContents("tags"), ",", "\"");
                                                      request.tags.trim();
                                                      request.tags.removeEmptyStrings();
                                                      request.requiredTierId = ownedDialog->getTextEditorContents("tier").trim();
                                                      request.minAppVersion = ownedDialog->getTextEditorContents("minAppVersion").trim();
                                                      request.fileType = selectedFile.getFileExtension().trimCharactersAtStart(".").toLowerCase();
                                                      request.packageFile = selectedFile;

                                                      safeThis->transportBar.setStatusText("Publishing content to LagDaemon...");
                                                      auto token = safeThis->authSession.getSession().token;

                                                      std::thread([safeThis, token, request]()
                                                      {
                                                          juce::String errorMessage;
                                                          juce::String createdId;

                                                          if (! safeThis->contentApiClient.createAdminContent(token, request, createdId, errorMessage)
                                                              || ! safeThis->contentApiClient.uploadAdminContentFile(token, createdId, request.packageFile, errorMessage))
                                                          {
                                                              juce::MessageManager::callAsync([safeThis, errorMessage]
                                                              {
                                                                  if (safeThis != nullptr)
                                                                      safeThis->transportBar.setStatusText(errorMessage);
                                                              });
                                                              return;
                                                          }

                                                          juce::MessageManager::callAsync([safeThis]
                                                          {
                                                              if (safeThis != nullptr)
                                                              {
                                                                  safeThis->transportBar.setStatusText("Content published to LagDaemon.");
                                                                  safeThis->refreshContentLibrary();
                                                              }
                                                          });
                                                      }).detach();
                                                  }), true);
                                          });
    };

    contentPanel.onDownloadRequested = [this](const ContentLibrary::Item& item)
    {
        downloadContentItem(item);
    };

    contentPanel.onRevealItemRequested = [this](const ContentLibrary::Item& item)
    {
        activateContentItem(item);
    };

    contentPanel.onOpenProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        openProjectAsset(asset);
    };

    contentPanel.onPlaceProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        placeProjectAssetOnTracker(asset);
    };

    contentPanel.onExportProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        exportProjectAssetRaw(asset);
    };

    contentPanel.onLaunchTutorialRequested = [this](const ContentPanel::TutorialItem& item)
    {
        launchTutorialItem(item);
    };

    contentPanel.onRevealTutorialRequested = [this](const ContentPanel::TutorialItem& item)
    {
        if (item.file.existsAsFile())
        {
            item.file.revealToUser();
            transportBar.setStatusText("Revealed tutorial: " + item.file.getFileName());
        }
    };

    scorePanel.onPlayRequested = [this](const ScorePanel::PlaybackRequest& request)
    {
        if (request.notes.isEmpty())
        {
            transportBar.setStatusText("Add a few notes to the score first.");
            return;
        }

        auto buffer = renderScorePreviewBuffer(request, 48000.0);
        juce::String errorMessage;

        engine.stopAssetPreview();
        engine.setPlaying(false);

        if (engine.previewGeneratedBuffer(buffer, 48000.0, errorMessage))
        {
            transportBar.setStatusText("Previewing score: " + request.songTitle);
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    pluginRackBar.onLoadPlugin = [this, refreshVisibleBank]
    {
        showPluginLoadMenu([this, refreshVisibleBank](const juce::File& file)
        {
            loadPluginIntoCurrentInsert(file);
            refreshVisibleBank();
        });
    };

    pluginRackBar.onManagePluginPaths = [this]
    {
        setWorkspaceMode(WorkspaceMode::plugins);
    };

    pluginRackBar.onUnloadPlugin = [this, refreshVisibleBank]
    {
        if (pluginRackBar.isTrackContext())
        {
            closePluginEditorWindowsForTrack(pluginRackBar.getTrackIndex());
            engine.unloadTrackPlugin(pluginRackBar.getTrackIndex());
        }
        else
        {
            closePluginEditorWindow("master-plugin");
            engine.unloadMasterPlugin();
        }

        refreshVisibleBank();
    };

    pluginRackBar.onBypassChanged = [this, refreshVisibleBank](bool shouldBypass)
    {
        if (pluginRackBar.isTrackContext())
            engine.setTrackPluginBypassed(pluginRackBar.getTrackIndex(), shouldBypass);
        else
            engine.setMasterPluginBypassed(shouldBypass);

        refreshVisibleBank();
    };

    pluginRackBar.onOpenPluginEditor = [this]
    {
        const auto isTrackContext = pluginRackBar.isTrackContext();
        const auto hasPlugin = isTrackContext ? engine.hasTrackPlugin(pluginRackBar.getTrackIndex())
                                              : engine.hasMasterPlugin();

        if (! hasPlugin)
            return;

        if (isTrackContext)
        {
            auto trackIndex = pluginRackBar.getTrackIndex();
            auto windowKey = "track-rack-" + juce::String(trackIndex);
            if (auto* existingWindow = findPluginEditorWindow(windowKey))
            {
                existingWindow->toFront(true);
                return;
            }

            if (auto* editor = engine.createTrackPluginEditor(trackIndex))
            {
                auto windowTitle = "Track " + juce::String(trackIndex + 1) + " Editor";
                auto window = std::make_unique<ManagedDocumentWindow>(windowTitle,
                                                                      juce::Colour(0xff11151c),
                                                                      juce::DocumentWindow::allButtons,
                                                                      [this, windowKey]
                                                                      {
                                                                          closePluginEditorWindow(windowKey);
                                                                      });
                window->setUsingNativeTitleBar(true);
                window->setResizable(true, true);
                window->setContentOwned(editor, true);
                window->centreWithSize(900, 650);
                window->setVisible(true);
                pluginEditorWindows.push_back({ windowKey, trackIndex, std::move(window) });
                pollPluginEditorReady(windowKey, trackIndex, juce::Component::SafePointer<juce::Component>(editor), 30);
            }
        }
        else
        {
            constexpr auto* windowKey = "master-plugin";
            if (auto* existingWindow = findPluginEditorWindow(windowKey))
            {
                existingWindow->toFront(true);
                return;
            }

            if (auto* editor = engine.createMasterPluginEditor())
            {
                auto window = std::make_unique<ManagedDocumentWindow>("Master Editor",
                                                                      juce::Colour(0xff11151c),
                                                                      juce::DocumentWindow::allButtons,
                                                                      [this, windowKey]
                                                                      {
                                                                          closePluginEditorWindow(windowKey);
                                                                      });
                window->setUsingNativeTitleBar(true);
                window->setResizable(true, true);
                window->setContentOwned(editor, true);
                window->centreWithSize(900, 650);
                window->setVisible(true);
                pluginEditorWindows.push_back({ windowKey, -1, std::move(window) });
            }
        }
    };

    pluginRackBar.onOpenFxStack = [this]
    {
        showFxStackWindow();
    };

    graphPanel.onEnabledChanged = [this](bool shouldEnable)
    {
        engine.setGraphEnabled(shouldEnable);
    };

    graphPanel.onInputChanged = [this](float amount)
    {
        engine.setGraphInput(amount);
    };
    graphPanel.onOscillatorFrequencyChanged = [this](float hz)
    {
        engine.setGraphSourceFrequency(hz);
    };

    graphPanel.onDriveChanged = [this](float amount)
    {
        engine.setGraphDrive(amount);
    };

    graphPanel.onToneChanged = [this](float amount)
    {
        engine.setGraphTone(amount);
    };

    graphPanel.onEchoChanged = [this](float amount)
    {
        engine.setGraphEcho(amount);
    };

    graphPanel.onWidthChanged = [this](float amount)
    {
        engine.setGraphWidth(amount);
    };
    graphPanel.onOutputLevelChanged = [this](float amount)
    {
        engine.setMasterGain(amount);
    };
    graphPanel.onVstMixChanged = [this](float amount)
    {
        engine.setGraphVstMix(amount);
    };
    graphPanel.onVstEnabledChanged = [this](bool shouldEnable)
    {
        engine.setGraphVstEnabled(shouldEnable);
    };
    graphPanel.onNodeDeleted = [this](const juce::String& nodeName)
    {
        if (nodeName == "Oscillator")
        {
            engine.setGraphInput(0.0f);
            graphPanel.setInput(0.0f);
            transportBar.setStatusText("Oscillator removed; source tone muted.");
            return;
        }

        if (nodeName == "VST Host")
        {
            engine.unloadGraphVstPlugin();
            graphPanel.clearAssignedVstPlugin();
            transportBar.setStatusText("VST host removed; plugin unloaded.");
        }
    };
    graphPanel.onAssignVstPluginRequested = [this]
    {
        showPluginLoadMenu([this](const juce::File& file)
        {
            assignPluginToGraphNode(file);
        });
    };
    graphPanel.onOpenAssignedVstRequested = [this]
    {
        if (! engine.hasGraphVstPlugin())
            return;

        constexpr auto* windowKey = "graph-vst";
        if (auto* existingWindow = findPluginEditorWindow(windowKey))
        {
            existingWindow->toFront(true);
            return;
        }

        if (auto* editor = engine.createGraphVstPluginEditor())
        {
            auto window = std::make_unique<ManagedDocumentWindow>("Patch VST Editor",
                                                                  juce::Colour(0xff11151c),
                                                                  juce::DocumentWindow::allButtons,
                                                                  [this, windowKey]
                                                                  {
                                                                      closePluginEditorWindow(windowKey);
                                                                  });
            window->setUsingNativeTitleBar(true);
            window->setResizable(true, true);
            window->setContentOwned(editor, true);
            window->centreWithSize(900, 650);
            window->setVisible(true);
            pluginEditorWindows.push_back({ windowKey, -1, std::move(window) });
        }
    };

    pluginsPanel.onAddPathRequested = [this]
    {
        configureVstSearchPaths();
    };

    pluginsPanel.onImportPathListRequested = [this]
    {
        importVstPathList();
    };

    pluginsPanel.onRemovePathRequested = [this](int pathIndex)
    {
        auto currentPaths = vstPluginCatalog.getSearchPaths();
        if (! juce::isPositiveAndBelow(pathIndex, currentPaths.size()))
            return;

        currentPaths.remove(pathIndex);

        vstPluginCatalog.setSearchPaths(currentPaths);
        saveAppSettings();
        rescanVstCatalog();
        transportBar.setStatusText("Removed VST folder.");
    };

    pluginsPanel.onRescanRequested = [this]
    {
        rescanVstCatalog();
        transportBar.setStatusText(vstPluginCatalog.describeSummary());
    };

    pluginsPanel.onBuildSamplePackRequested = [this]
    {
        setWorkspaceMode(WorkspaceMode::sampler);
    };

    pluginsPanel.onLoadIntoInsertRequested = [this](const VstPluginCatalog::Entry& entry)
    {
        loadPluginIntoCurrentInsert(entry.file);
    };

    pluginsPanel.onAssignNodeRequested = [this](const VstPluginCatalog::Entry& entry)
    {
        assignPluginToGraphNode(entry.file);
    };

    mixerPanel.onGainChanged = [this](int index, float value)
    {
        if (index == 8)
            engine.setMasterGain(value);
        else
        {
            engine.setTrackGain(index, value);
            recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackVolume, value);
        }

        if (index == 8)
            midiSurface.setMasterFaderValue(value);
        else
            midiSurface.setChannelGain(index, value);
    };

    mixerPanel.onBankOffsetChanged = [this, refreshVisibleBank](int)
    {
        refreshVisibleBank();
    };

    mixerPanel.onInsertButtonClicked = [this, refreshVisibleBank](int trackIndex)
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        refreshVisibleBank();
    };

    mixerPanel.onPanChanged = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
        {
            engine.setTrackPan(index, value);
            recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackPan, juce::jmap(value, -1.0f, 1.0f, 0.0f, 1.0f));
        }

        midiSurface.setChannelPan(index, value);
    };

    mixerPanel.onMuteChanged = [this](int index, bool muted)
    {
        if (index < engine.getTrackCount())
            engine.setTrackMuted(index, muted);

        midiSurface.setChannelMuted(index, muted);
    };

    mixerPanel.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < engine.getTrackCount())
            engine.setTrackSoloed(index, soloed);

        midiSurface.setChannelSoloed(index, soloed);
    };

    midiSurface.onBankStep = [this, refreshVisibleBank](int step)
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank, step]
        {
            if (safeThis == nullptr)
                return;

            safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() + step);
            refreshVisibleBank();
        });
    };

    midiSurface.onChannelSelected = [this, selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    midiSurface.onSpecialButtonPressed = [this, selectTrack, refreshVisibleBank](const juce::String& button)
    {
        auto advanceMode = [this](int step)
        {
            auto modeIndex = static_cast<int>(activeMode);
            constexpr int modeCount = static_cast<int>(WorkspaceMode::sampler) + 1;
            modeIndex = (modeIndex + step + modeCount) % modeCount;
            setWorkspaceMode(static_cast<WorkspaceMode>(modeIndex));
        };

        auto setMode = [this](WorkspaceMode mode)
        {
            setWorkspaceMode(mode);
        };

        if (button == creation::ui::surface_actions::cursorLeft)
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() - 1 : mixerPanel.getBankOffset();
            selectTrack(juce::jmax(0, trackIndex));
        }
        else if (button == creation::ui::surface_actions::cursorRight)
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() + 1 : mixerPanel.getBankOffset();
            if (engine.getTrackCount() > 0)
                selectTrack(juce::jlimit(0, engine.getTrackCount() - 1, trackIndex));
        }
        else if (button == creation::ui::surface_actions::cursorUp)
        {
            advanceMode(-1);
        }
        else if (button == creation::ui::surface_actions::cursorDown)
        {
            advanceMode(1);
        }
        else if (button == creation::ui::surface_actions::assignTrack)
        {
            setMode(WorkspaceMode::mix);
        }
        else if (button == creation::ui::surface_actions::assignSend)
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == creation::ui::surface_actions::assignPan)
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == creation::ui::surface_actions::assignPlugin)
        {
            setMode(WorkspaceMode::node);
        }
        else if (button == creation::ui::surface_actions::assignEq)
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == creation::ui::surface_actions::assignInstrument)
        {
            setMode(WorkspaceMode::score);
        }
        else if (button == "global_view")
        {
            setMode(WorkspaceMode::arrange);
        }
        else if (button == "view_midi_tracks")
        {
            setMode(WorkspaceMode::record);
        }
        else if (button == "view_inputs")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "view_audio_tracks")
        {
            setMode(WorkspaceMode::mix);
        }
        else if (button == "view_audio_instrument")
        {
            setMode(WorkspaceMode::node);
        }
        else if (button == "view_aux")
        {
            setMode(WorkspaceMode::library);
        }
        else if (button == "view_busses")
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == "view_outputs")
        {
            setMode(WorkspaceMode::record);
        }
        else if (button == "view_user")
        {
            showAiSidebar();
        }
        else if (button == "f1")
        {
            showProjectMenu();
        }
        else if (button == "f2")
        {
            saveProject();
        }
        else if (button == "f3")
        {
            showAudioSettings();
        }
        else if (button == "f4")
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == "f5")
        {
            setMode(WorkspaceMode::node);
        }
        else if (button == "f6")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "f7")
        {
            setMode(WorkspaceMode::score);
        }
        else if (button == "f8")
        {
            showAiSidebar();
        }
        else if (button == "bank_left_full")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() - safeThis->mixerPanel.getVisibleChannelCount());
                refreshVisibleBank();
            });
        }
        else if (button == "bank_right_full")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() + safeThis->mixerPanel.getVisibleChannelCount());
                refreshVisibleBank();
            });
        }
        else if (button == "channel_left")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() - 1);
                refreshVisibleBank();
            });
        }
        else if (button == "channel_right")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() + 1);
                refreshVisibleBank();
            });
        }
        else if (button == creation::ui::surface_actions::transportCycle)
        {
            transportBar.loopButton.triggerClick();
        }
        else if (button == creation::ui::surface_actions::transportSolo)
        {
            auto selectedChannel = mixerPanel.getSelectedChannel();
            if (selectedChannel < 0)
                selectedChannel = mixerPanel.getBankOffset();

            if (selectedChannel >= 0 && selectedChannel < engine.getTrackCount())
            {
                auto soloed = ! engine.isTrackSoloed(selectedChannel);
                engine.setTrackSoloed(selectedChannel, soloed);
                mixerPanel.setChannelSoloed(selectedChannel, soloed);
                midiSurface.setChannelSoloed(selectedChannel, soloed);
                transportBar.setStatusText("Solo " + juce::String(selectedChannel + 1) + (soloed ? " on" : " off"));
            }
        }
        else if (button == "transport_click")
        {
            transportBar.clickButton.triggerClick();
        }
        else if (button == creation::ui::surface_actions::transportMarker)
        {
            transportBar.setStatusText("Marker pressed.");
        }
        else if (button == creation::ui::surface_actions::transportNudge)
        {
            transportBar.setStatusText("Nudge pressed.");
        }
        else if (button == creation::ui::surface_actions::transportDrop)
        {
            transportBar.setStatusText("Drop pressed.");
        }
        else if (button == creation::ui::surface_actions::transportReplace)
        {
            transportBar.setStatusText("Replace pressed.");
        }
        else if (button == creation::ui::surface_actions::zoom)
        {
            if (pluginRackBar.isTrackContext() && engine.hasTrackPlugin(pluginRackBar.getTrackIndex()))
            {
                auto trackIndex = pluginRackBar.getTrackIndex();
                auto windowKey = "track-rack-" + juce::String(trackIndex);
                if (auto* existingWindow = findPluginEditorWindow(windowKey))
                {
                    existingWindow->toFront(true);
                }
                else if (auto* editor = engine.createTrackPluginEditor(trackIndex))
                {
                    auto window = std::make_unique<ManagedDocumentWindow>("Track Editor",
                                                                          juce::Colour(0xff11151c),
                                                                          juce::DocumentWindow::allButtons,
                                                                          [this, windowKey]
                                                                          {
                                                                              closePluginEditorWindow(windowKey);
                                                                          });
                    window->setUsingNativeTitleBar(true);
                    window->setResizable(true, true);
                    window->setContentOwned(editor, true);
                    window->centreWithSize(900, 650);
                    window->setVisible(true);
                    pluginEditorWindows.push_back({ windowKey, trackIndex, std::move(window) });
                    pollPluginEditorReady(windowKey, trackIndex, juce::Component::SafePointer<juce::Component>(editor), 30);
                }
            }
            else
            {
                setWorkspaceMode(WorkspaceMode::node);
            }
        }
        else if (button == creation::ui::surface_actions::scrub)
        {
            midiScrubModeEnabled = ! midiScrubModeEnabled;
            transportBar.setScrubModeEnabled(midiScrubModeEnabled);
            transportBar.setStatusText(midiScrubModeEnabled
                ? "X-Touch scrub mode armed. Jog wheel hookup is next."
                : "X-Touch scrub mode off.");
        }
        else if (button == creation::ui::surface_actions::userA)
        {
            saveSessionToDisk();
            transportBar.setStatusText("Session saved from X-Touch");
        }
        else if (button == creation::ui::surface_actions::userB)
        {
            loadSessionFromDisk();
            refreshVisibleBank();
            transportBar.setStatusText("Session reloaded from X-Touch");
        }
    };

    midiSurface.onFaderMoved = [this](int index, float value)
    {
        engine.setTrackGain(index, value);

        // Runs on the MIDI input thread, not the message thread - TimelineModel/TrackerPanel
        // mutation must be deferred, same as the mixerPanel update just below.
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), index, value]
        {
            if (safeThis != nullptr)
                safeThis->recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackVolume, value);
        });

        midiSurface.setChannelGain(index, value);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, value]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelGain(index, value);
            });
        }
    };

    midiSurface.onMasterFaderMoved = [this](float value)
    {
        engine.setMasterGain(value);
        mixerPanel.setMasterGain(value);

        auto safeBar = transportBarSafe;
        if (safeBar != nullptr)
        {
            juce::MessageManager::callAsync([safeBar, value]
            {
                if (safeBar != nullptr)
                    safeBar->setStatusText("Master: " + juce::String(value, 2));
            });
        }
    };

    midiSurface.onPanMoved = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
        {
            engine.setTrackPan(index, value);

            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), index, value]
            {
                if (safeThis != nullptr)
                    safeThis->recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackPan,
                                                           juce::jmap(value, -1.0f, 1.0f, 0.0f, 1.0f));
            });
        }

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, value]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelPan(index, value);
            });
        }
    };

    midiSurface.onMuteChanged = [this](int index, bool muted)
    {
        if (index < engine.getTrackCount())
            engine.setTrackMuted(index, muted);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, muted]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelMuted(index, muted);
            });
        }
    };

    midiSurface.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < engine.getTrackCount())
            engine.setTrackSoloed(index, soloed);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, soloed]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelSoloed(index, soloed);
            });
        }
    };

    midiSurface.onJogWheelMoved = [this](int delta)
    {
        if (! midiScrubModeEnabled || delta == 0 || engine.isPlaying())
            return;

        auto stepSeconds = 0.05 * (double) std::abs(delta);
        auto signedStep = delta > 0 ? stepSeconds : -stepSeconds;
        auto newSeconds = juce::jmax(0.0, timelineModel.getTransportSeconds() + signedStep);

        timelineModel.setTransportSeconds(newSeconds);
        engine.setPlaybackPositionSeconds(newSeconds);
        transportStartTimelineSeconds = newSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        trackerPanel.refreshTimelineView();
        previewScrubAudioAt(newSeconds);
    };

    midiSurface.onTransportCommand = [this](XTouchControlSurface::TransportCommand command)
    {
        auto safeBar = transportBarSafe;

        switch (command)
        {
            case XTouchControlSurface::TransportCommand::play:
                // One hardware button drives both directions: if already playing, this same
                // trigger pauses instead of restarting playback from the top.
                if (engine.isPlaying())
                {
                    engine.stopAssetPreview();
                    engine.setPlaying(false);
                    midiSurface.setTransportState(false, false);
                    if (safeBar != nullptr)
                        juce::MessageManager::callAsync([safeBar]
                        {
                            if (safeBar != nullptr)
                            {
                                safeBar->setStatusText("Transport: pause");
                                safeBar->setPlaybackVisualState(false, false);
                            }
                        });
                    break;
                }

                if (! prepareTrackerPlayback())
                    break;

                transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
                transportStartTimelineSeconds = timelineModel.getTransportSeconds();
                engine.setPlaybackPositionSeconds(transportStartTimelineSeconds);
                engine.setPlaying(true);
                midiSurface.setTransportState(true, false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                        {
                            safeBar->setStatusText("Transport: play");
                            safeBar->setPlaybackVisualState(true, false);
                        }
                    });
                break;
            case XTouchControlSurface::TransportCommand::stop:
                stopRecordingSession();
                engine.setPlaying(false);
                midiSurface.setTransportState(false, false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                        {
                            safeBar->setStatusText("Transport: stop");
                            safeBar->setPlaybackVisualState(false, false);
                        }
                    });
                break;
            case XTouchControlSurface::TransportCommand::record:
                if (engine.isRecording() || engine.isMidiRecording())
                {
                    stopRecordingSession();
                }
                else if (startRecordingSession())
                {
                    refreshTrackerPlaybackClips();
                    engine.setPlaying(true);
                    midiSurface.setTransportState(true, true);
                    if (safeBar != nullptr)
                        juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                        {
                            safeBar->setStatusText("Transport: record armed");
                            safeBar->setPlaybackVisualState(true, true);
                        }
                    });
                }
                break;
            case XTouchControlSurface::TransportCommand::rewind:
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->rewindButton.triggerClick();
                    });
                break;
            case XTouchControlSurface::TransportCommand::fastForward:
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->fastForwardButton.triggerClick();
                    });
                break;
        }
    };

    midiSurface.onStatusMessage = [this](juce::String text)
    {
        auto safeBar = transportBarSafe;
        if (safeBar != nullptr)
        {
            juce::MessageManager::callAsync([safeBar, text = std::move(text)]
            {
                if (safeBar != nullptr)
                    safeBar->setMidiStatusText(text);
            });
        }
    };

    midiSurface.attachToDeviceManager(deviceManager);
    midiSurface.setTrackCount(engine.getTrackCount());
    midiSurface.setBankOffset(0);

    mixerPanel.setChannelCount(engine.getTrackCount());
    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        mixerPanel.setChannelName(index, engine.getTrackName(index));
        trackerPanel.setTrackName(index, engine.getTrackName(index));
    }

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        const auto gain = index == 1 ? 0.18f : index == 2 ? 0.22f : 0.12f;
        const auto pan = index == 1 ? -0.15f : index == 2 ? 0.12f : 0.0f;
        mixerPanel.setChannelGain(index, gain);
        mixerPanel.setChannelPan(index, pan);
        engine.setTrackGain(index, gain);
        engine.setTrackPan(index, pan);
    }

    mixerPanel.setMasterGain(0.5f);
    engine.setMasterGain(0.5f);
    graphPanel.setEnabled(engine.isGraphEnabled());
    graphPanel.setInput(engine.getGraphInput());
    graphPanel.setDrive(engine.getGraphDrive());
    graphPanel.setTone(engine.getGraphTone());
    graphPanel.setEcho(engine.getGraphEcho());
    graphPanel.setWidth(engine.getGraphWidth());
    midiSurface.setMasterFaderValue(0.5f);
    engine.setPlaying(false);
    midiSurface.setTransportState(false, false);
    transportBar.setPlaybackVisualState(false, false);

    refreshVisibleBank();
    refreshInsertRack();
    refreshAudioDeviceSettingsView();
    refreshRecentTakes();
    refreshContentLibrary();
    refreshTutorialLibrary();

    configureTutorialOverlay();
    loadLayoutFromDisk();
    reportStartup("Creation Station is ready.", 1.0f);
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
        top->removeKeyListener(this);

    saveLayoutToDisk(true);
    stopTimer();
    pluginEditorWindows.clear();
    for (auto& window : workspacePopoutWindows)
        window.reset();
    midiSurface.detachFromDeviceManager(deviceManager);
    engine.detachFromDevice(deviceManager);
}

void MainComponent::confirmCloseApplication(const std::function<void(bool shouldClose)>& onDecision)
{
    if (! projectDirty)
    {
        if (onDecision)
            onDecision(true);
        return;
    }

    saveSessionToDisk(true);

    if (onDecision)
        onDecision(! projectDirty);
}

void MainComponent::timerCallback()
{
    refreshTrackInputSources();

    pollHostedPluginStateAutosave();

    if (layoutDirty && juce::Time::getMillisecondCounterHiRes() * 0.001 - layoutLastChangeWallSeconds > 0.75)
        saveLayoutToDisk();

    if (midiPlaybackRefreshPending && juce::Time::getMillisecondCounterHiRes() * 0.001 - midiPlaybackRefreshLastChangeWallSeconds > 0.35)
    {
        midiPlaybackRefreshPending = false;
        refreshTrackerPlaybackClips();
    }

    if (engine.isPlaying() || engine.isRecording())
    {
        auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        auto elapsed = juce::jmax(0.0, nowSeconds - transportStartWallSeconds);
        auto timelineSeconds = transportStartTimelineSeconds + elapsed;

        if (engine.isPlaying() && ! engine.isRecording() && timelineModel.isLoopEnabled()
            && timelineModel.getLoopEndSeconds() > timelineModel.getLoopStartSeconds()
            && timelineSeconds >= timelineModel.getLoopEndSeconds())
        {
            timelineSeconds = timelineModel.getLoopStartSeconds();
            transportStartTimelineSeconds = timelineSeconds;
            transportStartWallSeconds = nowSeconds;
            engine.setPlaybackPositionSeconds(timelineSeconds);
        }

        timelineModel.setTransportSeconds(timelineSeconds);

        if (engine.isRecording())
        {
            timelineModel.updateRecordingClip(timelineSeconds);
            for (int index = 0; index < engine.getTrackCount(); ++index)
                if (juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index])
                    timelineModel.addRecordingPeak(index, engine.consumeTrackRecordingPeak(index));
        }

        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
    }

    if (midiEditorPanel != nullptr)
    {
        if (engine.isPlaying() || engine.isRecording())
        {
            auto clipIndex = midiEditorPanel->getEditingClipIndex();
            if (juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
            {
                const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
                auto clipRelativeSeconds = juce::jmax(0.0, timelineModel.getTransportSeconds() - clip.startSeconds);
                midiEditorPanel->setDisplayedTransportSeconds(clipRelativeSeconds, true);
            }
            midiEditorPanel->setPlaybackState(true, engine.isRecording() || engine.isMidiRecording());
        }
        else if (midiEditorPreviewPlaying)
        {
            auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            auto localSeconds = midiEditorPreviewStartLocalSeconds + juce::jmax(0.0, nowSeconds - midiEditorPreviewStartWallSeconds);
            if (localSeconds >= midiEditorPreviewEndLocalSeconds)
            {
                if (midiEditorPreviewLoopEnabled && midiEditorPreviewLoopEndSeconds > midiEditorPreviewLoopStartSeconds)
                {
                    midiEditorPreviewStartLocalSeconds = midiEditorPreviewLoopStartSeconds;
                    midiEditorPreviewStartWallSeconds = nowSeconds;
                    updateMidiEditorPreviewNotes(midiEditorPreviewLoopStartSeconds, true);
                    localSeconds = midiEditorPreviewLoopStartSeconds;
                }
                else
                {
                    stopMidiEditorPreview();
                }
            }

            if (midiEditorPreviewPlaying)
            {
                updateMidiEditorPreviewNotes(localSeconds, false);
                midiEditorPanel->setDisplayedTransportSeconds(localSeconds, false);
                midiEditorPanel->setPlaybackState(true, false);
            }
        }
        else
        {
            midiEditorPanel->setPlaybackState(false, false);
        }
    }

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        trackerPanel.setTrackLevel(index, engine.getTrackLevel(index));

        // Keeps the header gain readout honest while an automation lane is driving this track's
        // volume - otherwise the slider only ever shows the last value the user dragged it to,
        // even while the engine is actually applying a different, automation-driven gain.
        trackerPanel.setTrackGain(index, engine.getTrackGain(index));
    }

    updateAutomationRecordModes();
}

void MainComponent::pollHostedPluginStateAutosave()
{
    auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (nowSeconds - pluginStateLastPollWallSeconds >= 0.5)
    {
        pluginStateLastPollWallSeconds = nowSeconds;
        auto currentSignature = engine.createHostedPluginStateSignature();

        if (lastObservedPluginStateSignature.isEmpty())
        {
            lastObservedPluginStateSignature = currentSignature;
        }
        else if (currentSignature != lastObservedPluginStateSignature)
        {
            lastObservedPluginStateSignature = currentSignature;
            pluginStateAutosavePending = true;
            pluginStateLastChangeWallSeconds = nowSeconds;
            projectDirty = true;
        }
    }

    if (! pluginStateAutosavePending || nowSeconds - pluginStateLastChangeWallSeconds <= 0.9)
        return;

    pluginStateAutosavePending = false;

    if (! projectSession.isValid() || ! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    projectSession.writeEntry("Project/state.xml", juce::MemoryBlock());
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    g.setColour(juce::Colour(0xff1c2230));
    auto area = getLocalBounds().toFloat().reduced(18.0f);
    g.fillRoundedRectangle(area, 24.0f);
    g.setColour(juce::Colour(0xff2a3244));
    g.drawRoundedRectangle(area, 24.0f, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto transportArea = area.removeFromTop(92);
    transportBar.setBounds(transportArea);

    auto pluginArea = area.removeFromTop(48);
    pluginRackBar.setBounds(pluginArea);

    auto modeArea = area.removeFromTop(42);
    viewModeBar.setBounds(modeArea);

    auto aiWidth = aiSidebarCollapsed ? 44 : 420;
    aiWidth = juce::jlimit(44, juce::jmin(560, juce::jmax(44, getWidth() / 2)), aiWidth);
    auto aiArea = area.removeFromRight(aiWidth);
    auto contentArea = area;
    if (! isWorkspacePoppedOut(WorkspaceMode::tracker)) trackerPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::sampler)) samplePackBuilderPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::arrange)) arrangeView.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::signal)) signalLabPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::library)) contentPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::mix)) mixerPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::plugins)) pluginsPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::node)) graphPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::code)) dslPanel.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::record)) recordView.setBounds(contentArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::score)) scorePanel.setBounds(contentArea);
    aiPanel.setBounds(aiArea);
    if (! isWorkspacePoppedOut(WorkspaceMode::settings)) settingsPanel.setBounds(contentArea);
    poppedWorkspacePlaceholder.setBounds(contentArea.reduced(24));
    authGateView.setBounds(getLocalBounds());
    tourOverlay.setBounds(getLocalBounds());
    markLayoutDirty();
}

void MainComponent::setWorkspaceMode(WorkspaceMode mode)
{
    if (activeMode == WorkspaceMode::settings && mode != WorkspaceMode::settings)
        saveAppSettings();

    if (mode == WorkspaceMode::settings)
        refreshMidiDeviceSettings();

    activeMode = mode;
    viewModeBar.setActiveMode(mode);
    refreshModeVisibility();
    markLayoutDirty();
}

void MainComponent::refreshModeVisibility()
{
    transportBar.setVisible(true);
    viewModeBar.setVisible(true);
    pluginRackBar.setVisible(true);
    trackerPanel.setVisible(activeMode == WorkspaceMode::tracker || isWorkspacePoppedOut(WorkspaceMode::tracker));
    samplePackBuilderPanel.setVisible(activeMode == WorkspaceMode::sampler || isWorkspacePoppedOut(WorkspaceMode::sampler));
    arrangeView.setVisible(activeMode == WorkspaceMode::arrange || isWorkspacePoppedOut(WorkspaceMode::arrange));
    signalLabPanel.setVisible(activeMode == WorkspaceMode::signal || isWorkspacePoppedOut(WorkspaceMode::signal));
    contentPanel.setVisible(activeMode == WorkspaceMode::library || isWorkspacePoppedOut(WorkspaceMode::library));
    mixerPanel.setVisible(activeMode == WorkspaceMode::mix || isWorkspacePoppedOut(WorkspaceMode::mix));
    pluginsPanel.setVisible(activeMode == WorkspaceMode::plugins || isWorkspacePoppedOut(WorkspaceMode::plugins));
    graphPanel.setVisible(activeMode == WorkspaceMode::node || isWorkspacePoppedOut(WorkspaceMode::node));
    dslPanel.setVisible(activeMode == WorkspaceMode::code || isWorkspacePoppedOut(WorkspaceMode::code));
    recordView.setVisible(activeMode == WorkspaceMode::record || isWorkspacePoppedOut(WorkspaceMode::record));
    scorePanel.setVisible(activeMode == WorkspaceMode::score || isWorkspacePoppedOut(WorkspaceMode::score));
    settingsPanel.setVisible(activeMode == WorkspaceMode::settings || isWorkspacePoppedOut(WorkspaceMode::settings));
    poppedWorkspacePlaceholder.setVisible(isWorkspacePoppedOut(activeMode));
    aiPanel.setVisible(true);
    authGateView.setVisible(false);
    if (tourOverlay.isActive())
        tourOverlay.toFront(true);
}

juce::ValueTree MainComponent::createLayoutState() const
{
    juce::ValueTree layout("Layout");
    layout.setProperty("format", "creation-station-layout", nullptr);
    layout.setProperty("formatVersion", 1, nullptr);
    layout.setProperty("activeMode", static_cast<int>(activeMode), nullptr);
    layout.setProperty("aiSidebarCollapsed", aiSidebarCollapsed, nullptr);

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        auto bounds = window->getBounds();
        layout.setProperty("mainWindowX", bounds.getX(), nullptr);
        layout.setProperty("mainWindowY", bounds.getY(), nullptr);
        layout.setProperty("mainWindowW", bounds.getWidth(), nullptr);
        layout.setProperty("mainWindowH", bounds.getHeight(), nullptr);
    }

    for (int index = 0; index < workspaceModeCount; ++index)
    {
        auto* window = workspacePopoutWindows[(size_t) index].get();
        if (window == nullptr)
            continue;

        auto bounds = window->getBounds();
        juce::ValueTree popped("PoppedWorkspace");
        popped.setProperty("mode", index, nullptr);
        popped.setProperty("x", bounds.getX(), nullptr);
        popped.setProperty("y", bounds.getY(), nullptr);
        popped.setProperty("w", bounds.getWidth(), nullptr);
        popped.setProperty("h", bounds.getHeight(), nullptr);
        layout.addChild(popped, -1, nullptr);
    }

    return layout;
}

void MainComponent::restoreLayoutState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    auto savedActiveMode = static_cast<WorkspaceMode>(juce::jlimit(0,
                                                                    static_cast<int>(WorkspaceMode::sampler),
                                                                    (int) state.getProperty("activeMode", static_cast<int>(WorkspaceMode::tracker))));
    activeMode = savedActiveMode;
    viewModeBar.setActiveMode(savedActiveMode);

    aiSidebarCollapsed = (bool) state.getProperty("aiSidebarCollapsed", false);
    aiPanel.setCollapsed(aiSidebarCollapsed);

    refreshModeVisibility();

    auto mainX = (int) state.getProperty("mainWindowX", -1);
    auto mainY = (int) state.getProperty("mainWindowY", -1);
    auto mainW = (int) state.getProperty("mainWindowW", -1);
    auto mainH = (int) state.getProperty("mainWindowH", -1);
    if (mainX >= 0 && mainY >= 0 && mainW > 0 && mainH > 0)
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setBounds(mainX, mainY, mainW, mainH);
    }

    for (int index = 0; index < workspaceModeCount; ++index)
    {
        auto child = state.getChildWithProperty("mode", index);
        if (! child.isValid())
            continue;

        auto mode = static_cast<WorkspaceMode>(index);
        auto popX = (int) child.getProperty("x", -1);
        auto popY = (int) child.getProperty("y", -1);
        auto popW = (int) child.getProperty("w", -1);
        auto popH = (int) child.getProperty("h", -1);
        juce::Rectangle<int> bounds(popX, popY, popW, popH);
        popOutWorkspace(mode, bounds.isEmpty() ? nullptr : &bounds);
    }

    layoutDirty = false;
}

void MainComponent::markLayoutDirty()
{
    layoutDirty = true;
    layoutLastChangeWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void MainComponent::saveLayoutToDisk(bool userInitiated)
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    auto layoutFile = suiteSettingsStore.getSuiteConfigDirectory().getChildFile("layout-last-used.json");
    if (layoutFile.getFullPathName().isEmpty())
        return;

    if (! userInitiated && ! layoutDirty)
        return;

    auto state = createLayoutState();
    auto layoutXml = state.createXml();
    if (layoutXml == nullptr)
        return;

    auto tempFile = layoutFile.getSiblingFile(layoutFile.getFileName() + ".tmp");
    if (tempFile.existsAsFile())
        tempFile.deleteFile();

    juce::ZipFile::Builder builder;
    addTextEntry(builder, "layout.xml", layoutXml->toString());

    auto* manifestRoot = new juce::DynamicObject();
    manifestRoot->setProperty("format", "creation-station-layout-package");
    manifestRoot->setProperty("formatVersion", 1);
    manifestRoot->setProperty("layoutName", "Last Used");
    manifestRoot->setProperty("entryCount", 1);
    addTextEntry(builder, "manifest.json", juce::JSON::toString(juce::var(manifestRoot), true));

    std::unique_ptr<juce::FileOutputStream> output(tempFile.createOutputStream());
    if (output == nullptr)
        return;

    double progress = 0.0;
    if (! builder.writeToStream(*output, &progress))
        return;

    output.reset();

    if (layoutFile.existsAsFile())
        layoutFile.deleteFile();

    if (tempFile.moveFileTo(layoutFile))
        layoutDirty = false;
}

void MainComponent::loadLayoutFromDisk()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    auto layoutFile = suiteSettingsStore.getSuiteConfigDirectory().getChildFile("layout-last-used.json");
    if (! layoutFile.existsAsFile())
        return;

    juce::ZipFile zip(layoutFile);
    auto index = zip.getIndexOfFileName("layout.xml");
    if (index < 0)
        return;

    std::unique_ptr<juce::InputStream> input(zip.createStreamForEntry(index));
    if (input == nullptr)
        return;

    auto xml = juce::parseXML(input->readEntireStreamAsString());
    if (xml == nullptr)
        return;

    restoreLayoutState(juce::ValueTree::fromXml(*xml));
}

juce::Component* MainComponent::getWorkspaceComponent(WorkspaceMode mode)
{
    switch (mode)
    {
        case WorkspaceMode::tracker: return &trackerPanel;
        case WorkspaceMode::sampler: return &samplePackBuilderPanel;
        case WorkspaceMode::arrange: return &arrangeView;
        case WorkspaceMode::signal: return &signalLabPanel;
        case WorkspaceMode::library: return &contentPanel;
        case WorkspaceMode::mix: return &mixerPanel;
        case WorkspaceMode::plugins: return &pluginsPanel;
        case WorkspaceMode::node: return &graphPanel;
        case WorkspaceMode::code: return &dslPanel;
        case WorkspaceMode::record: return &recordView;
        case WorkspaceMode::score: return &scorePanel;
        case WorkspaceMode::settings: return &settingsPanel;
    }

    return nullptr;
}

bool MainComponent::isWorkspacePoppedOut(WorkspaceMode mode) const
{
    auto index = workspaceModeIndex(mode);
    return workspacePopoutWindows[(size_t) index] != nullptr;
}

void MainComponent::popOutActiveWorkspace()
{
    popOutWorkspace(activeMode);
}

void MainComponent::popOutWorkspace(WorkspaceMode mode, const juce::Rectangle<int>* bounds)
{
    auto index = workspaceModeIndex(mode);
    auto& windowSlot = workspacePopoutWindows[(size_t) index];

    if (windowSlot != nullptr)
    {
        if (bounds != nullptr && ! bounds->isEmpty())
            windowSlot->setBounds(*bounds);

        windowSlot->toFront(true);
        transportBar.setStatusText(workspaceModeName(mode) + " is already popped out.");
        return;
    }

    auto* component = getWorkspaceComponent(mode);
    if (component == nullptr)
        return;

    poppedWorkspacePlaceholder.setText(workspaceModeName(mode) + " is open in its own window.\nClose that window to dock it back here.",
                                       juce::dontSendNotification);

    auto window = std::make_unique<ManagedDocumentWindow>("Creation Station - " + workspaceModeName(mode),
                                                          juce::Colour(0xff10141a),
                                                          juce::DocumentWindow::closeButton
                                                              | juce::DocumentWindow::minimiseButton
                                                              | juce::DocumentWindow::maximiseButton,
                                                          [this, mode]
                                                          {
                                                              dockWorkspace(mode);
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentNonOwned(component, false);

    if (bounds != nullptr && ! bounds->isEmpty())
        window->setBounds(*bounds);
    else
        window->centreWithSize(1180, 760);

    window->setVisible(true);
    window->toFront(true);
    windowSlot = std::move(window);

    refreshModeVisibility();
    resized();
    transportBar.setStatusText("Popped out " + workspaceModeName(mode) + ".");
    markLayoutDirty();
}

void MainComponent::dockWorkspace(WorkspaceMode mode)
{
    auto index = workspaceModeIndex(mode);
    auto& windowSlot = workspacePopoutWindows[(size_t) index];
    if (windowSlot == nullptr)
        return;

    auto* component = getWorkspaceComponent(mode);
    if (windowSlot != nullptr)
    {
        windowSlot->clearContentComponent();
        windowSlot.reset();
    }

    if (component != nullptr)
        addAndMakeVisible(component);

    if (activeMode == mode)
        poppedWorkspacePlaceholder.setVisible(false);

    setWorkspaceMode(mode);
    resized();
    transportBar.setStatusText("Docked " + workspaceModeName(mode) + ".");
    markLayoutDirty();
}

void MainComponent::refreshAuthState()
{
    refreshModeVisibility();
    contentPanel.setAuthState(authenticated, authenticated && isAdminRole(authSession.getSession().user.role));

    if (authenticated)
    {
        const auto& session = authSession.getSession();
        transportBar.setProfile(makeHeaderProfile(session));
        transportBar.setStatusText("Signed in. Welcome back.");
        syncSemanticAppContext();
    }
    else
    {
        transportBar.clearProfile();
        transportBar.setStatusText("Ready. Sign in from the top-right when you want sync.");
        appContextSyncInProgress = false;
    }
}

void MainComponent::openLagDaemonProfile()
{
    juce::URL("https://lagdaemon.com/profile").launchInDefaultBrowser();
}

void MainComponent::showAudioSettings()
{
    if (audioDeviceWindow != nullptr)
    {
        audioDeviceWindow->toFront(true);
        return;
    }

    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,
                                                                         0, 2,
                                                                         0, 2,
                                                                         true, true, true, false);

    auto window = std::make_unique<ManagedDocumentWindow>("Audio Devices",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              audioDeviceWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(false);
    window->setResizable(true, true);
    window->setContentOwned(selector.release(), true);
    window->centreWithSize(720, 540);
    window->setVisible(true);
    audioDeviceWindow = std::move(window);
}

void MainComponent::showFxStackWindow()
{
    if (! pluginRackBar.isTrackContext())
    {
        transportBar.setStatusText("Select a track to edit its FX stack.");
        return;
    }

    if (fxStackWindow != nullptr)
    {
        fxStackWindow->toFront(true);
        refreshFxStackWindow();
        return;
    }

    auto panel = std::make_unique<FxStackPanel>();
    fxStackPanel = panel.get();

    panel->onAddPlugin = [this](const VstPluginCatalog::Entry& entry)
    {
        const auto trackIndex = pluginRackBar.getTrackIndex();
        juce::String errorMessage;
        if (! engine.loadTrackPlugin(trackIndex, entry.file, errorMessage))
            transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not add plugin.");

        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onInsertPlugin = [this](int slotIndex, const VstPluginCatalog::Entry& entry)
    {
        const auto trackIndex = pluginRackBar.getTrackIndex();
        juce::String errorMessage;
        if (! engine.insertTrackPlugin(trackIndex, slotIndex, entry.file, errorMessage))
            transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not insert plugin.");

        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onRemovePlugin = [this](int slotIndex)
    {
        const auto trackIndex = pluginRackBar.getTrackIndex();

        // Close any open editors for this track before unloading - otherwise their editor
        // components would outlive the processors they belong to.
        closePluginEditorWindowsForTrack(trackIndex);

        engine.unloadTrackPlugin(trackIndex, slotIndex);
        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onMovePlugin = [this](int fromSlot, int toSlot)
    {
        if (engine.moveTrackPlugin(pluginRackBar.getTrackIndex(), fromSlot, toSlot))
        {
            refreshInsertRack();
            syncTrackViews();
            projectDirty = true;
        }
    };

    panel->onBypassChanged = [this](int slotIndex, bool shouldBypass)
    {
        engine.setTrackPluginBypassed(pluginRackBar.getTrackIndex(), slotIndex, shouldBypass);
        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onOpenPluginEditor = [this](int slotIndex)
    {
        openTrackPluginEditor(pluginRackBar.getTrackIndex(), slotIndex);
    };

    panel->onRescanRequested = [this]
    {
        rescanVstCatalog();
    };

    panel->setCatalog(vstPluginCatalog.getEntries());

    auto window = std::make_unique<ManagedDocumentWindow>("Creation Station - Track FX Stack",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              fxStackPanel = nullptr;
                                                              fxStackWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setResizeLimits(760, 420, 1600, 1000);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(1000, 560);
    window->setVisible(true);
    fxStackWindow = std::move(window);
    refreshFxStackWindow();
}

void MainComponent::showMidiEditorWindow(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return;

    if (midiEditorWindow != nullptr)
    {
        if (midiEditorPanel != nullptr)
            midiEditorPanel->setClip(&timelineModel, clipIndex);
        midiEditorWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<MidiEditorPanel>();
    midiEditorPanel = panel.get();

    panel->onNotesChanged = [this]
    {
        projectDirty = true;
        saveSessionToDisk();

        // Re-rendering a MIDI clip through its instrument plugin is too slow to do on every
        // single drag step (each call loads a fresh plugin instance) - coalesce rapid edits
        // and refresh once the user pauses, via the existing 30Hz UI timer below.
        midiPlaybackRefreshPending = true;
        midiPlaybackRefreshLastChangeWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    };

    panel->onCloseRequested = [this]
    {
        midiEditorPanel = nullptr;
        midiEditorWindow.reset();
    };

    panel->onPlayRequested = [this]
    {
        startMidiEditorPreview();
    };

    panel->onStopRequested = [this]
    {
        stopMidiEditorPreview();
    };

    panel->onLoopEnabledChanged = [this](bool enabled)
    {
        midiEditorPreviewLoopEnabled = enabled;
    };

    panel->onTransportChanged = [this](double seconds)
    {
        midiEditorPreviewStartLocalSeconds = juce::jmax(0.0, seconds);
        if (midiEditorPanel != nullptr)
            midiEditorPanel->setDisplayedTransportSeconds(midiEditorPreviewStartLocalSeconds, false);
    };

    panel->onLoopRegionChanged = [this](double startSeconds, double endSeconds)
    {
        midiEditorPreviewLoopStartSeconds = juce::jmax(0.0, startSeconds);
        midiEditorPreviewLoopEndSeconds = juce::jmax(midiEditorPreviewLoopStartSeconds, endSeconds);
        midiEditorPreviewLoopEnabled = midiEditorPreviewLoopEndSeconds > midiEditorPreviewLoopStartSeconds;
    };

    panel->onLoopRegionCleared = [this]
    {
        midiEditorPreviewLoopEnabled = false;
        midiEditorPreviewLoopStartSeconds = 0.0;
        midiEditorPreviewLoopEndSeconds = 0.0;
    };

    panel->onAuditionNote = [this](int pitch, int velocity, bool isOn)
    {
        if (midiEditorPanel == nullptr)
            return;

        auto editingClipIndex = midiEditorPanel->getEditingClipIndex();
        if (! juce::isPositiveAndBelow(editingClipIndex, (int) timelineModel.getClips().size()))
            return;

        auto trackIndex = timelineModel.getClips()[(size_t) editingClipIndex].trackIndex;
        if (isOn)
            engine.auditionNoteOn(trackIndex, pitch, velocity);
        else
            engine.auditionNoteOff(trackIndex, pitch);
    };

    panel->setClip(&timelineModel, clipIndex);
    panel->setPlaybackState(engine.isPlaying(), engine.isRecording() || engine.isMidiRecording());
    panel->setDisplayedTransportSeconds(0.0, false);

    auto window = std::make_unique<ManagedDocumentWindow>("Creation Station - MIDI Editor",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              midiEditorPanel = nullptr;
                                                              midiEditorWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setResizeLimits(820, 470, 1800, 1100);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(1100, 660);
    window->setVisible(true);
    midiEditorWindow = std::move(window);
}

bool MainComponent::startMidiEditorPreview()
{
    if (midiEditorPanel == nullptr)
        return false;

    const auto clipIndex = midiEditorPanel->getEditingClipIndex();
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return false;

    const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
    if (clip.kind != cs::ClipKind::midi || clip.midiNotes.empty())
    {
        transportBar.setStatusText("This MIDI clip has no notes to preview.");
        return false;
    }

    if (! juce::isPositiveAndBelow(clip.trackIndex, engine.getTrackCount()))
    {
        transportBar.setStatusText("That MIDI clip is not assigned to a valid track.");
        return false;
    }

    auto instrumentPluginFile = engine.getTrackInstrumentPluginFile(clip.trackIndex);
    if (! instrumentPluginFile.existsAsFile())
    {
        transportBar.setStatusText("Load an instrument on this track to preview the MIDI clip.");
        return false;
    }

    auto localStart = juce::jlimit(0.0, clip.durationSeconds, midiEditorPanel->getLocalTransportSeconds());
    auto previewEnd = clip.durationSeconds;
    if (midiEditorPreviewLoopEnabled && midiEditorPreviewLoopEndSeconds > midiEditorPreviewLoopStartSeconds)
    {
        localStart = juce::jlimit(midiEditorPreviewLoopStartSeconds, midiEditorPreviewLoopEndSeconds, localStart);
        previewEnd = midiEditorPreviewLoopEndSeconds;
    }

    releaseMidiEditorPreviewNotes();
    midiEditorPreviewPlaying = true;
    midiEditorPreviewStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    midiEditorPreviewStartLocalSeconds = localStart;
    midiEditorPreviewLastLocalSeconds = localStart;
    midiEditorPreviewEndLocalSeconds = previewEnd;
    if (midiEditorPanel != nullptr)
    {
        midiEditorPanel->setDisplayedTransportSeconds(localStart, false);
        midiEditorPanel->setPlaybackState(true, false);
    }
    updateMidiEditorPreviewNotes(localStart, true);
    transportBar.setStatusText("Previewing MIDI clip inside the editor.");
    return true;
}

void MainComponent::stopMidiEditorPreview(bool resetPlayheadToLoopStart)
{
    releaseMidiEditorPreviewNotes();

    midiEditorPreviewPlaying = false;
    auto nextLocalSeconds = midiEditorPreviewStartLocalSeconds;
    if (resetPlayheadToLoopStart && midiEditorPreviewLoopEnabled)
        nextLocalSeconds = midiEditorPreviewLoopStartSeconds;

    if (midiEditorPanel != nullptr)
    {
        midiEditorPanel->setDisplayedTransportSeconds(nextLocalSeconds, false);
        midiEditorPanel->setPlaybackState(engine.isPlaying(), engine.isRecording() || engine.isMidiRecording());
    }
}

void MainComponent::updateMidiEditorPreviewNotes(double currentLocalSeconds, bool restartCycle)
{
    if (midiEditorPanel == nullptr)
        return;

    const auto clipIndex = midiEditorPanel->getEditingClipIndex();
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return;

    const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
    if (! juce::isPositiveAndBelow(clip.trackIndex, engine.getTrackCount()))
        return;

    if (restartCycle)
        releaseMidiEditorPreviewNotes();

    for (const auto& note : clip.midiNotes)
    {
        const auto noteStartSeconds = timelineModel.beatToSeconds(note.startBeats);
        const auto noteEndSeconds = timelineModel.beatToSeconds(note.startBeats + note.lengthBeats);
        const bool shouldBeActive = currentLocalSeconds >= noteStartSeconds && currentLocalSeconds < noteEndSeconds;
        const bool isActive = midiEditorPreviewActiveNoteIds.contains(note.id);

        if (shouldBeActive && ! isActive)
        {
            engine.auditionNoteOn(clip.trackIndex, note.pitch, note.velocity);
            midiEditorPreviewActiveNoteIds.addIfNotAlreadyThere(note.id);
        }
        else if (! shouldBeActive && isActive)
        {
            engine.auditionNoteOff(clip.trackIndex, note.pitch);
            midiEditorPreviewActiveNoteIds.removeString(note.id);
        }
    }

    midiEditorPreviewLastLocalSeconds = currentLocalSeconds;
}

void MainComponent::releaseMidiEditorPreviewNotes()
{
    if (midiEditorPanel != nullptr)
    {
        const auto clipIndex = midiEditorPanel->getEditingClipIndex();
        if (juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        {
            const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
            if (juce::isPositiveAndBelow(clip.trackIndex, engine.getTrackCount()))
            {
                for (const auto& note : clip.midiNotes)
                {
                    if (midiEditorPreviewActiveNoteIds.contains(note.id))
                        engine.auditionNoteOff(clip.trackIndex, note.pitch);
                }
            }
        }
    }

    midiEditorPreviewActiveNoteIds.clear();
    engine.requestAllNotesOff();
}

void MainComponent::refreshFxStackWindow()
{
    if (fxStackPanel == nullptr || ! pluginRackBar.isTrackContext())
        return;

    const auto trackIndex = pluginRackBar.getTrackIndex();
    fxStackPanel->setTrackName("Track " + juce::String(trackIndex + 1) + " - " + engine.getTrackName(trackIndex));
    fxStackPanel->setPlugins(engine.getTrackPluginNames(trackIndex),
                             engine.getTrackPluginBypassStates(trackIndex));
}

juce::DocumentWindow* MainComponent::findPluginEditorWindow(const juce::String& key) const
{
    auto it = std::find_if(pluginEditorWindows.begin(), pluginEditorWindows.end(),
                           [&key](const PluginEditorWindowEntry& entry)
                           {
                               return entry.key == key && entry.window != nullptr;
                           });
    return it != pluginEditorWindows.end() ? it->window.get() : nullptr;
}

void MainComponent::refreshTrackPluginEditorState(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto hasOpenEditor = std::any_of(pluginEditorWindows.begin(), pluginEditorWindows.end(),
                                     [trackIndex](const PluginEditorWindowEntry& entry)
                                     {
                                         return entry.trackIndex == trackIndex && entry.window != nullptr;
                                     });
    engine.setTrackHasOpenEditor(trackIndex, hasOpenEditor);
}

void MainComponent::closePluginEditorWindow(const juce::String& key)
{
    auto it = std::find_if(pluginEditorWindows.begin(), pluginEditorWindows.end(),
                           [&key](const PluginEditorWindowEntry& entry)
                           {
                               return entry.key == key;
                           });
    if (it == pluginEditorWindows.end())
        return;

    auto trackIndex = it->trackIndex;
    it->window.reset();
    pluginEditorWindows.erase(it);
    if (trackIndex >= 0)
        refreshTrackPluginEditorState(trackIndex);
}

void MainComponent::closePluginEditorWindowsForTrack(int trackIndex)
{
    for (auto it = pluginEditorWindows.begin(); it != pluginEditorWindows.end();)
    {
        if (it->trackIndex == trackIndex)
        {
            it->window.reset();
            it = pluginEditorWindows.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (trackIndex >= 0)
        refreshTrackPluginEditorState(trackIndex);
}

void MainComponent::openTrackPluginEditor(int trackIndex, int slotIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto windowKey = "track-plugin-" + juce::String(trackIndex) + "-" + juce::String(slotIndex);
    if (auto* existingWindow = findPluginEditorWindow(windowKey))
    {
        existingWindow->toFront(true);
        return;
    }

    auto* editor = engine.createTrackPluginEditor(trackIndex, slotIndex);
    if (editor == nullptr)
        return;

    auto pluginNames = engine.getTrackPluginNames(trackIndex);
    auto pluginName = juce::isPositiveAndBelow(slotIndex, pluginNames.size()) ? pluginNames[slotIndex]
                                                                              : "Plugin";
    auto window = std::make_unique<ManagedDocumentWindow>("Track " + juce::String(trackIndex + 1) + " - " + pluginName,
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this, windowKey]
                                                          {
                                                              closePluginEditorWindow(windowKey);
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(editor, true);
    window->centreWithSize(900, 650);
    window->setVisible(true);
    pluginEditorWindows.push_back({ windowKey, trackIndex, std::move(window) });

    engine.reapplyTrackPluginState(trackIndex, slotIndex);
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::Timer::callAfterDelay(450, [safeThis, trackIndex, slotIndex]
    {
        if (safeThis != nullptr)
            safeThis->engine.reapplyTrackPluginState(trackIndex, slotIndex);
    });

    // Don't engage the live audio path (and start calling processBlock from the audio thread)
    // until the editor is actually showing with a real size, not just after a guessed delay - a
    // fixed delay that's fine for a light editor (e.g. TAL-NoiseMaker) can still be too short for
    // a heavier one (e.g. a sampler loading kit graphics), and the resulting race is intermittent
    // rather than a hard crash every time, which matches what was observed.
    pollPluginEditorReady(windowKey, trackIndex, juce::Component::SafePointer<juce::Component>(editor), 30);
}

void MainComponent::pollPluginEditorReady(const juce::String& windowKey,
                                          int trackIndex,
                                          juce::Component::SafePointer<juce::Component> editorPointer,
                                          int attemptsRemaining)
{
    if (findPluginEditorWindow(windowKey) == nullptr)
        return; // Editor was closed or replaced before it became ready - nothing to engage.

    auto isReady = editorPointer != nullptr && editorPointer->isShowing()
                  && editorPointer->getWidth() > 0 && editorPointer->getHeight() > 0;

    if (isReady || attemptsRemaining <= 0)
    {
        engine.setTrackHasOpenEditor(trackIndex, true);

        auto slotText = windowKey.fromLastOccurrenceOf("-", false, false);
        auto slotIndex = slotText.getIntValue();
        if (slotIndex >= 0)
        {
            engine.reapplyTrackPluginState(trackIndex, slotIndex);

            auto safeThis = juce::Component::SafePointer<MainComponent>(this);
            juce::Timer::callAfterDelay(900, [safeThis, trackIndex, slotIndex]
            {
                if (safeThis != nullptr)
                    safeThis->engine.reapplyTrackPluginState(trackIndex, slotIndex);
            });

            juce::Timer::callAfterDelay(2200, [safeThis, trackIndex, slotIndex]
            {
                if (safeThis != nullptr)
                    safeThis->engine.reapplyTrackPluginState(trackIndex, slotIndex);
            });
        }

        return;
    }

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(150, [safeThis, windowKey, trackIndex, editorPointer, attemptsRemaining]
    {
        if (safeThis != nullptr)
            safeThis->pollPluginEditorReady(windowKey, trackIndex, editorPointer, attemptsRemaining - 1);
    });
}

void MainComponent::configureVstSearchPaths()
{
    if (! ensureStorageRootConfigured())
        return;

    auto currentPaths = vstPluginCatalog.getSearchPaths();
    pluginChooser = std::make_unique<juce::FileChooser>("Choose a VST folder",
                                                        currentPaths.isEmpty() ? juce::File{} : juce::File(currentPaths[0]),
                                                        juce::String{},
                                                        true);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    pluginChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                               [safeThis, currentPaths](const juce::FileChooser& chooser) mutable
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   auto selectedDirectory = chooser.getResult();
                                   safeThis->pluginChooser.reset();

                                   if (! selectedDirectory.isDirectory())
                                       return;

                                   auto updatedPaths = currentPaths;
                                   updatedPaths.addIfNotAlreadyThere(selectedDirectory.getFullPathName());
                                   updatedPaths.trim();
                                   updatedPaths.removeEmptyStrings();
                                   updatedPaths.removeDuplicates(false);

                                   safeThis->vstPluginCatalog.setSearchPaths(updatedPaths);
                                   safeThis->saveAppSettings();
                                   safeThis->rescanVstCatalog();
                                   safeThis->transportBar.setStatusText("Added VST folder: " + selectedDirectory.getFileName());
                               });
}

juce::StringArray MainComponent::parseVstPathList(const juce::String& rawList)
{
    juce::StringArray result;
    auto tokens = juce::StringArray::fromTokens(rawList, ";", "");

    for (auto token : tokens)
    {
        token = token.trim();
        if (token.isEmpty())
            continue;

        // Expand %ENV_VAR% style references (e.g. %LOCALAPPDATA%, %PROGRAMFILES%).
        for (int guard = 0; guard < 8 && token.contains("%"); ++guard)
        {
            auto start = token.indexOfChar('%');
            auto end = token.indexOfChar(start + 1, '%');
            if (end < 0)
                break;

            auto varName = token.substring(start + 1, end);
            auto varValue = juce::SystemStats::getEnvironmentVariable(varName, {});
            if (varValue.isEmpty())
                break;

            token = token.substring(0, start) + varValue + token.substring(end + 1);
        }

        // Don't require the folder to exist yet - a dev project's VST3 output folder (e.g.
        // "...\Builds\VisualStudio2022\x64\Debug\VST3") may not exist until it's been built once,
        // and the scanner already skips missing search paths gracefully every rescan, so
        // registering it now means it starts working the moment it does exist.
        result.add(juce::File(token).getFullPathName());
    }

    return result;
}

void MainComponent::importVstPathList()
{
    if (! ensureStorageRootConfigured())
        return;

    auto* alertWindow = new juce::AlertWindow("Import VST Path List",
                                              "Paste a semicolon-separated list of VST folders (e.g. copied from Reaper's VST path setting). "
                                              "Windows %ENV_VAR% references are expanded automatically.",
                                              juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("pathList", "", "Path list");
    alertWindow->addButton("Import", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alertWindow](int result)
    {
        if (safeThis == nullptr || result != 1)
            return;

        auto rawList = alertWindow->getTextEditorContents("pathList");
        auto parsedPaths = parseVstPathList(rawList);

        if (parsedPaths.isEmpty())
        {
            safeThis->transportBar.setStatusText("No valid folders found in the pasted list.");
            return;
        }

        auto updatedPaths = safeThis->vstPluginCatalog.getSearchPaths();
        for (const auto& path : parsedPaths)
            updatedPaths.addIfNotAlreadyThere(path);
        updatedPaths.trim();
        updatedPaths.removeEmptyStrings();
        updatedPaths.removeDuplicates(false);

        safeThis->vstPluginCatalog.setSearchPaths(updatedPaths);
        safeThis->saveAppSettings();
        safeThis->rescanVstCatalog();
        safeThis->transportBar.setStatusText("Imported " + juce::String(parsedPaths.size()) + " VST folder(s).");
    }), true);
}

void MainComponent::editControlSurfaceMappings()
{
    if (! ensureStorageRootConfigured())
        return;

    auto mappingsFile = suiteSettingsStore.getSuiteConfigDirectory().getChildFile("control-surface-mappings.json");
    ControlSurfaceMappingStore mappings;
    juce::String errorMessage;

    if (! controlSurfaceMappings.loadFromFile(suiteSettingsStore.getSuiteConfigDirectory().getChildFile("control-surface-mappings.json"), errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    if (mappings.getProfiles().isEmpty())
    {
        mappings = ControlSurfaceMappingStore::createDefaultLibrary();

        if (! controlSurfaceMappings.saveToFile(suiteSettingsStore.getSuiteConfigDirectory().getChildFile("control-surface-mappings.json"), errorMessage))
        {
            transportBar.setStatusText(errorMessage);
            return;
        }
    }

    if (! mappingsFile.startAsProcess())
        mappingsFile.revealToUser();

    transportBar.setStatusText("Opened control surface mappings.");
}

void MainComponent::showMidiLearnDialog(const juce::String& targetId, const juce::String& displayLabel)
{
    if (midiLearnWindow != nullptr)
    {
        midiLearnWindow->toFront(true);
        return;
    }

    MidiLearnPanel::ExistingBinding existing;
    for (const auto& profile : controlSurfaceMappings.getProfiles())
    {
        auto found = false;
        for (const auto& binding : profile.bindings)
        {
            if (binding.targetId != targetId)
                continue;

            existing.found = true;
            existing.deviceLabel = profile.devicePattern.isNotEmpty() ? profile.devicePattern : "any device";
            existing.channel = binding.channel;
            existing.number = binding.number;
            existing.isController = binding.isController;
            found = true;
            break;
        }
        if (found)
            break;
    }

    auto panel = std::make_unique<MidiLearnPanel>(engine, targetId, displayLabel, existing);
    auto* panelPtr = panel.get();

    auto window = std::make_unique<ManagedDocumentWindow>("Learn MIDI Binding",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              midiLearnWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(false, false);

    // The panel stays open after a capture so the user can see what was saved - only Close (or
    // the window's own close control) dismisses it. Deferred via callAsync since onCancelled
    // fires from inside a MidiLearnPanel button click, and resetting midiLearnWindow
    // synchronously would destroy the panel - and this lambda - while still on the call stack.
    panelPtr->onLearned = [this, targetId](juce::String deviceId, int channel, int number, bool isCC)
    {
        applyLearnedMidiBinding(targetId, deviceId, channel, number, isCC);
    };
    panelPtr->onCancelled = [this]
    {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->midiLearnWindow.reset();
        });
    };

    window->setContentOwned(panel.release(), true);
    window->centreWithSize(460, 400);
    window->setVisible(true);
    midiLearnWindow = std::move(window);
}

void MainComponent::applyLearnedMidiBinding(const juce::String& targetId, const juce::String& deviceId, int channel, int number, bool isCC)
{
    juce::String deviceName = "*";
    if (deviceId.isNotEmpty())
    {
        for (const auto& device : juce::MidiInput::getAvailableDevices())
        {
            if (device.identifier == deviceId)
            {
                deviceName = device.name;
                break;
            }
        }
    }

    auto* profile = controlSurfaceMappings.findProfileById("custom-bindings");
    if (profile == nullptr)
    {
        ControlSurfaceMappingStore::Profile newProfile;
        newProfile.id = "custom-bindings";
        newProfile.displayName = "Custom Bindings";
        newProfile.devicePattern = deviceName;
        newProfile.usage = "*";
        newProfile.description = "Bindings created via right-click -> Learn MIDI Binding.";
        controlSurfaceMappings.addProfile(std::move(newProfile));
        profile = controlSurfaceMappings.findProfileById("custom-bindings");
    }
    else if (deviceName != "*" && ! profile->matchesDevice(deviceName))
    {
        profile->devicePattern += "," + deviceName;
    }

    if (profile == nullptr)
        return;

    // Only replace an exact duplicate (same target + same channel/number, i.e. re-learning the
    // identical control) - a DIFFERENT device or control bound to the same targetId is meant to
    // coexist, so more than one piece of hardware can trigger the same action.
    for (int i = profile->bindings.size(); --i >= 0;)
    {
        const auto& existing = profile->bindings.getReference(i);
        if (existing.targetId == targetId && existing.channel == channel && existing.number == number)
            profile->bindings.remove(i);
    }

    ControlSurfaceMappingStore::Binding binding;
    binding.triggerType = "transport";
    binding.actionId = targetId.fromFirstOccurrenceOf("transport_", false, false);
    binding.targetId = targetId;
    binding.behavior = "momentary";
    binding.channel = channel;
    binding.number = number;
    binding.isController = isCC;
    profile->bindings.add(binding);

    juce::String saveError;
    if (! controlSurfaceMappings.saveToFile(suiteSettingsStore.getSuiteConfigDirectory().getChildFile("control-surface-mappings.json"), saveError))
    {
        transportBar.setStatusText("Could not save MIDI binding: " + saveError);
        return;
    }

    midiSurface.setControlSurfaceMappings(controlSurfaceMappings);
    transportBar.setStatusText("Learned MIDI binding for " + targetId
                               + (deviceName != "*" ? (" on " + deviceName) : "") + ".");
}

void MainComponent::rescanVstCatalog()
{
    vstPluginCatalog.rescan();
    pluginRackBar.setCatalogSummary(vstPluginCatalog.describeSummary());
    refreshPluginsPanel();

    if (fxStackPanel != nullptr)
        fxStackPanel->setCatalog(vstPluginCatalog.getEntries());
}

void MainComponent::showPluginLoadMenu(const std::function<void(const juce::File&)>& onPluginChosen)
{
    auto entries = vstPluginCatalog.getEntries();
    if (entries.isEmpty())
    {
        pluginChooser = std::make_unique<juce::FileChooser>("Load a VST3 plugin", juce::File{}, "*.vst3");
        pluginChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this, onPluginChosen](const juce::FileChooser& chooser)
                                   {
                                       auto file = chooser.getResult();
                                       pluginChooser.reset();
                                       if (file.exists())
                                           onPluginChosen(file);
                                   });
        return;
    }

    juce::PopupMenu menu;
    menu.addItem(1, "Rescan VST folders");
    menu.addItem(2, "Manage VST folders...");
    menu.addSeparator();

    for (int index = 0; index < entries.size(); ++index)
        menu.addItem(100 + index, entries.getReference(index).name);

    menu.addSeparator();
    menu.addItem(1000, "Browse manually...");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pluginRackBar),
                       [this, onPluginChosen, entries](int result)
                       {
                           if (result == 1)
                           {
                               rescanVstCatalog();
                               transportBar.setStatusText(vstPluginCatalog.describeSummary());
                               return;
                           }

                           if (result == 2)
                           {
                               configureVstSearchPaths();
                               return;
                           }

                           if (result == 1000)
                           {
                               pluginChooser = std::make_unique<juce::FileChooser>("Load a VST3 plugin", juce::File{}, "*.vst3");
                               pluginChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                          [this, onPluginChosen](const juce::FileChooser& chooser)
                                                          {
                                                              auto file = chooser.getResult();
                                                              pluginChooser.reset();
                                                              if (file.exists())
                                                                  onPluginChosen(file);
                                                          });
                               return;
                           }

                           if (result >= 100 && result < 100 + entries.size())
                               onPluginChosen(entries.getReference(result - 100).file);
                       });
}

void MainComponent::refreshPluginsPanel()
{
    pluginsPanel.setSearchPaths(vstPluginCatalog.getSearchPaths());
    pluginsPanel.setPlugins(vstPluginCatalog.getEntries());
    pluginsPanel.setStatusText(vstPluginCatalog.describeSummary());

    if (pluginRackBar.isTrackContext())
        pluginsPanel.setInsertTargetDescription("Current insert target: Track " + juce::String(pluginRackBar.getTrackIndex() + 1)
                                                + " - " + engine.getTrackName(pluginRackBar.getTrackIndex()));
    else
        pluginsPanel.setInsertTargetDescription("Current insert target: Master");
}

void MainComponent::loadPluginIntoCurrentInsert(const juce::File& file)
{
    if (! file.existsAsFile() && ! file.isDirectory())
        return;

    if (pluginRackBar.isTrackContext())
        closePluginEditorWindowsForTrack(pluginRackBar.getTrackIndex());
    else
        closePluginEditorWindow("master-plugin");

    juce::String errorMessage;
    auto loaded = pluginRackBar.isTrackContext()
        ? engine.loadTrackPlugin(pluginRackBar.getTrackIndex(), file, errorMessage)
        : engine.loadMasterPlugin(file, errorMessage);

    if (loaded)
    {
        refreshInsertRack();
        refreshPluginsPanel();
        transportBar.setStatusText("Loaded plugin: " + file.getFileNameWithoutExtension());
    }
    else
    {
        transportBar.setStatusText("Plugin load failed: " + errorMessage);
        pluginsPanel.setStatusText("Plugin load failed: " + errorMessage);
    }
}

void MainComponent::assignPluginToGraphNode(const juce::File& file)
{
    juce::String errorMessage;
    if (! engine.loadGraphVstPlugin(file, errorMessage))
    {
        transportBar.setStatusText("Graph VST load failed: " + errorMessage);
        pluginsPanel.setStatusText("Graph VST load failed: " + errorMessage);
        return;
    }

    graphPanel.setAssignedVstPlugin(engine.getGraphVstPluginName().isNotEmpty() ? engine.getGraphVstPluginName()
                                                                                : file.getFileNameWithoutExtension(),
                                    file.getFullPathName());
    engine.setGraphVstEnabled(graphPanel.isVstEnabled());
    engine.setGraphVstMix(graphPanel.getVstMix());
    transportBar.setStatusText("Assigned VST node: " + file.getFileNameWithoutExtension());
    pluginsPanel.setStatusText("Assigned to VST node: " + file.getFileNameWithoutExtension());
    saveSessionToDisk();
}

void MainComponent::showTour()
{
    tourOverlay.start();
    tourOverlay.toFront(true);
}

void MainComponent::importProjectSounds()
{
    if (! ensureStorageRootConfigured())
        return;

    if (! projectSession.isValid())
    {
        juce::String errorMessage;
        if (! creation::assets::ProjectWorkspaceService::createProject(suiteSettings, creation::assets::SuiteAppDomain::station, "New Project", "1.0.0", "1.0.0", projectSession, errorMessage))
        {
            transportBar.setStatusText("Could not create a project for imported sounds.");
            return;
        }

        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
    }

    assetChooser = std::make_unique<juce::FileChooser>("Import project sounds",
                                                       juce::File{},
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    assetChooser->launchAsync(juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this](const juce::FileChooser& result)
                              {
                                  auto selectedFiles = result.getResults();
                                  assetChooser.reset();

                                  if (selectedFiles.isEmpty())
                                      return;

                                  auto importedCount = 0;
                                  juce::String lastError;

                                  for (const auto& sourceFile : selectedFiles)
                                  {
                                      juce::String errorMessage;
                                      auto imported = juce::File();
                                      if (imported.existsAsFile())
                                          ++importedCount;
                                      else
                                          lastError = errorMessage;
                                  }

                                  refreshProjectAssets();
                                  saveSessionToDisk();

                                  if (importedCount > 0)
                                      transportBar.setStatusText("Imported " + juce::String(importedCount) + " sound(s) into this project.");
                                  else if (lastError.isNotEmpty())
                                      transportBar.setStatusText(lastError);
                              });
}

void MainComponent::refreshProjectAssets()
{
    if (! projectSession.isValid())
    {
        arrangeView.setProjectAssets({});
        contentPanel.setProjectAssets({});
        refreshAiContextStore();
        return;
    }

    auto projectAssets = projectSession.getManifest().assetCatalog.assets;
    arrangeView.setProjectAssets(filterFoleyAudioAssets(projectAssets));
    contentPanel.setProjectAssets(projectAssets);
    refreshAiContextStore();
}

void MainComponent::refreshContentLibrary()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contentPanel.setStoragePath({});
        contentPanel.setItems({});
        contentPanel.setTutorialItems({});
        contentPanel.setStatusText("Choose a local storage location to initialize the content library.");
        refreshAiContextStore();
        return;
    }

    contentPanel.setStoragePath(juce::File(suiteSettings.suiteVfsRoot).getFullPathName());

    juce::String errorMessage;
    if (! contentLibrary.loadFromStorage(juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/BuiltIn"),
                                         juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/Downloaded"),
                                         juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/User"),
                                         juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/manifest.json"),
                                         errorMessage))
    {
        contentPanel.setItems({});
        contentPanel.setTutorialItems({});
        contentPanel.setStatusText(errorMessage);
        refreshAiContextStore();
        return;
    }

    auto combinedItems = contentLibrary.getItems();
    contentPanel.setItems(combinedItems);
    refreshTutorialLibrary();
    contentPanel.setStatusText(contentLibrary.createSummaryText());
    refreshAiContextStore();

    if (! authenticated)
        return;

    contentPanel.setStatusText(contentLibrary.createSummaryText() + "  |  Syncing LagDaemon...");
    auto token = authSession.getSession().token;

    std::thread([this, token, combinedItems]()
    {
        juce::Array<ContentApiClient::LibraryItem> remoteLibrary;
        juce::String remoteError;
        if (! contentApiClient.fetchLibrary(token, "creation-station", remoteLibrary, remoteError))
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), remoteError]
            {
                if (safeThis != nullptr)
                    safeThis->contentPanel.setStatusText(remoteError);
            });
            return;
        }

        auto mergedItems = combinedItems;
        juce::StringArray installedIds;
        for (const auto& localItem : combinedItems)
            installedIds.addIfNotAlreadyThere(localItem.id);

        for (const auto& remoteItem : remoteLibrary)
        {
            if (installedIds.contains(remoteItem.id))
                continue;

            ContentLibrary::Item item;
            item.id = remoteItem.id;
            item.name = remoteItem.name;
            item.type = remoteItem.itemType;
            item.category = remoteItem.tags.isEmpty() ? "LagDaemon Content" : remoteItem.tags.joinIntoString(", ");
            item.description = remoteItem.description.isNotEmpty() ? remoteItem.description : ("Remote " + remoteItem.itemType + " from LagDaemon.");
            item.requiredTier = remoteItem.requiredTier;
            item.version = remoteItem.version;
            item.origin = ContentLibrary::Origin::remote;
            item.accessState = remoteItem.accessState == "locked" ? ContentLibrary::AccessState::locked
                                                                  : ContentLibrary::AccessState::available;
            item.fileSizeBytes = remoteItem.sizeBytes;
            mergedItems.add(item);
        }

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), mergedItems]
        {
            if (safeThis != nullptr)
            {
                safeThis->contentPanel.setItems(mergedItems);
                safeThis->refreshTutorialLibrary();
                safeThis->contentPanel.setStatusText("Library ready: " + juce::String(mergedItems.size()) + " local + remote items.");
                safeThis->refreshAiContextStore();
            }
        });
    }).detach();
}

void MainComponent::refreshTutorialLibrary()
{
    juce::Array<ContentPanel::TutorialItem> tutorials;

    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contentPanel.setTutorialItems(tutorials);
        return;
    }

    auto collect = [&tutorials](const juce::File& directory, bool builtIn)
    {
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, false, "*.nalm");

        for (const auto& file : files)
        {
            ContentPanel::TutorialItem item;
            item.file = file;
            item.builtIn = builtIn;
            item.name = file.getFileNameWithoutExtension().replace("-", " ");
            item.description = builtIn ? "Bundled guided demo/tutorial." : "User-authored guided demo/tutorial.";

            auto firstLine = file.loadFileAsString().upToFirstOccurrenceOf("\n", false, false).trim();
            if (firstLine.startsWithIgnoreCase("tutorial "))
            {
                auto quotedName = firstLine.fromFirstOccurrenceOf("\"", false, false);
                if (quotedName.isNotEmpty())
                    item.name = quotedName.upToLastOccurrenceOf("\"", false, false);
            }

            tutorials.add(item);
        }
    };

    collect(juce::File(suiteSettings.suiteVfsRoot).getChildFile("Tutorials/BuiltIn"), true);
    collect(juce::File(suiteSettings.suiteVfsRoot).getChildFile("Tutorials/User"), false);
    contentPanel.setTutorialItems(tutorials);
}

void MainComponent::launchTutorialItem(const ContentPanel::TutorialItem& item)
{
    if (! item.file.existsAsFile())
    {
        transportBar.setStatusText("That tutorial file is not available.");
        return;
    }

    cw::tutorial::ScriptCompiler compiler;
    cw::tutorial::Script script;
    juce::String errorMessage;
    if (! compiler.compile(item.file.loadFileAsString(), script, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    tourOverlay.setSteps(buildTutorialSteps(script));
    showTour();
    transportBar.setStatusText("Started tutorial: " + script.name);
}

void MainComponent::refreshAiContextStore()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contextEngine.clearDocuments();
        return;
    }

    juce::String errorMessage;
    if (! contextStore.rebuild(projectSession,
                               suiteSettings,
                               contentLibrary,
                               workspaceModeName(activeMode),
                               dslPanel.getSourceText(),
                               errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    contextEngine.replaceDocuments(contextStore.getDocuments());
}

MainComponent::WorkspaceMode MainComponent::workspaceModeFromString(const juce::String& modeName) const
{
    auto normalized = modeName.trim().toLowerCase();

    if (normalized == "tracker")
        return WorkspaceMode::tracker;
    if (normalized == "sampler")
        return WorkspaceMode::sampler;
    if (normalized == "arrange" || normalized == "foley")
        return WorkspaceMode::arrange;
    if (normalized == "signal")
        return WorkspaceMode::signal;
    if (normalized == "library")
        return WorkspaceMode::library;
    if (normalized == "mix" || normalized == "layers")
        return WorkspaceMode::mix;
    if (normalized == "plugins" || normalized == "plugin")
        return WorkspaceMode::plugins;
    if (normalized == "node" || normalized == "patch")
        return WorkspaceMode::node;
    if (normalized == "code" || normalized == "script")
        return WorkspaceMode::code;
    if (normalized == "record" || normalized == "capture")
        return WorkspaceMode::record;
    if (normalized == "score" || normalized == "song" || normalized == "notation")
        return WorkspaceMode::score;
    if (normalized == "settings" || normalized == "options")
        return WorkspaceMode::settings;

    return activeMode;
}

void MainComponent::configureTutorialOverlay()
{
    cw::tutorial::Script script;

    auto loadFromFile = [this](const juce::File& file, cw::tutorial::Script& loadedScript, juce::String& errorMessage) -> bool
    {
        if (! file.existsAsFile())
            return false;

        cw::tutorial::ScriptCompiler compiler;
        return compiler.compile(file.loadFileAsString(), loadedScript, errorMessage);
    };

    juce::String errorMessage;
    if (suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        auto builtInDirectory = juce::File(suiteSettings.suiteVfsRoot).getChildFile("Tutorials/BuiltIn");
        auto sampleFile = builtInDirectory.getChildFile("getting-started-demo.nalm");
        auto builtInSource = cw::tutorial::getBuiltInGettingStartedTutorialSource();
        auto vstDemoFile = builtInDirectory.getChildFile("vst-node-demo.nalm");
        auto vstDemoSource = cw::tutorial::getBuiltInVstNodeDemoTutorialSource();

        if (! sampleFile.existsAsFile() || sampleFile.loadFileAsString() != builtInSource)
            sampleFile.replaceWithText(builtInSource);

        if (! vstDemoFile.existsAsFile() || vstDemoFile.loadFileAsString() != vstDemoSource)
            vstDemoFile.replaceWithText(vstDemoSource);

        auto userFile = juce::File(suiteSettings.suiteVfsRoot).getChildFile("Tutorials/User").getChildFile("getting-started-demo.nalm");

        if (! loadFromFile(userFile, script, errorMessage))
        {
            errorMessage.clear();
            loadFromFile(sampleFile, script, errorMessage);
        }
    }

    if (script.scenes.isEmpty())
        script = cw::tutorial::makeGettingStartedTutorial();

    tourOverlay.setSteps(buildTutorialSteps(script));
}

std::vector<TourGuideOverlay::Step> MainComponent::buildTutorialSteps(const cw::tutorial::Script& script)
{
    std::vector<TourGuideOverlay::Step> steps;
    steps.reserve((size_t) script.scenes.size());

    for (const auto& scene : script.scenes)
    {
        TourGuideOverlay::Step step;
        step.title = scene.title;
        step.body = scene.body;
        step.advanceOnTargetClick = scene.advanceOnTargetClick;
        step.drawConnector = scene.drawConnector;
        step.nextButtonText = scene.nextButtonText;
        step.targetBounds = [this, targetId = scene.targetId]()
        {
            return tutorialTargetBoundsForId(targetId);
        };
        step.onStepEntered = [this, actions = scene.actions]()
        {
            executeTutorialActions(actions);
        };
        steps.push_back(std::move(step));
    }

    return steps;
}

void MainComponent::executeTutorialActions(const juce::Array<cw::tutorial::Action>& actions)
{
    for (const auto& action : actions)
    {
        switch (action.type)
        {
            case cw::tutorial::ActionType::switchWorkspace:
                setWorkspaceMode(workspaceModeFromString(action.value));
                break;

            case cw::tutorial::ActionType::applySignalTemplate:
                signalLabPanel.applyAiTemplate(action.value);
                break;

            case cw::tutorial::ActionType::applyGraphMacro:
                graphPanel.applyAiMacro(action.value);
                break;
        }
    }
}

juce::Rectangle<int> MainComponent::tutorialTargetBoundsForId(const juce::String& targetId) const
{
    auto id = targetId.trim().toLowerCase();

    if (id == "transport")
        return transportBar.getBounds();
    if (id == "modes")
        return viewModeBar.getBounds();
    if (id == "signal")
        return signalLabPanel.getBounds();
    if (id == "library")
        return contentPanel.getBounds();
    if (id == "mix" || id == "layers")
        return mixerPanel.getBounds();
    if (id == "plugins" || id == "plugin")
        return pluginsPanel.getBounds();
    if (id == "patch" || id == "node")
        return graphPanel.getBounds();
    if (id == "code" || id == "script")
        return dslPanel.getBounds();
    if (id == "record" || id == "capture")
        return recordView.getBounds();
    if (id == "ai")
        return aiPanel.getBounds();

    return {};
}

void MainComponent::executeAiTaskStep(const CreationStationTaskPlanner::TaskStep& step)
{
    juce::StringArray actionNotes;

    for (const auto& action : step.actions)
    {
        switch (action.target)
        {
            case CreationStationTaskPlanner::ActionTarget::workspace:
                if (action.command == "switch-mode")
                {
                    auto mode = workspaceModeFromString(action.stringValue);
                    setWorkspaceMode(mode);
                    actionNotes.add("opened " + workspaceModeName(mode));
                }
                break;

            case CreationStationTaskPlanner::ActionTarget::signalLab:
                if (action.command == "apply-template")
                {
                    signalLabPanel.applyAiTemplate(action.stringValue);
                    actionNotes.add("seeded Signal Lab with " + action.stringValue);
                }
                else if (action.command == "preview-signal")
                {
                    if (signalLabPanel.previewCurrentSignal())
                        actionNotes.add("previewed the current signal");
                    else
                        actionNotes.add("could not preview because no signal is ready yet");
                }
                break;

            case CreationStationTaskPlanner::ActionTarget::patchGraph:
                if (action.command == "apply-macro")
                {
                    graphPanel.applyAiMacro(action.stringValue);
                    actionNotes.add("seeded the patch graph with " + action.stringValue);
                }
                break;

            case CreationStationTaskPlanner::ActionTarget::transport:
            case CreationStationTaskPlanner::ActionTarget::context:
                break;
        }
    }

    if (actionNotes.isEmpty())
        transportBar.setStatusText("AI step complete: " + step.title);
    else
        transportBar.setStatusText("AI step complete: " + step.title + " - " + actionNotes.joinIntoString(", "));
}

void MainComponent::setAiSidebarCollapsed(bool shouldCollapse)
{
    aiSidebarCollapsed = shouldCollapse;
    aiPanel.setCollapsed(shouldCollapse);
    resized();
    markLayoutDirty();
}

void MainComponent::showAiSidebar()
{
    setAiSidebarCollapsed(false);
}

void MainComponent::launchAiCompletion(const CreationStationContextEngine::ContextPacket& packet)
{
    if (aiCompletionInFlight)
        return;

    const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(aiProviderSettings.providerId);
    if (creation::services::SuiteAiProviderRuntime::requiresApiKey(profile, aiProviderSettings.apiKey))
    {
        aiPanel.setAssistantResponse("Enter your provider API key in Settings first.");
        transportBar.setStatusText("AI provider key is missing.");
        return;
    }

    if (pendingAiPrompt.trim().isEmpty())
    {
        aiPanel.setAssistantResponse("Type a prompt first.");
        return;
    }

    aiCompletionInFlight = true;

    auto systemPrompt = appManifest.instructions;
    auto userPrompt = pendingAiPrompt;

    // Only pass along project context that scored as genuinely relevant to this prompt.
    // (packet.summary carries internal ISD tuning metrics for the app's own debugging UI —
    // it is not useful grounding for the model and was previously drowning out the user's
    // actual question, causing generic app-orientation answers regardless of what was asked.)
    juce::String contextBlock;
    if (! packet.snippets.isEmpty())
    {
        contextBlock << "Project context (use only if directly relevant to the request below):\n";
        for (const auto& snippet : packet.snippets)
            contextBlock << "- " << snippet.title << " (" << snippet.category << "): " << snippet.excerpt << "\n";
        contextBlock << "\n";
    }

    userPrompt = contextBlock + userPrompt;

    std::thread([safeThis = juce::Component::SafePointer<MainComponent>(this),
                 systemPrompt = std::move(systemPrompt),
                 userPrompt = std::move(userPrompt)]() mutable
    {
        if (safeThis == nullptr)
            return;

        creation::services::SuiteAiChatClient::ChatResult result;
        auto ok = safeThis->openAiChatClient.sendChatCompletion(safeThis->aiProviderSettings,
                                                                systemPrompt,
                                                                userPrompt,
                                                                result);

        juce::MessageManager::callAsync([safeThis,
                                         ok,
                                         result = std::move(result)]() mutable
        {
            if (safeThis == nullptr)
                return;

            safeThis->aiCompletionInFlight = false;

            if (ok)
            {
                safeThis->aiPanel.setAssistantResponse(result.text);
                safeThis->transportBar.setStatusText("AI response ready.");
            }
            else
            {
                safeThis->aiPanel.setAssistantResponse(result.errorMessage);
                safeThis->transportBar.setStatusText(result.errorMessage);
            }
        });
    }).detach();
}

void MainComponent::refreshAiModelCatalog()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        settingsPanel.setAvailableAiModels({}, "Choose a storage folder first.");
        aiPanel.setAvailableModels({}, "Choose a storage folder first.");
        return;
    }

    const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(aiProviderSettings.providerId);
    if (creation::services::SuiteAiProviderRuntime::requiresApiKey(profile, aiProviderSettings.apiKey))
    {
        settingsPanel.setAvailableAiModels({}, "Enter your provider API key, then refresh the list.");
        aiPanel.setAvailableModels({}, "Enter your provider API key, then refresh the list.");
        return;
    }

    juce::StringArray modelIds;
    juce::String errorMessage;
    if (! modelCatalogClient.fetchModelIds(aiProviderSettings.baseUrl,
                                           aiProviderSettings.providerId,
                                           aiProviderSettings.apiKey,
                                           modelIds,
                                           errorMessage))
    {
        settingsPanel.setAvailableAiModels({}, errorMessage);
        aiPanel.setAvailableModels({}, errorMessage);
        transportBar.setStatusText(errorMessage);
        return;
    }

    auto statusText = "Loaded " + juce::String(modelIds.size()) + " model(s) from your provider.";
    settingsPanel.setAvailableAiModels(modelIds, statusText);
    aiPanel.setAvailableModels(modelIds, statusText);
    transportBar.setStatusText(statusText);
}

bool MainComponent::loadSuiteAiProviderSettings(bool migrateLegacyIfNeeded)
{
    creation::services::SuiteAiSettingsStore store;
    juce::String errorMessage;
    auto suiteAiSettings = store.load(errorMessage);

    const auto runtimeSettings = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station);
    if (runtimeSettings.isValid())
    {
        aiProviderSettings = makeAiProviderSettings(runtimeSettings);
        settingsPanel.setAiProviderSettings(aiProviderSettings);
        aiPanel.setSelectedProvider(aiProviderSettings.providerDisplayName);
        aiPanel.setSelectedModel(aiProviderSettings.modelName);
        return true;
    }

    if (! migrateLegacyIfNeeded)
        return false;

    auto legacyProviderName = aiProviderSettings.providerDisplayName.trim();
    auto legacyBaseUrl = aiProviderSettings.baseUrl.trim();
    auto legacyModelName = aiProviderSettings.modelName.trim();
    auto hasLegacySettings = legacyProviderName.isNotEmpty()
                             || legacyBaseUrl.isNotEmpty()
                             || legacyModelName.isNotEmpty()
                             || aiProviderSettings.apiKey.trim().isNotEmpty();
    if (! hasLegacySettings)
        return false;

    if (! saveSuiteAiProviderSettings(aiProviderSettings, errorMessage))
        return false;

    settingsPanel.setAiProviderSettings(aiProviderSettings);
    aiPanel.setSelectedProvider(aiProviderSettings.providerDisplayName);
    aiPanel.setSelectedModel(aiProviderSettings.modelName);
    return true;
}

bool MainComponent::saveSuiteAiProviderSettings(const AiProviderSettings& settings, juce::String& errorMessage)
{
    creation::services::SuiteAiSettingsStore store;
    auto suiteAiSettings = store.load(errorMessage);

    auto resolvedRuntimeSettings = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station);
    resolvedRuntimeSettings.providerDisplayName = settings.providerDisplayName.trim();
    resolvedRuntimeSettings.providerId = creation::services::SuiteAiProviderRuntime::normalizeProviderId(
        settings.providerId.isNotEmpty() ? settings.providerId : settings.providerDisplayName);
    resolvedRuntimeSettings.baseUrl = settings.baseUrl.trim();
    resolvedRuntimeSettings.modelName = settings.modelName.trim();
    resolvedRuntimeSettings.apiKey = settings.apiKey;

    creation::services::SuiteAiSettingsResolver::upsertRuntimeSettingsForApp(
        suiteAiSettings,
        creation::assets::SuiteAppDomain::station,
        resolvedRuntimeSettings,
        "Creation Station");

    return store.save(suiteAiSettings, errorMessage);
}

void MainComponent::syncSemanticAppContext()
{
    if (! authenticated || appContextSyncInProgress)
        return;

    appContextSyncInProgress = true;
    auto token = authSession.getSession().token;
    auto manifest = appManifest;
    auto appName = juce::String("creation-station");
    auto manifestFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("CreationStation")
                            .getChildFile("creation-station-app-context.json");
    manifestFile.getParentDirectory().createDirectory();
    manifestFile.replaceWithText(manifest.toJson());
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    std::thread([safeThis, token, manifest, appName]() mutable
    {
        LiteSemRagApiClient client;
        LiteSemRagApiClient::AppContextInfo info;
        juce::String errorMessage;
        auto published = client.syncAppContext(token, appName, manifest, info, errorMessage);

        juce::MessageManager::callAsync([safeThis, published, errorMessage, info, checksum = manifest.checksum()]
        {
            if (safeThis == nullptr)
                return;

            safeThis->appContextSyncInProgress = false;

            if (published)
            {
                safeThis->appContextLastPublishedChecksum = checksum;
                safeThis->transportBar.setStatusText("LiteSemRAG app context synced.");
                return;
            }

            if (errorMessage.isNotEmpty())
                safeThis->transportBar.setStatusText(errorMessage);
        });
    }).detach();
}

void MainComponent::downloadContentItem(const ContentLibrary::Item& item)
{
    if (! authenticated)
    {
        contentPanel.setStatusText("Sign in to download LagDaemon content.");
        return;
    }

    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contentPanel.setStatusText("Choose a local storage location before downloading content.");
        return;
    }

    if (item.origin != ContentLibrary::Origin::remote || item.id.isEmpty())
    {
        contentPanel.setStatusText("That item is already local.");
        return;
    }

    auto slug = item.name.trim().toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");
    if (slug.isEmpty())
        slug = "content";

    juce::String extension;
    if (item.type == "patch")
        extension = ".cspatch";
    else if (item.type == "pack" || item.type == "sample-pack")
        extension = ".cspack";
    else if (item.type == "audio")
        extension = ".wav";
    else
        extension = ".bin";

    auto destination = juce::File(suiteSettings.suiteVfsRoot).getChildFile("Content/Downloaded")
                           .getChildFile(item.id + "__" + slug + extension);
    auto token = authSession.getSession().token;

    contentPanel.setStatusText("Downloading " + item.name + "...");
    std::thread([this, token, item, destination]()
    {
        juce::String errorMessage;
        auto success = contentApiClient.downloadContentItem(token, item.id, destination, errorMessage);

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), success, errorMessage, item]
        {
            if (safeThis == nullptr)
                return;

            if (! success)
            {
                safeThis->contentPanel.setStatusText(errorMessage);
                return;
            }

            safeThis->contentPanel.setStatusText("Downloaded " + item.name + " from LagDaemon.");
            safeThis->refreshContentLibrary();
        });
    }).detach();
}

void MainComponent::activateContentItem(const ContentLibrary::Item& item)
{
    if (! item.file.existsAsFile())
    {
        contentPanel.setStatusText("That content item is not available on disk.");
        return;
    }

    if (item.type == "patch")
    {
        juce::String errorMessage;
        cw::PatchDocument document;
        if (! cw::parsePatchDocumentJson(item.file.loadFileAsString(), document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        if (! signalLabPanel.loadPatchDocument(document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        transportBar.setStatusText("Opened patch from library: " + item.file.getFileName());
        setWorkspaceMode(WorkspaceMode::signal);
        return;
    }

    if (item.type == "audio")
    {
        if (! projectSession.isValid())
        {
            contentPanel.setStatusText("Open or create a project before importing library audio into Foley.");
            return;
        }

        juce::String errorMessage;
        auto importedFile = juce::File();
        if (! importedFile.existsAsFile())
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        refreshProjectAssets();
        arrangeView.addAssetClipToSelectedTrack(importedFile.getFileName());
        refreshFoleyArrangement();
        transportBar.setStatusText("Imported library audio into Foley: " + importedFile.getFileName());
        setWorkspaceMode(WorkspaceMode::arrange);
        return;
    }

    item.file.revealToUser();
    transportBar.setStatusText("Revealed content item: " + item.file.getFileName());
}

void MainComponent::openProjectAsset(const creation::assets::AssetDescriptor& asset)
{
    juce::String errorMessage;
    creation::assets::MaterializedAssetLease lease;
    if (! projectSession.materializeEntry(suiteSettings, asset.logicalPath,
                                          creation::assets::MaterializationAccess::readOnly,
                                          lease, errorMessage))
    {
        contentPanel.setStatusText("Could not read that project asset: " + errorMessage);
        return;
    }

    if (asset.kind == creation::assets::AssetKind::patch)
    {
        cw::PatchDocument document;
        if (! cw::parsePatchDocumentJson(lease.materializedFile.loadFileAsString(), document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        if (! signalLabPanel.loadPatchDocument(document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        setWorkspaceMode(WorkspaceMode::signal);
        transportBar.setStatusText("Opened project sound: " + asset.displayName);
        return;
    }

    if (asset.kind == creation::assets::AssetKind::audio
        || asset.kind == creation::assets::AssetKind::render)
    {
        if (! engine.previewAssetFile(lease.materializedFile, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        transportBar.setStatusText("Previewing project asset: " + asset.displayName);
        return;
    }

    lease.materializedFile.revealToUser();
    transportBar.setStatusText("Revealed project asset: " + asset.displayName);
}

void MainComponent::placeProjectAssetOnTracker(const creation::assets::AssetDescriptor& asset)
{
    if (asset.kind != creation::assets::AssetKind::audio
        && asset.kind != creation::assets::AssetKind::render)
    {
        contentPanel.setStatusText("Only audio project assets can be placed on the Tracker right now.");
        return;
    }

    if (engine.getTrackCount() == 0)
        addTrack();

    auto targetTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
        targetTrack = 0;

    juce::String errorMessage;
    auto sourceTool = asset.kind == creation::assets::AssetKind::render ? "render" : "project-audio";
    auto clipIndex = placeAudioAssetOnTracker(asset,
                                              targetTrack,
                                              timelineModel.getTransportSeconds(),
                                              sourceTool,
                                              errorMessage);

    if (clipIndex < 0)
    {
        contentPanel.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not place that asset on the Tracker.");
        return;
    }

    trackerPanel.setSelectedTrack(targetTrack);
    trackerPanel.refreshTimelineView();
    setWorkspaceMode(WorkspaceMode::tracker);
    saveSessionToDisk();
    transportBar.setStatusText("Placed project asset on Tracker: " + asset.displayName);
}

int MainComponent::placeAudioAssetOnTracker(const creation::assets::AssetDescriptor& asset,
                                            int targetTrack,
                                            double startSeconds,
                                            const juce::String& sourceTool,
                                            juce::String& errorMessage)
{
    // Materialize the asset from the VFS container to a real temp file for the audio engine
    creation::assets::MaterializedAssetLease lease;
    if (! projectSession.materializeEntry(suiteSettings, asset.logicalPath,
                                          creation::assets::MaterializationAccess::readOnly,
                                          lease, errorMessage))
        return -1;

    cs::AssetRef assetRef;
    assetRef.id = asset.id;
    assetRef.versionId = asset.versionId;
    assetRef.mode = creation::assets::AssetReferenceMode::exact;

    auto clipIndex = timelineModel.addClip(cs::ClipKind::audio,
                                           targetTrack,
                                           asset.displayName,
                                           asset.id,
                                           sourceTool,
                                           lease.materializedFile,
                                           startSeconds,
                                           0.0,
                                           errorMessage);

    if (clipIndex >= 0)
        timelineModel.setClipAssetReference(clipIndex, assetRef);

    if (clipIndex >= 0 && juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
    {
        engine.setTrackStereoEnabled(targetTrack, true);
        timelineModel.setTrackChannelMode(targetTrack, cs::TrackChannelMode::stereo);
        trackerPanel.setTrackStereo(targetTrack, true);
    }

    return clipIndex;
}

bool MainComponent::importAudioFilesToTracker(const juce::StringArray& filePaths, int preferredTrack, double startSeconds)
{
    if (filePaths.isEmpty())
        return false;

    if (! ensureStorageRootConfigured())
        return false;

    juce::String projectError;
    if (! ensureProjectSessionActive(projectError))
    {
        transportBar.setStatusText(projectError.isNotEmpty() ? projectError : "Could not initialize project for imported audio.");
        return false;
    }

    if (engine.getTrackCount() == 0)
        addTrack();

    auto targetTrack = preferredTrack;
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
        targetTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
    {
        addTrack();
        targetTrack = engine.getTrackCount() - 1;
    }

    auto placedCount = 0;
    auto nextStartSeconds = juce::jmax(0.0, startSeconds);
    juce::String lastError;
    auto insertionTrack = targetTrack;

    for (const auto& filePath : filePaths)
    {
        auto sourceFile = juce::File(filePath);
        if (! sourceFile.existsAsFile())
            continue;

        if (! juce::isPositiveAndBelow(insertionTrack, engine.getTrackCount()))
        {
            addTrack();
            insertionTrack = engine.getTrackCount() - 1;
        }

        // Import the external audio file into the VFS container
        juce::String importError;
        auto logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot
                         + sourceFile.getFileName();

        juce::MemoryBlock fileData;
        if (! sourceFile.loadFileAsData(fileData))
        {
            lastError = "Could not read: " + sourceFile.getFileName();
            continue;
        }

        if (! projectSession.writeEntry(logicalPath, fileData, juce::Time::getCurrentTime()))
        {
            lastError = "Could not import: " + sourceFile.getFileName();
            continue;
        }

        creation::assets::AssetDescriptor importedAsset;
        importedAsset.id = "asset:" + juce::Uuid().toString();
        importedAsset.version = "1";
        importedAsset.versionId = importedAsset.id + "@1";
        importedAsset.displayName = sourceFile.getFileNameWithoutExtension();
        importedAsset.logicalPath = logicalPath;
        importedAsset.kind = creation::assets::AssetKind::audio;
        importedAsset.mediaType = "audio/wav";
        importedAsset.fileSizeBytes = (int64) fileData.getSize();
        importedAsset.createdAt = importedAsset.modifiedAt = juce::Time::getCurrentTime();
        importedAsset.sourceApp = "Creation Station";
        projectSession.upsertAssetDescriptor(importedAsset);

        if (! projectSession.commit(importError))
        {
            lastError = importError;
            continue;
        }

        juce::String clipError;
        auto clipIndex = placeAudioAssetOnTracker(importedAsset, insertionTrack, nextStartSeconds, "import", clipError);
        if (clipIndex < 0)
        {
            lastError = clipError;
            continue;
        }

        const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
        ++placedCount;

        if (filePaths.size() > 1)
        {
            nextStartSeconds = juce::jmax(0.0, startSeconds);
            ++insertionTrack;
        }
        else
        {
            nextStartSeconds = clip.startSeconds + clip.durationSeconds;
        }
    }

    if (placedCount <= 0)
    {
        if (lastError.isNotEmpty())
            transportBar.setStatusText(lastError);
        return false;
    }

    refreshProjectAssets();
    trackerPanel.setSelectedTrack(targetTrack);
    trackerPanel.refreshTimelineView();
    setWorkspaceMode(WorkspaceMode::tracker);
    saveSessionToDisk(true);
    transportBar.setStatusText("Imported " + juce::String(placedCount) + " audio file(s) onto the Tracker.");
    return true;
}

std::optional<creation::assets::AssetDescriptor> MainComponent::resolveTimelineClipAsset(const cs::TimelineClip& clip) const
{
    if (! projectSession.isValid() || clip.assetId.isEmpty())
        return std::nullopt;

    const auto* descriptor = projectSession.getManifest().assetCatalog.findById(clip.assetId);
    if (descriptor != nullptr)
        return *descriptor;

    return std::nullopt;
}

void MainComponent::resolveTrackerClipAssetFiles()
{
    const auto& clips = timelineModel.getClips();
    for (int clipIndex = 0; clipIndex < (int) clips.size(); ++clipIndex)
    {
        const auto& clip = clips[(size_t) clipIndex];
        if (clip.assetId.isEmpty() || clip.file.existsAsFile())
            continue;

        auto assetOpt = resolveTimelineClipAsset(clip);
        if (! assetOpt.has_value())
            continue;

        // Materialize the asset from the VFS so the audio engine can access a real file
        juce::String matError;
        creation::assets::MaterializedAssetLease lease;
        if (projectSession.materializeEntry(suiteSettings, assetOpt->logicalPath,
                                            creation::assets::MaterializationAccess::readOnly,
                                            lease, matError))
        {
            timelineModel.setClipFile(clipIndex, lease.materializedFile);
        }
    }
}

void MainComponent::exportProjectAssetRaw(const creation::assets::AssetDescriptor& asset)
{
    if (asset.kind != creation::assets::AssetKind::audio
        && asset.kind != creation::assets::AssetKind::render)
    {
        contentPanel.setStatusText("Only project WAV/render audio can be exported raw right now.");
        return;
    }

    juce::String matError;
    creation::assets::MaterializedAssetLease lease;
    if (! projectSession.materializeEntry(suiteSettings, asset.logicalPath,
                                          creation::assets::MaterializationAccess::readOnly,
                                          lease, matError))
    {
        contentPanel.setStatusText("Could not read asset for export: " + matError);
        return;
    }

    auto fileName = asset.logicalPath.fromLastOccurrenceOf("/", false, false);
    rawAssetExportChooser = std::make_unique<juce::FileChooser>("Export raw project audio",
                                                                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                    .getChildFile(fileName),
                                                                "*.wav",
                                                                true);
    auto chooser = rawAssetExportChooser.get();
    auto materializedFile = lease.materializedFile;
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser, asset, materializedFile](const juce::FileChooser& result)
                         {
                             auto destination = result.getResult();
                             if (chooser == rawAssetExportChooser.get())
                                 rawAssetExportChooser.reset();

                             if (destination.getFullPathName().isEmpty())
                                 return;

                             if (destination.getFileExtension().isEmpty())
                                 destination = destination.withFileExtension(materializedFile.getFileExtension());

                             if (destination.existsAsFile() && ! destination.deleteFile())
                             {
                                 contentPanel.setStatusText("Could not replace the existing export file.");
                                 return;
                             }

                             if (! materializedFile.copyFileTo(destination))
                             {
                                 contentPanel.setStatusText("Could not export the raw audio file.");
                                 return;
                             }

                             contentPanel.setStatusText("Exported raw audio: " + destination.getFileName());
                         });
}

bool MainComponent::renderFullMixToProject()
{
    if (engine.isRecording() || engine.isPlaying())
    {
        transportBar.setStatusText("Stop playback or recording before rendering.");
        return false;
    }

    if (! projectSession.isValid())
    {
        transportBar.setStatusText("Create or open a project before rendering.");
        return false;
    }

    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double durationSeconds = 0.0;
    juce::String errorMessage;
    if (! buildTrackerPlaybackTargets(targets, durationSeconds, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return false;
    }

    auto* currentDevice = deviceManager.getCurrentAudioDevice();
    WorkstationAudioEngine::RenderSettings settings;
    settings.sampleRate = currentDevice != nullptr ? currentDevice->getCurrentSampleRate() : 48000.0;
    settings.blockSize = currentDevice != nullptr ? currentDevice->getCurrentBufferSizeSamples() : 512;

    juce::AudioBuffer<float> renderedMix;
    transportBar.setStatusText("Rendering full mix...");
    if (! engine.renderTrackerMixToBuffer(targets, durationSeconds, settings, renderedMix, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return false;
    }

    auto renderName = projectSession.getManifest().projectName.toLowerCase().replace(" ", "-") + "-full-mix";
    auto renderFile = juce::File();
    if (! renderFile.existsAsFile())
    {
        transportBar.setStatusText(errorMessage);
        return false;
    }

    refreshProjectAssets();
    saveSessionToDisk();
    transportBar.setStatusText("Rendered full mix to project: " + renderFile.getFileName());
    return true;
}

void MainComponent::exportFullMixAsWav()
{
    if (engine.isRecording() || engine.isPlaying())
    {
        transportBar.setStatusText("Stop playback or recording before exporting.");
        return;
    }

    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double durationSeconds = 0.0;
    juce::String errorMessage;
    if (! buildTrackerPlaybackTargets(targets, durationSeconds, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    auto defaultName = projectSession.isValid() ? projectSession.getManifest().projectName.toLowerCase().replace(" ", "-") + "-full-mix.wav"
                                                   : "creation-station-full-mix.wav";
    renderExportChooser = std::make_unique<juce::FileChooser>("Export full mix as WAV",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                  .getChildFile(defaultName),
                                                              "*.wav",
                                                              true);
    auto chooser = renderExportChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser, targets, durationSeconds](const juce::FileChooser& result)
                         {
                             auto destination = result.getResult();
                             if (chooser == renderExportChooser.get())
                                 renderExportChooser.reset();

                             if (destination.getFullPathName().isEmpty())
                                 return;

                             if (destination.getFileExtension().isEmpty())
                                 destination = destination.withFileExtension(".wav");

                             auto* currentDevice = deviceManager.getCurrentAudioDevice();
                             WorkstationAudioEngine::RenderSettings settings;
                             settings.sampleRate = currentDevice != nullptr ? currentDevice->getCurrentSampleRate() : 48000.0;
                             settings.blockSize = currentDevice != nullptr ? currentDevice->getCurrentBufferSizeSamples() : 512;

                             juce::String errorMessage;
                             juce::AudioBuffer<float> renderedMix;
                             transportBar.setStatusText("Exporting full mix...");
                             if (! engine.renderTrackerMixToBuffer(targets, durationSeconds, settings, renderedMix, errorMessage))
                             {
                                 transportBar.setStatusText(errorMessage);
                                 return;
                             }

                             if (destination.existsAsFile() && ! destination.deleteFile())
                             {
                                 transportBar.setStatusText("Could not replace the existing export file.");
                                 return;
                             }

                             if (! writeWavFile(destination, renderedMix, settings.sampleRate, errorMessage))
                             {
                                 transportBar.setStatusText(errorMessage);
                                 return;
                             }

                             transportBar.setStatusText("Exported full mix: " + destination.getFileName());
                         });
}

void MainComponent::refreshFoleyArrangement()
{
    if (! projectSession.isValid())
        return;

    // Pre-materialize all audio assets so the engine can access real files
    auto foleyAssets = filterFoleyAudioAssets(projectSession.getManifest().assetCatalog.query({}));
    juce::StringPairArray assetIdToFilePath;
    for (const auto& asset : foleyAssets)
    {
        juce::String matError;
        creation::assets::MaterializedAssetLease lease;
        if (projectSession.materializeEntry(suiteSettings, asset.logicalPath,
                                            creation::assets::MaterializationAccess::readOnly,
                                            lease, matError))
        {
            assetIdToFilePath.set(asset.id, lease.materializedFile.getFullPathName());
        }
    }

    juce::String errorMessage;
    if (! engine.setFoleyArrangement(arrangeView.createState(),
                                     assetIdToFilePath,
                                     errorMessage)
        && errorMessage.isNotEmpty())
    {
        transportBar.setStatusText(errorMessage);
    }
}

void MainComponent::showProjectMenu()
{
    juce::PopupMenu menu;
    constexpr int projectItemBase = 1000;
    juce::String listError;
    auto availableProjects = creation::assets::ProjectContainerService::listProjects(
        suiteSettings,
        creation::assets::SuiteAppDomain::station,
        listError);

    if (! availableProjects.isEmpty())
    {
        for (int index = 0; index < availableProjects.size(); ++index)
        {
            const auto& project = availableProjects.getReference(index);
            bool isCurrentProject = projectSession.isValid()
                                    && project.containerFile == projectSession.getContainerFile();
            auto label = project.manifest.projectName;
            if (label.isEmpty())
                label = project.containerFile.getFileNameWithoutExtension();

            menu.addItem(projectItemBase + index, label, true, isCurrentProject);
        }

        menu.addSeparator();
    }

    menu.addItem(1, "Create New Project...");
    menu.addItem(2, "Create New Project From Template...");
    menu.addItem(3, "Open Project Browser / Package...");
    menu.addSeparator();
    menu.addItem(4, "Save Project");
    menu.addItem(5, "Save Project As...");
    menu.addItem(6, "Save Project As Template...");
    menu.addItem(7, "Open Project Folder");
    menu.addSeparator();
    menu.addItem(8, "Render Full Mix to Project");
    menu.addItem(9, "Export Full Mix as WAV...");
    menu.addSeparator();
    auto screenArea = transportBar.getProjectButtonScreenBounds();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenArea),
                       [this, availableProjects, listError](int result)
                       {
                           if (result >= projectItemBase && result < projectItemBase + availableProjects.size())
                           {
                               auto selectedProject = availableProjects.getReference(result - projectItemBase);
                               guardUnsavedProjectChange("opening another project", [this, selectedProject]
                               {
                                   juce::String errorMessage;
                                   if (! creation::assets::ProjectWorkspaceService::openProject(selectedProject.containerFile, projectSession, errorMessage))
                                   {
                                       juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                              "Project Error",
                                                                              errorMessage);
                                       return;
                                   }

                                   transportBar.setProjectLabel(projectSession.getManifest().projectName);
                                   settingsPanel.setProjectMetadata(projectSession.getManifest());
                                   refreshProjectAssets();
                                   loadSessionFromDisk();
                               });
                               return;
                           }

                           switch (result)
                           {
                               case 1: createNewProject(); break;
                               case 2: createProjectFromTemplate(); break;
                               case 3: openProject(); break;
                               case 4: saveProject(); break;
                               case 5: saveProjectAs(); break;
                               case 6: saveProjectAsTemplate(); break;
                               case 7: revealProjectFolder(); break;
                               case 8: renderFullMixToProject(); break;
                               case 9: exportFullMixAsWav(); break;
                               default: break;
                           }

                           if (result == 0 && listError.isNotEmpty())
                               transportBar.setStatusText("Project list warning: " + listError);
                       });
}

void MainComponent::showSuiteSettingsWindow()
{
    if (suiteSettingsWindow != nullptr)
    {
        suiteSettingsWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<SuiteSettingsPanel>();
    panel->setSettings(suiteSettings);
    panel->onBrowseRequested = [this](const juce::String& fieldId)
    {
        chooseSuiteDirectory(fieldId);
    };
    panel->onApplyRequested = [this](const SuiteSettings& settings)
    {
        applySuiteSettings(settings);
    };
    panel->onReadEulaRequested = [this]
    {
        suiteShellController.showSuiteEula();
    };

    auto* panelRaw = panel.get();
    auto window = std::make_unique<ManagedDocumentWindow>("Creation Suite Control",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              closeSuiteSettingsWindow();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(940, 560);
    window->setVisible(true);

    suiteSettingsPanel = panelRaw;
    suiteSettingsWindow = std::move(window);
}

void MainComponent::closeSuiteSettingsWindow()
{
    suiteSettingsPanel = nullptr;
    suiteSettingsWindow.reset();
}

void MainComponent::chooseSuiteDirectory(const juce::String& fieldId)
{
    juce::String currentPath = suiteSettings.suiteVfsRoot;
    if (fieldId == "shared_resources_root")
        currentPath = suiteSettings.sharedResourcesRoot;
    else if (fieldId == "creation_station_projects_root")
        currentPath = suiteSettings.creationStationProjectsRoot;
    else if (fieldId == "creation_engine_projects_root")
        currentPath = suiteSettings.creationEngineProjectsRoot;
    else if (fieldId == "creation_movie_projects_root")
        currentPath = suiteSettings.creationMovieProjectsRoot;
    else if (fieldId == "creation_live_projects_root")
        currentPath = suiteSettings.creationLiveProjectsRoot;

    suiteDirectoryChooser = std::make_unique<juce::FileChooser>("Choose a folder for the Creation Suite",
                                                                currentPath.isNotEmpty()
                                                                    ? juce::File(currentPath)
                                                                    : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                                "*",
                                                                true);
    auto chooser = suiteDirectoryChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser, fieldId](const juce::FileChooser& result)
                         {
                             auto selected = result.getResult();
                             if (chooser == suiteDirectoryChooser.get())
                                 suiteDirectoryChooser.reset();

                             if (selected == juce::File())
                                 return;

                             auto selectedPath = selected.getFullPathName();
                             if (fieldId == "suite_vfs_root")
                                 suiteSettings.suiteVfsRoot = selectedPath;
                             else if (fieldId == "shared_resources_root")
                                 suiteSettings.sharedResourcesRoot = selectedPath;
                             else if (fieldId == "creation_station_projects_root")
                                 suiteSettings.creationStationProjectsRoot = selectedPath;
                             else if (fieldId == "creation_engine_projects_root")
                                 suiteSettings.creationEngineProjectsRoot = selectedPath;
                             else if (fieldId == "creation_movie_projects_root")
                                 suiteSettings.creationMovieProjectsRoot = selectedPath;
                             else if (fieldId == "creation_live_projects_root")
                                 suiteSettings.creationLiveProjectsRoot = selectedPath;

                             if (suiteSettingsPanel != nullptr)
                                 suiteSettingsPanel->setSettings(suiteSettings);
                         });
}

void MainComponent::applySuiteSettings(const SuiteSettings& settings)
{
    juce::String errorMessage;
    if (! suiteSettingsStore.save(settings, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        if (suiteSettingsPanel != nullptr)
            suiteSettingsPanel->setStatusText(errorMessage);
        return;
    }

    suiteSettings = settings;
    transportBar.setStatusText("Saved Creation Suite settings.");
    if (suiteSettingsPanel != nullptr)
        suiteSettingsPanel->setStatusText("Saved suite-wide settings for all Creation apps.");
}

void MainComponent::createNewProject()
{
    guardUnsavedProjectChange("creating a new project", [this] { beginCreateNewProject(); });
}

void MainComponent::beginCreateNewProject()
{
    if (! ensureStorageRootConfigured())
        return;

    auto* nameEditor = new juce::AlertWindow("Create New Project",
                                             "Enter a name for your new project container:",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", "");
    nameEditor->addButton("Create Project", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto name = dialog->getTextEditorContents("projectName").trim();
        if (name.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   "Project name cannot be empty. Please enter a name.");
            return;
        }

        juce::String errorMessage;
        if (! creation::assets::ProjectWorkspaceService::createProject(options->suiteSettings, creation::assets::SuiteAppDomain::station, name, "1.0.0", "1.0.0", options->projectSession, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        options->transportBar.setProjectLabel("Project: " + options->projectSession.getManifest().projectName);
        options->settingsPanel.setProjectMetadata(options->projectSession.getManifest());
        options->refreshProjectAssets();
        options->saveSessionToDisk(true);
        options->transportBar.setStatusText("Created project: " + options->projectSession.getContainerFile().getFileName());
    }), true);
}

void MainComponent::openProject()
{
    if (! ensureStorageRootConfigured())
        return;

    suiteShellController.showProjectBrowser();
}

void MainComponent::openProject(const juce::File& containerFile)
{
    guardUnsavedProjectChange("opening another project", [this, containerFile]
    {
        juce::String errorMessage;
        if (! creation::assets::ProjectWorkspaceService::openProject(containerFile, projectSession, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        settingsPanel.setProjectMetadata(projectSession.getManifest());
        refreshProjectAssets();
        loadSessionFromDisk();
        saveAppSettings();
    });
}

void MainComponent::saveProject()
{
    if (! projectSession.isValid())
    {
        createNewProject();
        return;
    }

    saveSessionToDisk(true);
    if (! projectDirty)
        transportBar.setStatusText("Project saved: " + projectSession.getContainerFile().getFileName());
}

void MainComponent::saveProjectAs()
{
    if (! projectSession.isValid())
    {
        createNewProject();
        return;
    }

    auto* nameEditor = new juce::AlertWindow("Save Project As",
                                             "Give the copied project a new name.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", projectSession.getManifest().projectName + " Copy");
    nameEditor->addButton("Save As", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto projectName = dialog->getTextEditorContents("projectName").trim();
        if (projectName.isEmpty())
            projectName = options->projectSession.getManifest().projectName + " Copy";

        juce::String errorMessage;
        creation::assets::ProjectSession newSession;
        if (! creation::assets::ProjectWorkspaceService::createProject(options->suiteSettings,
                                                                        creation::assets::SuiteAppDomain::station,
                                                                        projectName, "1.0.0", "1.0.0",
                                                                        newSession, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        // Transfer current session XML entry
        juce::MemoryBlock sessionData;
        if (options->projectSession.readEntry("session.xml", sessionData))
            newSession.writeEntry("session.xml", sessionData);
        newSession.commit(errorMessage);

        options->projectSession = std::move(newSession);
        options->transportBar.setProjectLabel("Project: " + options->projectSession.getManifest().projectName);
        options->settingsPanel.setProjectMetadata(options->projectSession.getManifest());
        options->refreshProjectAssets();
        options->loadSessionFromDisk();
        options->saveSessionToDisk(true);
        options->transportBar.setStatusText("Saved project as: " + options->projectSession.getContainerFile().getFileName());
    }), true);
}

void MainComponent::createProjectFromTemplate()
{
    guardUnsavedProjectChange("creating a project from a template", [this] { beginCreateProjectFromTemplate(); });
}

void MainComponent::beginCreateProjectFromTemplate()
{
    if (! ensureStorageRootConfigured())
        return;

    projectChooser = std::make_unique<juce::FileChooser>("Create a project from a Creation Station template",
                                                         juce::File(suiteSettings.suiteVfsRoot).getChildFile("Templates"),
                                                         "*.csproj",
                                                         true);

    auto chooser = projectChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& result)
                         {
                             auto templateFile = result.getResult();
                             if (! templateFile.existsAsFile())
                                 return;

                             auto* nameEditor = new juce::AlertWindow("New Project From Template",
                                                                      "Give the new project a name.",
                                                                      juce::MessageBoxIconType::QuestionIcon);
                             nameEditor->addTextEditor("projectName", templateFile.getFileNameWithoutExtension());
                             nameEditor->addButton("Create", 1);
                             nameEditor->addButton("Cancel", 0);

                             auto options = juce::Component::SafePointer<MainComponent>(this);
                             nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor, templateFile](int modalResult) mutable
                             {
                                 std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
                                 if (modalResult != 1 || options == nullptr)
                                     return;

                                 auto projectName = dialog->getTextEditorContents("projectName").trim();
                                 juce::String errorMessage;
                                 if (! creation::assets::ProjectWorkspaceService::createProject(options->suiteSettings,
                                                                                                 creation::assets::SuiteAppDomain::station,
                                                                                                 projectName, "1.0.0", "1.0.0",
                                                                                                 options->projectSession, errorMessage))
                                 {
                                     juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                            "Template Error",
                                                                            errorMessage);
                                     return;
                                 }

                                 // Read template container session.xml if present
                                 creation::assets::ProjectSession templateSession;
                                 if (creation::assets::ProjectWorkspaceService::openProject(templateFile, templateSession, errorMessage))
                                 {
                                     juce::MemoryBlock sessionData;
                                     if (templateSession.readEntry("session.xml", sessionData))
                                         options->projectSession.writeEntry("session.xml", sessionData);
                                     options->projectSession.commit(errorMessage);
                                 }

                                 options->transportBar.setProjectLabel("Project: " + options->projectSession.getManifest().projectName);
                                 options->settingsPanel.setProjectMetadata(options->projectSession.getManifest());
                                 options->refreshProjectAssets();
                                 options->loadSessionFromDisk();
                                 options->saveSessionToDisk(true);
                                 options->transportBar.setStatusText("Project created from template.");
                             }), true);
                         });
}

void MainComponent::saveProjectAsTemplate()
{
    if (! projectSession.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Template Needs A Project",
                                               "Create or open a project first, then save it as a template.");
        return;
    }

    auto* nameEditor = new juce::AlertWindow("Save Project As Template",
                                             "Name this reusable studio setup.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("templateName", projectSession.getManifest().projectName + " Template");
    nameEditor->addButton("Save Template", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto templateName = dialog->getTextEditorContents("templateName").trim();
        juce::File templatesDir = juce::File(options->suiteSettings.suiteVfsRoot).getChildFile("Templates");
        templatesDir.createDirectory();
        juce::File templateFile = templatesDir.getChildFile(templateName + ".csproj");

        juce::String errorMessage;
        if (! options->projectSession.getContainerFile().copyFileTo(templateFile))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Template Error",
                                                   "Could not create template file.");
            return;
        }

        options->transportBar.setStatusText("Saved template: " + templateFile.getFileName());
    }), true);
}

void MainComponent::guardUnsavedProjectChange(const juce::String& actionName, const std::function<void()>& action)
{
    juce::ignoreUnused(actionName);

    if (! projectDirty)
    {
        if (action)
            action();
        return;
    }

    saveSessionToDisk(true);

    if (! projectDirty && action)
        action();
}

juce::String MainComponent::createRecordingTakeName() const
{
    return "Take-" + makeRecordingTimestamp() + ".wav";
}

void MainComponent::refreshRecentTakes()
{
    if (! projectSession.isValid())
    {
        recordView.setRecentTakes({});
        arrangeView.setRecordedClips({});
        refreshProjectAssets();
        return;
    }

    juce::StringArray names;
    auto projectAssets = projectSession.getManifest().assetCatalog.query({});
    for (const auto& asset : projectAssets)
    {
        if (asset.kind != creation::assets::AssetKind::audio
            && asset.kind != creation::assets::AssetKind::render)
            continue;

        auto fileName = asset.logicalPath.fromLastOccurrenceOf("/", false, false);
        if (fileName.isNotEmpty())
            names.add(fileName);
        if (names.size() >= 10)
            break;
    }

    recordView.setRecentTakes(names);
    arrangeView.setRecordedClips(names);
    arrangeView.setProjectAssets(filterFoleyAudioAssets(projectAssets));
    contentPanel.setProjectAssets(projectAssets);
}

bool MainComponent::startRecordingSession()
{
    if (! ensureStorageRootConfigured())
        return false;

    juce::String recProjectError;
    if (! ensureProjectSessionActive(recProjectError))
    {
        transportBar.setStatusText(recProjectError.isNotEmpty() ? recProjectError : "Could not initialize project for recording.");
        return false;
    }

    if (engine.getTrackCount() == 0)
        addTrack();

    activeRecordingTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(activeRecordingTrack, engine.getTrackCount()))
        activeRecordingTrack = 0;

    auto hasArmedTrack = false;
    for (auto armed : armedTracks)
        hasArmedTrack = hasArmedTrack || armed;

    if (! hasArmedTrack && juce::isPositiveAndBelow(activeRecordingTrack, (int) armedTracks.size()))
    {
        armedTracks[(size_t) activeRecordingTrack] = true;
        engine.setTrackRecordingArmed(activeRecordingTrack, true);
        trackerPanel.setTrackArmed(activeRecordingTrack, true);
    }

    juce::Array<WorkstationAudioEngine::RecordingTarget> recordingTargets;
    juce::Array<int> midiRecordingTracks;
    const auto timestamp = makeRecordingTimestamp();

    for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) trackIndex])
            continue;

        if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::midi)
        {
            midiRecordingTracks.add(trackIndex);
            continue;
        }

        auto trackName = engine.getTrackName(trackIndex).retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        trackName = trackName.trim().replace(" ", "-");

        if (trackName.isEmpty())
            trackName = "Track-" + juce::String(trackIndex + 1).paddedLeft('0', 2);

        WorkstationAudioEngine::RecordingTarget target;
        target.trackIndex = trackIndex;
        target.file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Take-" + timestamp
                                                                                    + "-T" + juce::String(trackIndex + 1).paddedLeft('0', 2)
                                                                                    + "-" + trackName
                                                                                    + ".wav");
        recordingTargets.add(target);
    }

    if (! recordingTargets.isEmpty())
    {
        juce::String errorMessage;
        if (! engine.startRecordingToFiles(recordingTargets, errorMessage))
        {
            transportBar.setStatusText("Record failed: " + errorMessage);
            return false;
        }

        for (const auto& target : recordingTargets)
            timelineModel.beginRecordingClip(target.trackIndex, target.file);
    }

    if (! midiRecordingTracks.isEmpty())
    {
        engine.startMidiRecording();
        for (auto trackIndex : midiRecordingTracks)
            timelineModel.beginRecordingMidiClip(trackIndex);
    }

    const auto totalArmedCount = recordingTargets.size() + midiRecordingTracks.size();
    if (totalArmedCount == 0)
    {
        transportBar.setStatusText("Record failed: no armed tracks were available for recording.");
        return false;
    }

    trackerPanel.refreshTimelineView();
    transportBar.setStatusText("Recording " + juce::String(totalArmedCount) + " track(s).");
    recordView.setRecordingState(true, totalArmedCount == 1
                                            ? (recordingTargets.size() == 1 ? recordingTargets[0].file.getFileName()
                                                                             : juce::String("MIDI"))
                                            : juce::String(totalArmedCount) + " tracks");
    refreshRecentTakes();
    return true;
}

void MainComponent::stopRecordingSession()
{
    const auto wasMidiRecording = engine.isMidiRecording();
    if (! engine.isRecording() && ! wasMidiRecording)
        return;

    auto takeFiles = engine.getRecordingFiles();
    engine.stopRecording();

    auto midiTrackCount = 0;

    if (wasMidiRecording)
    {
        engine.stopMidiRecording();
        auto recordedEvents = engine.takeRecordedMidiEvents();
        const auto engineSampleRate = engine.getSampleRate();

        juce::Array<int> recordedTrackIndices;
        for (const auto& event : recordedEvents)
            recordedTrackIndices.addIfNotAlreadyThere(event.trackIndex);

        for (auto trackIndex : recordedTrackIndices)
        {
            std::vector<WorkstationAudioEngine::RecordedMidiEvent> trackEvents;
            for (const auto& event : recordedEvents)
                if (event.trackIndex == trackIndex)
                    trackEvents.push_back(event);

            std::sort(trackEvents.begin(), trackEvents.end(),
                      [](const auto& a, const auto& b) { return a.samplePosition < b.samplePosition; });

            // Pair each note-on with the next note-off on the same channel/pitch, converting the
            // engine's absolute recording-clock sample position into clip-relative beats.
            std::vector<cs::MidiNoteEvent> notes;
            struct OpenNote { int channel; int pitch; size_t noteIndex; };
            std::vector<OpenNote> openNotes;

            for (const auto& event : trackEvents)
            {
                const auto& message = event.message;
                const auto eventSeconds = engineSampleRate > 0.0 ? (double) event.samplePosition / engineSampleRate : 0.0;
                const auto elapsedBeats = timelineModel.secondsToBeat(eventSeconds - transportStartTimelineSeconds);

                if (message.isNoteOn())
                {
                    cs::MidiNoteEvent note;
                    note.id = juce::Uuid().toString();
                    note.pitch = message.getNoteNumber();
                    note.velocity = message.getVelocity();
                    note.channel = message.getChannel();
                    note.startBeats = juce::jmax(0.0, elapsedBeats);
                    note.lengthBeats = 0.25;
                    notes.push_back(note);
                    openNotes.push_back({ note.channel, note.pitch, notes.size() - 1 });
                }
                else if (message.isNoteOff())
                {
                    for (auto it = openNotes.begin(); it != openNotes.end(); ++it)
                    {
                        if (it->channel == message.getChannel() && it->pitch == message.getNoteNumber())
                        {
                            auto& note = notes[it->noteIndex];
                            note.lengthBeats = juce::jmax(0.05, elapsedBeats - note.startBeats);
                            openNotes.erase(it);
                            break;
                        }
                    }
                }
            }

            timelineModel.setRecordingClipMidiNotes(trackIndex, std::move(notes));
        }

        midiTrackCount = recordedTrackIndices.size();
    }

    for (const auto& takeFile : takeFiles)
    {
        if (! takeFile.existsAsFile())
            continue;

        juce::String importError;
        auto importedFile = juce::File();
        if (! importedFile.existsAsFile())
        {
            transportBar.setStatusText(importError.isNotEmpty() ? importError
                                                                : "Recorded take could not be registered in the project library.");
            continue;
        }

        auto importedAssetId = "";
        auto importedAssetVersionId = "";

        const auto& clips = timelineModel.getClips();
        for (int clipIndex = 0; clipIndex < static_cast<int>(clips.size()); ++clipIndex)
        {
            if (clips[(size_t) clipIndex].file != takeFile)
                continue;

            cs::AssetRef assetRef;
            assetRef.id = importedAssetId;
            assetRef.versionId = importedAssetVersionId;
            assetRef.mode = cs::AssetReferenceMode::exact;
            assetRef.displayName = importedFile.getFileNameWithoutExtension().replace("-", " ");
            timelineModel.setClipAssetReference(clipIndex, assetRef);
            timelineModel.setClipFile(clipIndex, importedFile);
        }

        if (importedFile != takeFile && takeFile.existsAsFile())
            takeFile.deleteFile();
    }

    timelineModel.finishRecordingClip(timelineModel.getTransportSeconds());
    activeRecordingTrack = -1;
    trackerPanel.refreshTimelineView();
    midiSurface.setTransportState(false, false);

    const auto totalTrackCount = takeFiles.size() + midiTrackCount;
    transportBar.setStatusText("Recording stopped: " + juce::String(totalTrackCount) + " track(s).");
    recordView.setRecordingState(false, totalTrackCount == 1
                                             ? (takeFiles.size() == 1 ? takeFiles[0].getFileName() : juce::String("MIDI"))
                                             : juce::String(totalTrackCount) + " tracks");
    refreshRecentTakes();
    saveSessionToDisk();
}

void MainComponent::revealProjectFolder()
{
    if (! projectSession.isValid())
        return;

    projectSession.getContainerFile().revealToUser();
}

bool MainComponent::chooseStorageRoot(bool promptWhenAlreadyConfigured)
{
    juce::ignoreUnused(promptWhenAlreadyConfigured);
    suiteShellController.showSuiteSettings();
    transportBar.setStatusText("Configure Creation Station project storage in Creation Suite settings.");
    return false;
}

bool MainComponent::ensureStorageRootConfigured()
{
    if (suiteSettings.suiteVfsRoot.isNotEmpty())
        return true;

    transportBar.setStatusText("Configure Creation Station project storage in Creation Suite settings.");
    if (! chooseStorageRoot())
    {
        transportBar.setStatusText("Creation Station project storage is required before the studio can save projects or content.");
        return false;
    }

    return true;
}

bool MainComponent::ensureProjectSessionActive(juce::String& errorMessage)
{
    if (projectSession.isValid())
        return true;

    if (! ensureStorageRootConfigured())
    {
        errorMessage = "Creation Station project storage is not configured.";
        return false;
    }

    auto settingsFile = getAppSettingsFile();
    if (settingsFile.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(settingsFile))
        {
            auto settings = juce::ValueTree::fromXml(*xml);
            auto lastContainerPath = settings.getProperty("lastOpenedProjectContainer").toString();
            if (lastContainerPath.isNotEmpty())
            {
                juce::File lastContainer(lastContainerPath);
                if (lastContainer.existsAsFile())
                {
                    if (creation::assets::ProjectWorkspaceService::openProject(lastContainer, projectSession, errorMessage))
                    {
                        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
                        settingsPanel.setProjectMetadata(projectSession.getManifest());
                        refreshProjectAssets();
                        loadSessionFromDisk();
                        return true;
                    }
                }
            }
        }
    }

    juce::String listError;
    auto availableProjects = creation::assets::ProjectContainerService::listProjects(
        suiteSettings, creation::assets::SuiteAppDomain::station, listError);

    if (! availableProjects.isEmpty())
    {
        if (creation::assets::ProjectWorkspaceService::openProject(
                availableProjects.getFirst().containerFile, projectSession, errorMessage))
        {
            transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
            settingsPanel.setProjectMetadata(projectSession.getManifest());
            refreshProjectAssets();
            loadSessionFromDisk();
            return true;
        }
    }

    beginCreateNewProject();
    return false;
}



juce::File MainComponent::getAppSettingsFile() const
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return {};

    return suiteSettingsStore.getSuiteConfigDirectory().getChildFile("settings.xml");
}

void MainComponent::saveAppSettings()
{
    auto settingsFile = getAppSettingsFile();
    if (settingsFile.getFullPathName().isEmpty())
        return;

    juce::ValueTree state("CreationStationSettings");
    state.setProperty("formatVersion", 1, nullptr);
    state.setProperty("autoloadLastProject", autoloadLastProject, nullptr);
    if (projectSession.isValid())
        state.setProperty("lastOpenedProjectContainer", projectSession.getContainerFile().getFullPathName(), nullptr);
    state.setProperty("audioSystem", selectedStudioAudioSystem, nullptr);
    state.setProperty("audioInputDevice", selectedStudioInputDevice, nullptr);
    state.setProperty("audioOutputDevice", selectedStudioOutputDevice, nullptr);
    state.addChild(studioIOModel.createState(), -1, nullptr);

    juce::ValueTree vstPathsState("VstSearchPaths");
    for (const auto& path : vstPluginCatalog.getSearchPaths())
    {
        juce::ValueTree pathState("Path");
        pathState.setProperty("value", path, nullptr);
        vstPathsState.addChild(pathState, -1, nullptr);
    }
    state.addChild(vstPathsState, -1, nullptr);

    juce::ValueTree disabledMidiState("DisabledMidiInputDevices");
    for (const auto& deviceId : disabledMidiInputDeviceIds)
    {
        juce::ValueTree deviceState("Device");
        deviceState.setProperty("id", deviceId, nullptr);
        disabledMidiState.addChild(deviceState, -1, nullptr);
    }
    state.addChild(disabledMidiState, -1, nullptr);

    settingsFile.getParentDirectory().createDirectory();
    if (auto xml = state.createXml())
        xml->writeTo(settingsFile);
}

void MainComponent::loadAppSettings()
{
    auto settingsFile = getAppSettingsFile();
    if (! settingsFile.existsAsFile())
    {
        autoloadLastProject = true;
        loadSuiteAiProviderSettings();
        vstPluginCatalog.setSearchPaths(juce::StringArray());
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        return;
    }

    auto xml = juce::parseXML(settingsFile);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    if (! state.isValid() || state.getType() != juce::Identifier("CreationStationSettings"))
        return;

    selectedStudioAudioSystem = state.getProperty("audioSystem").toString();
    selectedStudioInputDevice = state.getProperty("audioInputDevice").toString();
    selectedStudioOutputDevice = state.getProperty("audioOutputDevice").toString();
    autoloadLastProject = true;
    auto legacyAiProviderSettings = aiProviderSettings;
    legacyAiProviderSettings.providerDisplayName = state.getProperty("aiProviderName", aiProviderSettings.providerDisplayName).toString();
    legacyAiProviderSettings.providerId = creation::services::SuiteAiProviderRuntime::normalizeProviderId(
        legacyAiProviderSettings.providerDisplayName);
    legacyAiProviderSettings.baseUrl = state.getProperty("aiBaseUrl", aiProviderSettings.baseUrl).toString();
    legacyAiProviderSettings.modelName = state.getProperty("aiModelName", aiProviderSettings.modelName).toString();
    legacyAiProviderSettings.apiKey = state.getProperty("aiApiKey", aiProviderSettings.apiKey).toString();

    if (auto studioState = state.getChildWithName("StudioIO"); studioState.isValid())
        studioIOModel.restoreState(studioState);

    if (auto vstPathsState = state.getChildWithName("VstSearchPaths"); vstPathsState.isValid())
    {
        juce::StringArray paths;
        for (const auto child : vstPathsState)
            paths.add(child.getProperty("value").toString());
        paths.trim();
        paths.removeEmptyStrings();
        paths.removeDuplicates(false);
        vstPluginCatalog.setSearchPaths(paths);
    }

    disabledMidiInputDeviceIds.clear();
    if (auto disabledMidiState = state.getChildWithName("DisabledMidiInputDevices"); disabledMidiState.isValid())
        for (const auto child : disabledMidiState)
            disabledMidiInputDeviceIds.add(child.getProperty("id").toString());

    aiProviderSettings = legacyAiProviderSettings;
    loadSuiteAiProviderSettings(true);
    settingsPanel.setAiProviderSettings(aiProviderSettings);
    engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
}

void MainComponent::applySelectedAudioDeviceSettings()
{
    if (selectedStudioAudioSystem.isNotEmpty())
        deviceManager.setCurrentAudioDeviceType(selectedStudioAudioSystem, true);

    // engine.attachToDevice() auto-enables every non-control-surface MIDI input device as a
    // friendly default (so a freshly plugged-in keyboard just works) - apply the user's saved
    // exceptions on top of that default rather than replacing it, so newly connected devices
    // keep working out of the box while explicit opt-outs still stick across restarts.
    for (const auto& deviceId : disabledMidiInputDeviceIds)
        deviceManager.setMidiInputDeviceEnabled(deviceId, false);
    refreshMidiDeviceSettings();

    if (selectedStudioInputDevice.isEmpty() && selectedStudioOutputDevice.isEmpty())
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (selectedStudioInputDevice.isNotEmpty())
    {
        setup.inputDeviceName = selectedStudioInputDevice;
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        for (int channel = 0; channel < 32; ++channel)
            setup.inputChannels.setBit(channel);
    }

    if (selectedStudioOutputDevice.isNotEmpty())
    {
        setup.outputDeviceName = selectedStudioOutputDevice;
        setup.useDefaultOutputChannels = true;
    }

    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
        transportBar.setStatusText("Audio restore: " + error);
}

void MainComponent::refreshMidiDeviceSettings()
{
    juce::Array<SettingsPanel::MidiDeviceInfo> devices;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        SettingsPanel::MidiDeviceInfo info;
        info.id = device.identifier;
        info.name = device.name;
        info.enabled = deviceManager.isMidiInputDeviceEnabled(device.identifier);
        info.routedTrackIndex = -1;

        for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
        {
            if (engine.getTrackMidiInputDeviceId(trackIndex) == device.identifier)
            {
                info.routedTrackIndex = trackIndex;
                break;
            }
        }

        devices.add(info);
    }

    juce::StringArray trackNames;
    for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
        trackNames.add(engine.getTrackName(trackIndex));

    settingsPanel.setMidiInputDevices(devices, trackNames);
}

juce::ValueTree MainComponent::createProjectStateForSave()
{
    auto state = engine.createSessionState();
    state.setProperty("bankOffset", mixerPanel.getBankOffset(), nullptr);
    state.setProperty("insertContext", pluginRackBar.isTrackContext() ? "track" : "master", nullptr);
    state.setProperty("insertTrackIndex", pluginRackBar.getTrackIndex(), nullptr);
    state.setProperty("graphEnabled", engine.isGraphEnabled(), nullptr);
    state.setProperty("graphInput", engine.getGraphInput(), nullptr);
    state.setProperty("graphSourceFrequency", engine.getGraphSourceFrequency(), nullptr);
    state.setProperty("graphDrive", engine.getGraphDrive(), nullptr);
    state.setProperty("graphTone", engine.getGraphTone(), nullptr);
    state.setProperty("graphEcho", engine.getGraphEcho(), nullptr);
    state.setProperty("graphWidth", engine.getGraphWidth(), nullptr);
    state.setProperty("arrangeVisibleTracks", arrangeView.getVisibleTrackCount(), nullptr);
    state.setProperty("workspaceMode", static_cast<int>(activeMode), nullptr);
    state.setProperty("dslSource", dslPanel.getSourceText(), nullptr);
    state.setProperty("selectedClipIndex", selectedClipIndex, nullptr);
    state.addChild(arrangeView.createState(), -1, nullptr);
    state.addChild(signalLabPanel.createState(), -1, nullptr);
    state.addChild(graphPanel.createState(), -1, nullptr);
    state.addChild(scorePanel.createState(), -1, nullptr);
    state.addChild(timelineModel.createState(), -1, nullptr);
    juce::ValueTree undoState("TimelineUndoStack");
    for (const auto& undoTimelineState : timelineUndoStack)
        undoState.addChild(undoTimelineState.createCopy(), -1, nullptr);
    state.addChild(undoState, -1, nullptr);
    juce::ValueTree redoState("TimelineRedoStack");
    for (const auto& redoTimelineState : timelineRedoStack)
        redoState.addChild(redoTimelineState.createCopy(), -1, nullptr);
    state.addChild(redoState, -1, nullptr);

    return state;
}

void MainComponent::remapTemplateStateFilesToCurrentProject(juce::ValueTree& state) const
{
    if (! projectSession.isValid())
        return;

    juce::StringPairArray filesByName;
    for (const auto& asset : projectSession.getManifest().assetCatalog.query({}))
    {
        // Map the logical filename -> materialized path so the template remapper can work
        auto fileName = asset.logicalPath.fromLastOccurrenceOf("/", false, false).toLowerCase();
        if (fileName.isNotEmpty() && ! filesByName.containsKey(fileName))
        {
            juce::String matError;
            creation::assets::MaterializedAssetLease lease;
            if (projectSession.materializeEntry(suiteSettings, asset.logicalPath,
                                                creation::assets::MaterializationAccess::readOnly,
                                                lease, matError))
            {
                filesByName.set(fileName, lease.materializedFile.getFullPathName());
            }
        }
    }

    std::function<void(juce::ValueTree&)> remapTree = [&](juce::ValueTree& tree)
    {
        if (tree.hasProperty("file"))
        {
            auto oldFileName = juce::File(tree.getProperty("file").toString()).getFileName().toLowerCase();
            auto newPath = filesByName[oldFileName];
            if (newPath.isNotEmpty())
                tree.setProperty("file", newPath, nullptr);
        }

        for (int index = 0; index < tree.getNumChildren(); ++index)
        {
            auto child = tree.getChild(index);
            remapTree(child);
        }
    };

    remapTree(state);
}

void MainComponent::saveSessionToDisk(bool userInitiated)
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    if (! projectSession.isValid())
    {
        if (userInitiated)
            transportBar.setStatusText("There is no active project to save yet.");
        return;
    }

    auto state = createProjectStateForSave();
    if (auto xml = state.createXml())
    {
        auto xmlString = xml->toString();
        juce::MemoryBlock xmlBlock(xmlString.toRawUTF8(), xmlString.getNumBytesAsUTF8());
        projectSession.writeEntry("session.xml", xmlBlock);
    }
    lastObservedPluginStateSignature = engine.createHostedPluginStateSignature();
    pluginStateAutosavePending = false;

    // Startup restore now uses the active project's session.xml. Remove legacy config snapshot.
    auto legacySessionFile = suiteSettingsStore.getSuiteConfigDirectory().getChildFile("session.xml");
    if (legacySessionFile.existsAsFile())
        legacySessionFile.deleteFile();

    juce::String packageError;
    if (! projectSession.commit(packageError))
    {
        projectDirty = true;
        transportBar.setStatusText("Project package save failed: " + packageError);
        return;
    }

    projectDirty = false;
}

bool MainComponent::prepareTrackerPlayback()
{
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double lastClipEnd = 0.0;
    juce::String errorMessage;

    if (! buildTrackerPlaybackTargets(targets, lastClipEnd, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return false;
    }

    if (! engine.setTrackerPlaybackClips(targets, errorMessage))
    {
        transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not prepare tracker playback.");
        return false;
    }

    refreshMidiPlaybackClips();

    // Play always starts from wherever the playhead currently is - no auto-snap to the
    // first clip. (Previously this reset the transport position whenever it was at or past
    // the last clip's end, which silently discarded the user's chosen playhead position -
    // including immediately before recording, since onRecord also flows through here.)
    transportBar.setStatusText("Tracker playback ready: " + juce::String(targets.size()) + " clip(s).");
    return true;
}

void MainComponent::refreshTrackerPlaybackClips()
{
    // Keeps the engine's cached tracker playback clip list in sync with timelineModel
    // immediately after any edit (delete/split/duplicate/move/undo/redo), not just when
    // Play is next pressed. Without this, a deleted clip's audio keeps being cached by the
    // engine and can still be heard - and captured into a new take - the next time
    // anything triggers playback, including pressing Record.
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double lastClipEnd = 0.0;
    juce::String errorMessage;
    buildTrackerPlaybackTargets(targets, lastClipEnd, errorMessage);

    juce::String engineError;
    engine.setTrackerPlaybackClips(targets, engineError);

    refreshMidiPlaybackClips();
}

void MainComponent::refreshMidiPlaybackClips()
{
    juce::Array<WorkstationAudioEngine::MidiPlaybackClip> midiClips;

    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.kind != cs::ClipKind::midi || clip.midiNotes.empty())
            continue;

        WorkstationAudioEngine::MidiPlaybackClip midiClip;
        midiClip.trackIndex = clip.trackIndex;
        midiClip.startSeconds = clip.startSeconds;
        midiClip.durationSeconds = clip.durationSeconds;
        midiClip.notes = clip.midiNotes;
        midiClips.add(std::move(midiClip));
    }

    engine.setTrackerMidiClips(midiClips);
}

bool MainComponent::buildTrackerPlaybackTargets(juce::Array<WorkstationAudioEngine::PlaybackClipTarget>& targets,
                                                double& durationSeconds,
                                                juce::String& errorMessage) const
{
    targets.clear();
    durationSeconds = 0.0;

    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.recording)
            continue;

        if (clip.kind == cs::ClipKind::midi)
        {
            // Offline-rendering a MIDI clip means driving the instrument plugin through
            // hundreds of processBlock calls back-to-back with no real-time pacing. At least
            // one real-world plugin (a sample-streaming drum sampler) crashes reliably under
            // that load - confirmed by an identical crash signature to the earlier idle-audio
            // instability. Disabled until a safer rendering approach exists; see task #7.
            continue;
        }

        auto clipFile = clip.file;
        if (! clipFile.existsAsFile() && clip.assetId.isNotEmpty())
        {
            auto assetOpt = resolveTimelineClipAsset(clip);
            if (assetOpt.has_value())
            {
                juce::String matError;
                creation::assets::MaterializedAssetLease lease;
                if (projectSession.materializeEntry(suiteSettings, assetOpt->logicalPath,
                                                    creation::assets::MaterializationAccess::readOnly,
                                                    lease, matError))
                {
                    clipFile = lease.materializedFile;
                }
            }
        }

        if (! clipFile.existsAsFile())
            continue;

        WorkstationAudioEngine::PlaybackClipTarget target;
        target.trackIndex = clip.trackIndex;
        target.file = clipFile;
        target.startSeconds = clip.startSeconds;
        target.sourceStartSeconds = clip.sourceStartSeconds;
        target.durationSeconds = clip.durationSeconds;
        targets.add(target);
        durationSeconds = juce::jmax(durationSeconds, clip.startSeconds + clip.durationSeconds);
    }

    if (targets.isEmpty())
    {
        errorMessage = "No recorded or rendered clips are available.";
        return false;
    }

    return true;
}

void MainComponent::previewScrubAudioAt(double timelineSeconds)
{
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> fullTargets;
    double fullDurationSeconds = 0.0;
    juce::String errorMessage;
    if (! buildTrackerPlaybackTargets(fullTargets, fullDurationSeconds, errorMessage))
        return;

    constexpr double scrubPreviewLeadSeconds = 0.03;
    constexpr double scrubPreviewLengthSeconds = 0.16;

    auto windowStart = juce::jmax(0.0, timelineSeconds - scrubPreviewLeadSeconds);
    auto windowEnd = windowStart + scrubPreviewLengthSeconds;

    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> scrubTargets;
    for (const auto& target : fullTargets)
    {
        auto clipStart = target.startSeconds;
        auto clipEnd = target.startSeconds + target.durationSeconds;
        auto overlapStart = juce::jmax(clipStart, windowStart);
        auto overlapEnd = juce::jmin(clipEnd, windowEnd);

        if (overlapEnd <= overlapStart)
            continue;

        WorkstationAudioEngine::PlaybackClipTarget scrubTarget = target;
        scrubTarget.startSeconds = overlapStart - windowStart;
        scrubTarget.sourceStartSeconds = target.sourceStartSeconds + (overlapStart - clipStart);
        scrubTarget.durationSeconds = overlapEnd - overlapStart;
        scrubTargets.add(std::move(scrubTarget));
    }

    if (scrubTargets.isEmpty())
    {
        engine.stopAssetPreview();
        return;
    }

    WorkstationAudioEngine::RenderSettings settings;
    settings.sampleRate = engine.getSampleRate() > 0.0 ? engine.getSampleRate() : 48000.0;
    settings.blockSize = 256;
    settings.normalizePeak = false;

    juce::AudioBuffer<float> previewBuffer;
    if (! engine.renderTrackerMixToBuffer(scrubTargets, scrubPreviewLengthSeconds, settings, previewBuffer, errorMessage))
        return;

    engine.previewGeneratedBuffer(previewBuffer, settings.sampleRate, errorMessage);
}

void MainComponent::pushTimelineUndoState()
{
    pushTimelineUndoState(timelineModel.createState());
}

void MainComponent::pushTimelineUndoState(const juce::ValueTree& stateBeforeEdit)
{
    timelineUndoStack.push_back(stateBeforeEdit.createCopy());
    if (timelineUndoStack.size() > 100)
        timelineUndoStack.erase(timelineUndoStack.begin());

    timelineRedoStack.clear();
}

void MainComponent::restoreTimelineEditState(const juce::ValueTree& state, const juce::String& statusText)
{
    // Most undo entries (clip split/duplicate/delete/move) are a bare "Timeline" tree and only
    // need timelineModel restored, as before. Track-level operations (add/remove) push a wrapping
    // "UndoSnapshot" that also carries the engine's full session state (gain/pan/mute/solo/plugin
    // chains), since removing a track destroys engine-side state that timelineModel never held.
    if (state.hasType(juce::Identifier("UndoSnapshot")))
    {
        if (auto timelineState = state.getChildWithName("Timeline"); timelineState.isValid())
            timelineModel.restoreState(timelineState);

        if (auto engineState = state.getChildWithName("CreationStationSession"); engineState.isValid())
        {
            juce::String engineError;
            engine.restoreSessionState(engineState, engineError);
        }
    }
    else
    {
        timelineModel.restoreState(state);
    }

    selectedClipIndex = -1;
    syncTrackViews();
    trackerPanel.setSelectedClip(-1);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText(statusText);
}

void MainComponent::undoTimelineEdit()
{
    if (timelineUndoStack.empty())
        return;

    timelineRedoStack.push_back(timelineModel.createState());
    if (timelineRedoStack.size() > 100)
        timelineRedoStack.erase(timelineRedoStack.begin());

    auto stateToRestore = timelineUndoStack.back().createCopy();
    timelineUndoStack.pop_back();
    restoreTimelineEditState(stateToRestore, "Tracker edit undone.");
}

void MainComponent::redoTimelineEdit()
{
    if (timelineRedoStack.empty())
        return;

    timelineUndoStack.push_back(timelineModel.createState());
    if (timelineUndoStack.size() > 100)
        timelineUndoStack.erase(timelineUndoStack.begin());

    auto stateToRestore = timelineRedoStack.back().createCopy();
    timelineRedoStack.pop_back();
    restoreTimelineEditState(stateToRestore, "Tracker edit redone.");
}

void MainComponent::splitClipAt(int clipIndex, double splitSeconds)
{
    auto stateBeforeEdit = timelineModel.createState();
    if (! timelineModel.splitClip(clipIndex, splitSeconds))
    {
        transportBar.setStatusText("Split needs the playhead inside a clip.");
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    selectedClipIndex = juce::jmin(clipIndex + 1, static_cast<int>(timelineModel.getClips().size()) - 1);
    trackerPanel.setSelectedClip(selectedClipIndex);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText("Clip split.");
}

void MainComponent::duplicateClip(int clipIndex)
{
    auto stateBeforeEdit = timelineModel.createState();
    if (! timelineModel.duplicateClip(clipIndex))
    {
        transportBar.setStatusText("No clip selected to duplicate.");
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    selectedClipIndex = static_cast<int>(timelineModel.getClips().size()) - 1;
    trackerPanel.setSelectedClip(selectedClipIndex);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText("Clip duplicated.");
}

void MainComponent::deleteClip(int clipIndex)
{
    auto stateBeforeEdit = timelineModel.createState();
    if (! timelineModel.deleteClip(clipIndex))
    {
        transportBar.setStatusText("No clip selected to delete.");
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    selectedClipIndex = -1;
    trackerPanel.setSelectedClip(-1);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText("Clip deleted.");
}

void MainComponent::renameClip(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(timelineModel.getClips().size())))
    {
        transportBar.setStatusText("No clip selected to rename.");
        return;
    }

    const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
    auto* renameDialog = new juce::AlertWindow("Rename Clip",
                                               "Give this clip a useful name.",
                                               juce::MessageBoxIconType::QuestionIcon);
    renameDialog->addTextEditor("clipName", clip.displayName);
    renameDialog->addButton("Rename", 1);
    renameDialog->addButton("Cancel", 0);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    renameDialog->enterModalState(true,
                                  juce::ModalCallbackFunction::create([safeThis, renameDialog, clipIndex](int result) mutable
                                  {
                                      std::unique_ptr<juce::AlertWindow> dialog(renameDialog);
                                      if (result != 1 || safeThis == nullptr)
                                          return;

                                      auto newName = dialog->getTextEditorContents("clipName").trim();
                                      if (newName.isEmpty())
                                          return;

                                      auto stateBeforeEdit = safeThis->timelineModel.createState();
                                      safeThis->timelineModel.setClipDisplayName(clipIndex, newName);
                                      safeThis->pushTimelineUndoState(stateBeforeEdit);
                                      safeThis->selectedClipIndex = clipIndex;
                                      safeThis->trackerPanel.setSelectedClip(clipIndex);
                                      safeThis->trackerPanel.refreshTimelineView();
                                      safeThis->projectDirty = true;
                                      safeThis->saveSessionToDisk();
                                      safeThis->transportBar.setStatusText("Clip renamed.");
                                  }),
                                  true);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    return handleGlobalKeyPress(key);
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return handleGlobalKeyPress(key);
}

void MainComponent::parentHierarchyChanged()
{
    // Register as a key listener on the top-level window so app shortcuts (undo/redo/etc.) fire
    // regardless of which child currently holds keyboard focus - or when the previously focused
    // component (e.g. a just-removed track header) has been destroyed and nothing holds focus.
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
    {
        top->removeKeyListener(this);
        top->addKeyListener(this);
    }
}

bool MainComponent::handleGlobalKeyPress(const juce::KeyPress& key)
{
    auto mods = key.getModifiers();
    auto code = key.getKeyCode();

    // JUCE returns the *uppercase* letter for alphabetic keys from getKeyCode(); normalise so the
    // shortcuts fire regardless of case (comparing against lowercase 'z' alone silently never matched).
    auto letter = (juce::juce_wchar) juce::CharacterFunctions::toUpperCase((juce::juce_wchar) code);

    if (mods.isCommandDown() && ! mods.isShiftDown() && letter == 'Z')
    {
        undoTimelineEdit();
        return true;
    }

    if ((mods.isCommandDown() && ! mods.isShiftDown() && letter == 'Y')
        || (mods.isCommandDown() && mods.isShiftDown() && letter == 'Z'))
    {
        redoTimelineEdit();
        return true;
    }

    if (mods.isCommandDown() && ! mods.isShiftDown() && letter == 'D' && selectedClipIndex >= 0)
    {
        duplicateClip(selectedClipIndex);
        return true;
    }

    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && selectedClipIndex >= 0)
    {
        deleteClip(selectedClipIndex);
        return true;
    }

    if (code == juce::KeyPress::F2Key && selectedClipIndex >= 0)
    {
        renameClip(selectedClipIndex);
        return true;
    }

    if (mods.isCommandDown() && mods.isShiftDown() && letter == 'S' && selectedClipIndex >= 0)
    {
        splitClipAt(selectedClipIndex, timelineModel.getTransportSeconds());
        return true;
    }

    return false;
}

void MainComponent::loadSessionFromDisk()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    juce::MemoryBlock sessionData;
    juce::ValueTree state;
    if (projectSession.isValid() && projectSession.readEntry("session.xml", sessionData))
    {
        state = juce::ValueTree::fromXml(juce::String::createStringFromData(sessionData.getData(), (int)sessionData.getSize()));
    }
    if (! state.isValid())
        return;

    loadAppSettings();
    applySelectedAudioDeviceSettings();

    juce::String errorMessage;
    engine.restoreSessionState(state, errorMessage);

    if (auto timelineState = state.getChildWithName("Timeline"); timelineState.isValid())
        timelineModel.restoreState(timelineState);

    resolveTrackerClipAssetFiles();

    transportBar.loopButton.setToggleState(timelineModel.isLoopEnabled(), juce::dontSendNotification);

    timelineUndoStack.clear();
    if (auto undoState = state.getChildWithName("TimelineUndoStack"); undoState.isValid())
    {
        for (const auto child : undoState)
            if (child.hasType("Timeline"))
                timelineUndoStack.push_back(child.createCopy());

        while (timelineUndoStack.size() > 100)
            timelineUndoStack.erase(timelineUndoStack.begin());
    }

    timelineRedoStack.clear();
    if (auto redoState = state.getChildWithName("TimelineRedoStack"); redoState.isValid())
    {
        for (const auto child : redoState)
            if (child.hasType("Timeline"))
                timelineRedoStack.push_back(child.createCopy());

        while (timelineRedoStack.size() > 100)
            timelineRedoStack.erase(timelineRedoStack.begin());
    }

    auto bankOffset = (int) state.getProperty("bankOffset", 0);
    mixerPanel.setBankOffset(bankOffset);
    midiSurface.setBankOffset(bankOffset);

    auto insertContext = state.getProperty("insertContext").toString();
    auto trackIndex = (int) state.getProperty("insertTrackIndex", -1);

    if (insertContext == "track" && juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
    }
    else
    {
        pluginRackBar.setContextMaster();
        mixerPanel.setSelectedChannel(-1);
    }

    engine.setGraphEnabled((bool) state.getProperty("graphEnabled", true));
    engine.setGraphInput((float) state.getProperty("graphInput", engine.getGraphInput()));
    engine.setGraphSourceFrequency((float) state.getProperty("graphSourceFrequency", engine.getGraphSourceFrequency()));
    engine.setGraphDrive((float) state.getProperty("graphDrive", engine.getGraphDrive()));
    engine.setGraphTone((float) state.getProperty("graphTone", engine.getGraphTone()));
    engine.setGraphEcho((float) state.getProperty("graphEcho", engine.getGraphEcho()));
    engine.setGraphWidth((float) state.getProperty("graphWidth", engine.getGraphWidth()));
    graphPanel.setEnabled(engine.isGraphEnabled());
    graphPanel.setInput(engine.getGraphInput());
    graphPanel.setOscillatorFrequency(engine.getGraphSourceFrequency());
    graphPanel.setDrive(engine.getGraphDrive());
    graphPanel.setTone(engine.getGraphTone());
    graphPanel.setEcho(engine.getGraphEcho());
    graphPanel.setWidth(engine.getGraphWidth());
    graphPanel.setOutputLevel(engine.getMasterGain());

    if (auto dslSource = state.getProperty("dslSource").toString(); dslSource.isNotEmpty())
        dslPanel.setSourceText(dslSource);

    refreshProjectAssets();

    if (auto graphState = state.getChildWithName("NodeGraph"); graphState.isValid())
        graphPanel.restoreState(graphState);

    if (! graphPanel.hasNode("Oscillator"))
    {
        engine.setGraphInput(0.0f);
        graphPanel.setInput(0.0f);
    }

    engine.setGraphVstEnabled(graphPanel.isVstEnabled());
    engine.setGraphVstMix(graphPanel.getVstMix());
    if (engine.hasGraphVstPlugin() && engine.getGraphVstPluginName().isNotEmpty())
        graphPanel.setAssignedVstPlugin(engine.getGraphVstPluginName(), engine.getGraphVstPluginFile().getFullPathName());

    if (auto arrangeState = state.getChildWithName("ArrangeView"); arrangeState.isValid())
        arrangeView.restoreState(arrangeState);

    if (auto signalState = state.getChildWithName("SignalLab"); signalState.isValid())
        signalLabPanel.restoreState(signalState);

    if (auto scoreState = state.getChildWithName("ScoreView"); scoreState.isValid())
        scorePanel.restoreState(scoreState);

    syncTrackViews();

    selectedClipIndex = (int) state.getProperty("selectedClipIndex", -1);
    if (! juce::isPositiveAndBelow(selectedClipIndex, static_cast<int>(timelineModel.getClips().size())))
        selectedClipIndex = -1;
    trackerPanel.setSelectedClip(selectedClipIndex);

    armedTracks.resize((size_t) engine.getTrackCount(), false);
    monitoredTracks.resize((size_t) engine.getTrackCount(), false);
    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        armedTracks[(size_t) index] = engine.isTrackRecordingArmed(index);
        monitoredTracks[(size_t) index] = engine.isTrackMonitoringEnabled(index);
        trackerPanel.setTrackArmed(index, armedTracks[(size_t) index]);
        trackerPanel.setTrackMonitored(index, monitoredTracks[(size_t) index]);
    }

    auto visibleTracks = (int) state.getProperty("arrangeVisibleTracks", arrangeView.getVisibleTrackCount());
    arrangeView.setVisibleTrackCount(engine.getTrackCount() == 0 ? 0 : juce::jlimit(1, engine.getTrackCount(), visibleTracks));
    recordView.setTrackCount(engine.getTrackCount());
    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        auto trackName = engine.getTrackName(index);
        arrangeView.setTrackName(index, trackName);
        if (index < engine.getTrackCount())
            recordView.setTrackName(index, trackName);
    }

    auto savedMode = (int) state.getProperty("workspaceMode", static_cast<int>(WorkspaceMode::tracker));
    if (savedMode > static_cast<int>(WorkspaceMode::sampler))
        savedMode = static_cast<int>(WorkspaceMode::tracker);

    setWorkspaceMode(static_cast<WorkspaceMode>(juce::jlimit(0, static_cast<int>(WorkspaceMode::sampler), savedMode)));

    refreshInsertRack();
    transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
    refreshRecentTakes();
    refreshFoleyArrangement();

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::Timer::callAfterDelay(300, [safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->engine.reapplyHostedPluginStates();
        safeThis->lastObservedPluginStateSignature = safeThis->engine.createHostedPluginStateSignature();
    });

    lastObservedPluginStateSignature = engine.createHostedPluginStateSignature();
    pluginStateAutosavePending = false;
    pluginStateLastPollWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    projectDirty = false;

}

void MainComponent::syncTrackViews()
{
    auto trackCount = engine.getTrackCount();

    refreshTrackInputSources();
    trackerPanel.setTimingInfo(timelineModel.getTempoBpm(),
                               timelineModel.getTimeSignatureNumerator(),
                               timelineModel.getTimeSignatureDenominator(),
                               timelineModel.getMusicalKey());
    timelineModel.setTrackCount(trackCount);
    trackerPanel.setTrackCount(trackCount);
    arrangeView.setTotalTrackCount(trackCount);
    recordView.setTrackCount(trackCount);
    midiSurface.setTrackCount(trackCount);
    mixerPanel.setChannelCount(trackCount);

    if ((int) armedTracks.size() < trackCount)
        armedTracks.resize((size_t) trackCount, false);
    else if ((int) armedTracks.size() > trackCount)
        armedTracks.resize((size_t) trackCount);
    if ((int) automationLastManualWriteWallSeconds.size() < trackCount)
        automationLastManualWriteWallSeconds.resize((size_t) trackCount, 0.0);
    else if ((int) automationLastManualWriteWallSeconds.size() > trackCount)
        automationLastManualWriteWallSeconds.resize((size_t) trackCount);
    if ((int) monitoredTracks.size() < trackCount)
        monitoredTracks.resize((size_t) trackCount, false);
    else if ((int) monitoredTracks.size() > trackCount)
        monitoredTracks.resize((size_t) trackCount);

    for (int index = 0; index < trackCount; ++index)
    {
        auto trackName = engine.getTrackName(index);
        timelineModel.setTrackName(index, trackName);
        timelineModel.setTrackChannelMode(index, engine.isTrackStereoEnabled(index) ? cs::TrackChannelMode::stereo
                                                                                    : cs::TrackChannelMode::mono);
        trackerPanel.setTrackName(index, trackName);
        trackerPanel.setTrackKind(index, timelineModel.getTrackKind(index));
        engine.setTrackIsMidiKind(index, timelineModel.getTrackKind(index) == cs::TrackKind::midi);
        engine.setTrackIsAutomationKind(index, timelineModel.getTrackKind(index) == cs::TrackKind::automation);
        if (timelineModel.getTrackKind(index) == cs::TrackKind::automation)
            pushAutomationDataToEngine(index);
        engine.setTrackParentIndex(index, timelineModel.getTrackParent(index));
        trackerPanel.setTrackIndented(index, timelineModel.getTrackParent(index) >= 0);
        trackerPanel.setTrackAccentColour(index, computeTrackAccentColour(index));
        trackerPanel.setTrackStereo(index, engine.isTrackStereoEnabled(index));
        arrangeView.setTrackName(index, trackName);
        arrangeView.setTrackKind(index, timelineModel.getTrackKind(index));
        recordView.setTrackName(index, trackName);
        mixerPanel.setChannelName(index, trackName);
        midiSurface.setChannelName(index, trackName);
        trackerPanel.setTrackGain(index, engine.getTrackGain(index));
        trackerPanel.setTrackMuted(index, engine.isTrackMuted(index));
        trackerPanel.setTrackSoloed(index, engine.isTrackSoloed(index));
        trackerPanel.setTrackArmed(index, juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index]);
        trackerPanel.setTrackMonitored(index, juce::isPositiveAndBelow(index, (int) monitoredTracks.size()) && monitoredTracks[(size_t) index]);
        trackerPanel.setTrackInput(index, timelineModel.getTrackKind(index) == cs::TrackKind::midi
                                              ? engine.getTrackMidiInputChannel(index)
                                              : studioIOModel.getInputIndexForChannel(engine.getTrackInputChannel(index)));
        trackerPanel.setTrackFxSummary(index, engine.getTrackPluginCount(index));
        engine.setTrackRecordingArmed(index, juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index]);
        engine.setTrackMonitoringEnabled(index, juce::isPositiveAndBelow(index, (int) monitoredTracks.size()) && monitoredTracks[(size_t) index]);
        trackerPanel.setTrackLevel(index, engine.getTrackLevel(index));

        if (timelineModel.getTrackKind(index) == cs::TrackKind::automation)
        {
            auto target = timelineModel.getAutomationTarget(index);
            trackerPanel.setAutomationTargetLabel(index, target.displayName);
            trackerPanel.setAutomationRecordMode(index, timelineModel.getAutomationRecordMode(index));
            trackerPanel.setAutomationRecordingRate(index, timelineModel.getAutomationRecordingRate(index));
        }
    }
}

void MainComponent::pushAutomationDataToEngine(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto target = timelineModel.getAutomationTarget(trackIndex);

    std::vector<cs::AutomationPoint> points;
    if (auto* stored = timelineModel.getAutomationPoints(trackIndex))
        points = *stored;

    engine.setTrackAutomationData(trackIndex, target, points);
}

void MainComponent::recordAutomationWriteIfArmed(int targetTrackIndex, cs::AutomationTargetKind kind, float normalizedValue)
{
    if (! engine.isPlaying())
        return;

    auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (int automationTrackIndex = 0; automationTrackIndex < timelineModel.getTrackCount(); ++automationTrackIndex)
    {
        if (timelineModel.getTrackKind(automationTrackIndex) != cs::TrackKind::automation)
            continue;

        if (! juce::isPositiveAndBelow(automationTrackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) automationTrackIndex])
            continue;

        auto target = timelineModel.getAutomationTarget(automationTrackIndex);
        if (target.kind != kind || target.targetTrackIndex != targetTrackIndex)
            continue;

        // Suspend this lane's own playback of its (about-to-be-overwritten) curve for as long as
        // it's being actively written - otherwise the audio-thread automation pass and this
        // manual write fight over the same value every block.
        engine.setTrackAutomationWriteActive(automationTrackIndex, true);
        if (juce::isPositiveAndBelow(automationTrackIndex, (int) automationLastManualWriteWallSeconds.size()))
            automationLastManualWriteWallSeconds[(size_t) automationTrackIndex] = nowSeconds;

        auto mergeToleranceSeconds = 1.0 / juce::jmax(1, timelineModel.getAutomationRecordingRate(automationTrackIndex));
        timelineModel.addOrUpdateAutomationPoint(automationTrackIndex, timelineModel.getTransportSeconds(),
                                                 juce::jlimit(0.0f, 1.0f, normalizedValue), mergeToleranceSeconds);
        pushAutomationDataToEngine(automationTrackIndex);
        trackerPanel.refreshTimelineView();
    }
}

void MainComponent::updateAutomationRecordModes()
{
    if (! engine.isPlaying())
    {
        // Passes don't carry a write-active lane over into the next Play - Latch/Write both
        // reset naturally on stop, matching how a real DAW ends a recording pass.
        for (int trackIndex = 0; trackIndex < timelineModel.getTrackCount(); ++trackIndex)
            if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::automation)
                engine.setTrackAutomationWriteActive(trackIndex, false);
        return;
    }

    constexpr double touchReleaseTimeoutSeconds = 0.2;
    auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (int trackIndex = 0; trackIndex < timelineModel.getTrackCount(); ++trackIndex)
    {
        if (timelineModel.getTrackKind(trackIndex) != cs::TrackKind::automation)
            continue;

        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) trackIndex])
            continue;

        auto target = timelineModel.getAutomationTarget(trackIndex);
        if (target.kind == cs::AutomationTargetKind::none)
            continue;

        auto mode = timelineModel.getAutomationRecordMode(trackIndex);

        if (mode == cs::AutomationRecordMode::write)
        {
            // Write mode records continuously for the whole pass, touched or not - sample
            // whatever the target's live value currently is (manual writes above may have just
            // set it; otherwise it's whatever it already was) and bake it into the curve.
            engine.setTrackAutomationWriteActive(trackIndex, true);

            float currentValue = 0.5f;
            if (target.kind == cs::AutomationTargetKind::trackVolume)
                currentValue = engine.getTrackGain(target.targetTrackIndex);
            else if (target.kind == cs::AutomationTargetKind::trackPan)
                currentValue = juce::jmap(engine.getTrackPan(target.targetTrackIndex), -1.0f, 1.0f, 0.0f, 1.0f);
            else if (target.kind == cs::AutomationTargetKind::pluginParameter)
                currentValue = engine.getTrackPluginParameterValue(target.targetTrackIndex, target.pluginSlotIndex, target.pluginParameterIndex);

            auto mergeToleranceSeconds = 1.0 / juce::jmax(1, timelineModel.getAutomationRecordingRate(trackIndex));
            timelineModel.addOrUpdateAutomationPoint(trackIndex, timelineModel.getTransportSeconds(),
                                                     juce::jlimit(0.0f, 1.0f, currentValue), mergeToleranceSeconds);
            pushAutomationDataToEngine(trackIndex);
            trackerPanel.refreshTimelineView();
        }
        else if (mode == cs::AutomationRecordMode::touch)
        {
            // Touch releases (resumes normal curve playback) once nothing has manually moved the
            // target for a short idle period - Latch deliberately has no such release here.
            if (juce::isPositiveAndBelow(trackIndex, (int) automationLastManualWriteWallSeconds.size())
                && nowSeconds - automationLastManualWriteWallSeconds[(size_t) trackIndex] > touchReleaseTimeoutSeconds)
                engine.setTrackAutomationWriteActive(trackIndex, false);
        }
    }
}

bool MainComponent::performTrackMove(int trackIndex, int destinationIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()))
        return false;

    // Computed BEFORE the move and reused for the engine call below, so both arrays agree on
    // exactly which range moved - TimelineModel::moveTrackGroup decides this the same way
    // internally, but doesn't hand the value back.
    auto blockLength = timelineModel.getTrackKind(trackIndex) == cs::TrackKind::folder
                          ? timelineModel.getFolderBlockLength(trackIndex)
                          : 1;

    if (! timelineModel.moveTrackGroup(trackIndex, destinationIndex))
        return false;

    engine.moveTrackRange(trackIndex, blockLength, destinationIndex);
    return true;
}

juce::Colour MainComponent::computeTrackAccentColour(int trackIndex) const
{
    static const juce::Colour palette[] = {
        juce::Colour(0xff67e8a5), juce::Colour(0xff74caff), juce::Colour(0xffffd166),
        juce::Colour(0xffff9f6e), juce::Colour(0xffb185ff), juce::Colour(0xffff6b6b),
        juce::Colour(0xff5da5ff), juce::Colour(0xffffc857)
    };
    constexpr auto paletteSize = (int) (sizeof(palette) / sizeof(palette[0]));

    auto colourForFolder = [&](int folderIndex)
    {
        auto id = timelineModel.getTrackId(folderIndex);
        auto index = (int) ((juce::uint32) id.hashCode() % (juce::uint32) paletteSize);
        return palette[index];
    };

    if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::folder)
        return colourForFolder(trackIndex);

    auto parent = timelineModel.getTrackParent(trackIndex);
    if (parent >= 0)
        return colourForFolder(parent);

    return juce::Colour();
}

void MainComponent::showMoveToFolderPicker(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()))
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "None (top-level)", true, timelineModel.getTrackParent(trackIndex) < 0);

    juce::Array<int> folderTrackIndices;
    int nextItemId = 2;
    for (int otherIndex = 0; otherIndex < timelineModel.getTrackCount(); ++otherIndex)
    {
        if (otherIndex == trackIndex || timelineModel.getTrackKind(otherIndex) != cs::TrackKind::folder)
            continue;

        auto folderName = timelineModel.getTrackName(otherIndex);
        if (folderName.isEmpty())
            folderName = "Track " + juce::String(otherIndex + 1);

        menu.addItem(nextItemId, folderName, true, timelineModel.getTrackParent(trackIndex) == otherIndex);
        folderTrackIndices.add(otherIndex);
        ++nextItemId;
    }

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, trackIndex, folderTrackIndices](int result)
    {
        if (result <= 0)
            return;

        auto newParent = result == 1 ? -1 : folderTrackIndices[result - 2];

        if (! timelineModel.setTrackParent(trackIndex, newParent))
        {
            transportBar.setStatusText("Couldn't move that track there - it would create a folder loop.");
            return;
        }

        // Assigning into a folder also physically groups it there - directly under the folder
        // and any of its existing members - so the track list and the routing always agree.
        // Detaching back to top-level (newParent < 0) leaves its position alone.
        if (newParent >= 0)
            performTrackMove(trackIndex, newParent + timelineModel.getFolderBlockLength(newParent));

        syncTrackViews();
        saveSessionToDisk();
    });
}

void MainComponent::showAutomationTargetPicker(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()))
        return;

    struct MenuAction
    {
        cs::AutomationTarget target;
    };

    auto actions = std::make_shared<std::vector<MenuAction>>();
    juce::PopupMenu menu;
    int nextItemId = 1;

    for (int otherIndex = 0; otherIndex < timelineModel.getTrackCount(); ++otherIndex)
    {
        if (otherIndex == trackIndex)
            continue;

        if (timelineModel.getTrackKind(otherIndex) == cs::TrackKind::automation)
            continue;

        auto trackName = timelineModel.getTrackName(otherIndex);
        if (trackName.isEmpty())
            trackName = "Track " + juce::String(otherIndex + 1);

        juce::PopupMenu trackMenu;

        {
            cs::AutomationTarget target;
            target.kind = cs::AutomationTargetKind::trackVolume;
            target.targetTrackIndex = otherIndex;
            target.displayName = trackName + " \xe2\x86\x92 Volume";
            trackMenu.addItem(nextItemId, "Volume");
            actions->push_back({ target });
            ++nextItemId;
        }

        {
            cs::AutomationTarget target;
            target.kind = cs::AutomationTargetKind::trackPan;
            target.targetTrackIndex = otherIndex;
            target.displayName = trackName + " \xe2\x86\x92 Pan";
            trackMenu.addItem(nextItemId, "Pan");
            actions->push_back({ target });
            ++nextItemId;
        }

        auto pluginNames = engine.getTrackPluginNames(otherIndex);
        for (int slotIndex = 0; slotIndex < pluginNames.size(); ++slotIndex)
        {
            auto paramCount = engine.getTrackPluginParameterCount(otherIndex, slotIndex);
            if (paramCount <= 0)
                continue;

            juce::PopupMenu pluginMenu;
            for (int paramIndex = 0; paramIndex < paramCount; ++paramIndex)
            {
                auto paramName = engine.getTrackPluginParameterName(otherIndex, slotIndex, paramIndex);
                if (paramName.isEmpty())
                    paramName = "Param " + juce::String(paramIndex + 1);

                cs::AutomationTarget target;
                target.kind = cs::AutomationTargetKind::pluginParameter;
                target.targetTrackIndex = otherIndex;
                target.pluginSlotIndex = slotIndex;
                target.pluginParameterIndex = paramIndex;
                target.parameterId = juce::String(paramIndex);
                target.displayName = trackName + " \xe2\x86\x92 " + pluginNames[slotIndex] + " \xe2\x86\x92 " + paramName;

                pluginMenu.addItem(nextItemId, paramName);
                actions->push_back({ target });
                ++nextItemId;
            }

            trackMenu.addSubMenu(pluginNames[slotIndex], pluginMenu);
        }

        menu.addSubMenu(trackName, trackMenu);
    }

    if (nextItemId == 1)
    {
        menu.addItem(-1, "No other tracks available", false);
    }

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, trackIndex, actions](int result)
    {
        if (result <= 0 || (size_t) result > actions->size())
            return;

        const auto& action = (*actions)[(size_t) result - 1];
        timelineModel.setAutomationTarget(trackIndex, action.target);
        pushAutomationDataToEngine(trackIndex);
        trackerPanel.setAutomationTargetLabel(trackIndex, action.target.displayName);
        saveSessionToDisk();
    });
}

void MainComponent::refreshTrackInputSources()
{
    auto inputSources = engine.getInputSources();
    juce::Array<cs::HardwareInputSource> hardwareInputs;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    auto selectedInputDeviceName = selectedStudioInputDevice.trim();
    if (selectedInputDeviceName.isEmpty())
        selectedInputDeviceName = setup.inputDeviceName.trim();

    for (const auto& source : inputSources)
    {
        cs::HardwareInputSource hardwareInput;
        hardwareInput.channelIndex = source.channelIndex;
        hardwareInput.id = source.id;
        hardwareInput.name = source.name;

        if (selectedInputDeviceName.isNotEmpty())
        {
            auto suffix = source.name.fromLastOccurrenceOf(" / ", false, false).trim();
            if (suffix.isEmpty() || suffix == source.name)
                suffix = "Input channel " + juce::String(source.channelIndex + 1);

            hardwareInput.name = selectedInputDeviceName + " / " + suffix;
        }

        hardwareInputs.add(std::move(hardwareInput));
    }

    studioIOModel.setHardwareInputs(hardwareInputs);
    juce::Array<juce::String> trackerInputNames;
    for (const auto& name : studioIOModel.getNames())
        trackerInputNames.add(name);
    trackerPanel.setInputSources(trackerInputNames);
    settingsPanel.setStudioInputRows(studioIOModel.getNames(),
                                     studioIOModel.getHardwareNames(),
                                     studioIOModel.getAvailability());
}

void MainComponent::refreshAudioDeviceSettingsView()
{
    juce::OwnedArray<juce::AudioIODeviceType> deviceTypes;
    deviceManager.createAudioDeviceTypes(deviceTypes);

    juce::StringArray audioSystems;
    juce::StringArray inputDevices;
    juce::StringArray outputDevices;
    auto selectedSystem = deviceManager.getCurrentAudioDeviceType();
    selectedStudioAudioSystem = selectedSystem;

    for (auto* type : deviceTypes)
    {
        if (type == nullptr)
            continue;

        auto typeName = type->getTypeName();
        audioSystems.addIfNotAlreadyThere(typeName);

        if (typeName == selectedSystem)
        {
            type->scanForDevices();
            inputDevices = type->getDeviceNames(true);
            outputDevices = type->getDeviceNames(false);
        }
    }

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    if (selectedStudioInputDevice.isEmpty())
        selectedStudioInputDevice = setup.inputDeviceName;
    if (selectedStudioOutputDevice.isEmpty())
        selectedStudioOutputDevice = setup.outputDeviceName;

    settingsPanel.setAudioDeviceLists(audioSystems,
                                      inputDevices,
                                      outputDevices,
                                      selectedSystem,
                                      selectedStudioInputDevice.isNotEmpty() ? selectedStudioInputDevice : setup.inputDeviceName,
                                      selectedStudioOutputDevice.isNotEmpty() ? selectedStudioOutputDevice : setup.outputDeviceName);

    juce::String diagnostics;
    auto canOpenDriverControlPanel = false;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        auto activeInputs = device->getActiveInputChannels();
        auto activeOutputs = device->getActiveOutputChannels();
        auto inputNames = device->getInputChannelNames();

        diagnostics << "Driver: " << device->getTypeName() << " / " << device->getName() << "\n";
        diagnostics << "Rate: " << juce::String(device->getCurrentSampleRate(), 0)
                    << " Hz   Buffer: " << device->getCurrentBufferSizeSamples() << " samples\n";
        diagnostics << "Inputs: " << activeInputs.countNumberOfSetBits()
                    << " active   Outputs: " << activeOutputs.countNumberOfSetBits() << " active";

        if (! inputNames.isEmpty())
        {
            diagnostics << "\nInput names: ";
            for (int index = 0; index < inputNames.size(); ++index)
            {
                if (index > 0)
                    diagnostics << ", ";
                diagnostics << inputNames[index];
            }
        }

        canOpenDriverControlPanel = device->hasControlPanel();
    }
    else
    {
        diagnostics = "No active audio device. Choose an audio system and device.";
    }

    settingsPanel.setAudioDiagnostics(diagnostics, canOpenDriverControlPanel);
}

void MainComponent::setAudioSystem(const juce::String& audioSystem)
{
    if (audioSystem.isEmpty() || audioSystem == deviceManager.getCurrentAudioDeviceType())
        return;

    selectedStudioAudioSystem = audioSystem;
    selectedStudioInputDevice.clear();
    selectedStudioOutputDevice.clear();
    deviceManager.setCurrentAudioDeviceType(audioSystem, true);
    refreshAudioDeviceSettingsView();
    refreshTrackInputSources();
    saveAppSettings();
    transportBar.setStatusText("Audio system: " + audioSystem);
}

void MainComponent::setAudioInputDevice(const juce::String& inputDeviceName)
{
    if (inputDeviceName.isEmpty())
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (setup.inputDeviceName == inputDeviceName)
    {
        selectedStudioInputDevice = inputDeviceName;
        return;
    }

    selectedStudioInputDevice = inputDeviceName;
    setup.inputDeviceName = inputDeviceName;
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    for (int channel = 0; channel < 32; ++channel)
        setup.inputChannels.setBit(channel);

    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        transportBar.setStatusText(error);
        refreshAudioDeviceSettingsView();
        return;
    }

    refreshAudioDeviceSettingsView();
    refreshTrackInputSources();
    syncTrackViews();
    saveAppSettings();
    transportBar.setStatusText("Input device: " + inputDeviceName);
}

void MainComponent::setAudioOutputDevice(const juce::String& outputDeviceName)
{
    if (outputDeviceName.isEmpty())
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (setup.outputDeviceName == outputDeviceName)
    {
        selectedStudioOutputDevice = outputDeviceName;
        return;
    }

    selectedStudioOutputDevice = outputDeviceName;
    setup.outputDeviceName = outputDeviceName;
    setup.useDefaultOutputChannels = true;

    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        transportBar.setStatusText(error);
        refreshAudioDeviceSettingsView();
        return;
    }

    refreshAudioDeviceSettingsView();
    refreshTrackInputSources();
    saveAppSettings();
    transportBar.setStatusText("Output device: " + outputDeviceName);
}

void MainComponent::addTrack()
{
    juce::ValueTree undoSnapshot("UndoSnapshot");
    undoSnapshot.addChild(timelineModel.createState(), -1, nullptr);
    undoSnapshot.addChild(engine.createSessionState(), -1, nullptr);

    auto trackIndex = engine.addTrack();
    if (trackIndex < 0)
        return;

    pushTimelineUndoState(undoSnapshot);

    if ((int) armedTracks.size() <= trackIndex)
        armedTracks.resize((size_t) trackIndex + 1, false);
    if ((int) monitoredTracks.size() <= trackIndex)
        monitoredTracks.resize((size_t) trackIndex + 1, false);

    syncTrackViews();

    auto visibleCount = arrangeView.getVisibleTrackCount();
    if (visibleCount == 0)
        visibleCount = juce::jmin(8, engine.getTrackCount());
    else
        visibleCount = juce::jmin(visibleCount, engine.getTrackCount());

    arrangeView.setVisibleTrackCount(visibleCount);

    auto bankOffset = (trackIndex / mixerPanel.getVisibleChannelCount()) * mixerPanel.getVisibleChannelCount();
    mixerPanel.setBankOffset(bankOffset);
    midiSurface.setBankOffset(bankOffset);

    if (juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
        trackerPanel.setSelectedTrack(trackIndex);
        arrangeView.setSelectedTrack(trackIndex);
    }

    auto visibleStart = mixerPanel.getBankOffset();
    auto bankVisibleCount = mixerPanel.getVisibleChannelCount();

    for (int slot = 0; slot < bankVisibleCount; ++slot)
    {
        auto visibleTrackIndex = visibleStart + slot;
        if (! juce::isPositiveAndBelow(visibleTrackIndex, engine.getTrackCount()))
            continue;

        auto name = engine.getTrackName(visibleTrackIndex);
        auto gain = engine.getTrackGain(visibleTrackIndex);
        auto pan = engine.getTrackPan(visibleTrackIndex);
        auto muted = engine.isTrackMuted(visibleTrackIndex);
        auto soloed = engine.isTrackSoloed(visibleTrackIndex);
        auto pluginName = engine.getTrackPluginName(visibleTrackIndex);
        auto pluginBypassed = engine.isTrackPluginBypassed(visibleTrackIndex);

        mixerPanel.setChannelName(visibleTrackIndex, name);
        mixerPanel.setChannelInsertName(visibleTrackIndex, pluginName.isNotEmpty() ? ("FX: " + pluginName) : "FX: none");
        mixerPanel.setChannelInsertBypassed(visibleTrackIndex, pluginBypassed);
        mixerPanel.setChannelGain(visibleTrackIndex, gain);
        mixerPanel.setChannelPan(visibleTrackIndex, pan);
        mixerPanel.setChannelMuted(visibleTrackIndex, muted);
        mixerPanel.setChannelSoloed(visibleTrackIndex, soloed);

        midiSurface.setChannelName(visibleTrackIndex, name);
        midiSurface.setChannelGain(visibleTrackIndex, gain);
        midiSurface.setChannelPan(visibleTrackIndex, pan);
        midiSurface.setChannelMuted(visibleTrackIndex, muted);
        midiSurface.setChannelSoloed(visibleTrackIndex, soloed);
    }

    auto masterGain = engine.getMasterGain();
    mixerPanel.setMasterGain(masterGain);
    midiSurface.setMasterFaderValue(masterGain);
    midiSurface.refreshVisibleWindow();
    refreshInsertRack();
    saveSessionToDisk();
}

void MainComponent::removeTrack(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto trackName = engine.getTrackName(trackIndex);
    if (trackName.trim().isEmpty())
        trackName = "Track " + juce::String(trackIndex + 1);

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Remove Track")
        .withMessage("Remove track \"" + trackName + "\"? This can be undone with Ctrl+Z.")
        .withButton("Remove")
        .withButton("Cancel");

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showAsync(options,
                                 [safeThis, trackIndex](int result)
                                 {
                                     if (safeThis == nullptr || result != 1)
                                         return;

                                     safeThis->performTrackRemoval(trackIndex);
                                 });
}

void MainComponent::performTrackRemoval(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    juce::ValueTree undoSnapshot("UndoSnapshot");
    undoSnapshot.addChild(timelineModel.createState(), -1, nullptr);
    undoSnapshot.addChild(engine.createSessionState(), -1, nullptr);

    if (! engine.removeTrack(trackIndex))
        return;

    pushTimelineUndoState(undoSnapshot);

    timelineModel.removeTrack(trackIndex);
    trackerPanel.refreshTimelineView();

    if (juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
        armedTracks.erase(armedTracks.begin() + trackIndex);
    if (juce::isPositiveAndBelow(trackIndex, (int) monitoredTracks.size()))
        monitoredTracks.erase(monitoredTracks.begin() + trackIndex);

    syncTrackViews();

    auto trackCount = engine.getTrackCount();
    auto visibleCount = trackCount == 0 ? 0 : juce::jmin(juce::jmax(1, arrangeView.getVisibleTrackCount()), trackCount);
    arrangeView.setVisibleTrackCount(visibleCount);

    auto maxBankOffset = juce::jmax(0, trackCount - mixerPanel.getVisibleChannelCount());
    mixerPanel.setBankOffset(juce::jlimit(0, maxBankOffset, mixerPanel.getBankOffset()));
    midiSurface.setBankOffset(mixerPanel.getBankOffset());

    if (trackCount == 0)
    {
        pluginRackBar.setContextMaster();
        mixerPanel.setSelectedChannel(-1);
        trackerPanel.setSelectedTrack(-1);
    }
    else
    {
        auto selectedTrack = juce::jlimit(0, trackCount - 1, trackIndex);
        pluginRackBar.setContextTrack(selectedTrack, engine.getTrackName(selectedTrack));
        mixerPanel.setSelectedChannel(selectedTrack);
        trackerPanel.setSelectedTrack(selectedTrack);
        arrangeView.setSelectedTrack(selectedTrack);
    }

    midiSurface.refreshVisibleWindow();
    refreshInsertRack();
    saveSessionToDisk();
}

void MainComponent::refreshInsertRack()
{
    if (pluginRackBar.isTrackContext())
    {
        auto trackIndex = pluginRackBar.getTrackIndex();
        pluginRackBar.setPluginName(engine.getTrackPluginName(trackIndex));
        pluginRackBar.setBypassed(engine.isTrackPluginBypassed(trackIndex));
    }
    else
    {
        pluginRackBar.setPluginName(engine.getMasterPluginName());
        pluginRackBar.setBypassed(engine.isMasterPluginBypassed());
    }

    graphPanel.setVstEnabled(engine.isGraphVstEnabled());
    graphPanel.setVstMix(engine.getGraphVstMix());
    if (engine.hasGraphVstPlugin())
        graphPanel.setAssignedVstPlugin(engine.getGraphVstPluginName(), engine.getGraphVstPluginFile().getFullPathName());

    refreshFxStackWindow();
    refreshPluginsPanel();
}
