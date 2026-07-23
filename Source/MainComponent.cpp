#include "MainComponent.h"
#include "Branding.h"
#include "Patch/PatchModel.h"
#include "Tutorial/TutorialScriptCompiler.h"
#include <thread>

namespace
{
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
        if (onClose)
            onClose();
    }

private:
    std::function<void()> onClose;
};

class TransportButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        juce::ignoreUnused(backgroundColour);

        auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
        auto isToggle = button.getToggleState();
        auto isRecord = button.getButtonText() == "record";
        auto isPrimaryActive = isToggle && (button.getButtonText() == "play" || isRecord);
        auto accent = isRecord ? juce::Colour(0xffff4f5f) : juce::Colour(0xff59dfff);
        auto fill = juce::Colour(0xff17222c);

        if (isToggle)
            fill = accent.withAlpha(isPrimaryActive ? 0.48f : 0.25f).overlaidWith(juce::Colour(0xff13202b));
        else if (isButtonDown)
            fill = accent.withAlpha(0.20f).overlaidWith(fill);
        else if (isMouseOverButton)
            fill = accent.withAlpha(0.12f).overlaidWith(fill);

        g.setColour(accent.withAlpha(isToggle ? (isPrimaryActive ? 0.62f : 0.35f)
                                               : isMouseOverButton ? 0.35f : 0.14f));
        g.fillRoundedRectangle(bounds.expanded(isPrimaryActive ? 4.0f : 2.0f), 13.0f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 11.0f);

        g.setColour(accent.withAlpha(isToggle ? 1.0f : 0.62f));
        g.drawRoundedRectangle(bounds, 11.0f, isToggle ? (isPrimaryActive ? 2.8f : 2.0f) : 1.3f);

        auto ring = bounds.reduced(7.0f, 5.0f);
        if (ring.getWidth() > 18.0f && ring.getHeight() > 18.0f)
        {
            auto diameter = juce::jmin(ring.getWidth(), ring.getHeight());
            auto circle = juce::Rectangle<float>(diameter, diameter).withCentre(ring.getCentre());
            g.setColour(accent.withAlpha(isToggle ? 0.96f : 0.36f));
            g.drawEllipse(circle, isPrimaryActive ? 3.0f : 2.0f);
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool isMouseOverButton,
                        bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(8.0f, 7.0f);
        auto text = button.getButtonText();

        g.setColour(button.getToggleState() ? juce::Colours::white
                                            : (isButtonDown ? juce::Colour(0xffeaf6ff)
                                                            : isMouseOverButton ? juce::Colour(0xffdcecff)
                                                                                : juce::Colour(0xffb8c4d5)));
        if (button.getToggleState() && button.getButtonText() == "play")
            g.setColour(juce::Colour(0xffffffff));
        if (button.getButtonText() == "record")
            g.setColour(juce::Colour(0xffff4f5f));

        drawTransportIcon(g, bounds, text);
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool isMouseOverButton,
                          bool isButtonDown) override
    {
        drawButtonBackground(g,
                             button,
                             button.findColour(juce::TextButton::buttonColourId),
                             isMouseOverButton,
                             isButtonDown);

        auto bounds = button.getLocalBounds().toFloat().reduced(8.0f, 7.0f);
        g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xffb8c4d5));
        if (button.getToggleState())
            g.setColour(juce::Colour(0xff5ce8ff));

        drawTransportIcon(g, bounds, button.getButtonText());
    }

private:
    static void drawTransportIcon(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& iconName)
    {
        auto centre = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

        if (iconName == "play")
        {
            juce::Path path;
            path.addTriangle(centre.x - size * 0.22f, centre.y - size * 0.32f,
                             centre.x - size * 0.22f, centre.y + size * 0.32f,
                             centre.x + size * 0.32f, centre.y);
            g.fillPath(path);
            return;
        }

        if (iconName == "pause")
        {
            auto barWidth = size * 0.17f;
            auto gap = size * 0.10f;
            auto height = size * 0.62f;
            g.fillRoundedRectangle(centre.x - gap - barWidth, centre.y - height * 0.5f, barWidth, height, 1.5f);
            g.fillRoundedRectangle(centre.x + gap, centre.y - height * 0.5f, barWidth, height, 1.5f);
            return;
        }

        if (iconName == "stop")
        {
            auto square = juce::Rectangle<float>(size * 0.55f, size * 0.55f).withCentre(centre);
            g.fillRoundedRectangle(square, 2.0f);
            return;
        }

        if (iconName == "record")
        {
            g.setColour(juce::Colour(0xffff5f6d));
            g.fillEllipse(juce::Rectangle<float>(size * 0.58f, size * 0.58f).withCentre(centre));
            return;
        }

        if (iconName == "prev" || iconName == "next")
        {
            auto direction = iconName == "prev" ? 1.0f : -1.0f;
            auto barX = centre.x + direction * size * 0.33f;
            g.fillRoundedRectangle(barX - size * 0.04f, centre.y - size * 0.33f, size * 0.08f, size * 0.66f, 1.5f);

            for (auto offset : { -0.08f, 0.18f })
            {
                juce::Path path;
                path.addTriangle(centre.x - direction * size * (0.24f + offset), centre.y,
                                 centre.x + direction * size * (0.02f - offset), centre.y - size * 0.30f,
                                 centre.x + direction * size * (0.02f - offset), centre.y + size * 0.30f);
                g.fillPath(path);
            }
            return;
        }

        if (iconName == "loop")
        {
            auto arc = bounds.reduced(size * 0.12f);
            g.drawEllipse(arc, 2.0f);
            juce::Path arrow;
            arrow.addTriangle(arc.getRight() - size * 0.02f, arc.getCentreY() - size * 0.20f,
                              arc.getRight() + size * 0.16f, arc.getCentreY() - size * 0.06f,
                              arc.getRight() - size * 0.02f, arc.getCentreY() + size * 0.08f);
            g.fillPath(arrow);
            return;
        }

        if (iconName == "click")
        {
            g.fillEllipse(juce::Rectangle<float>(size * 0.18f, size * 0.18f).withCentre({ centre.x, centre.y - size * 0.18f }));
            g.fillEllipse(juce::Rectangle<float>(size * 0.18f, size * 0.18f).withCentre({ centre.x - size * 0.18f, centre.y + size * 0.14f }));
            g.fillEllipse(juce::Rectangle<float>(size * 0.18f, size * 0.18f).withCentre({ centre.x + size * 0.18f, centre.y + size * 0.14f }));
            return;
        }

        g.setFont(juce::Font(13.0f).boldened());
        g.drawText(iconName, bounds.toNearestInt(), juce::Justification::centred, true);
    }
};

TransportButtonLookAndFeel& getTransportButtonLookAndFeel()
{
    static TransportButtonLookAndFeel lookAndFeel;
    return lookAndFeel;
}

juce::String makeInitials(const juce::String& displayName, const juce::String& email)
{
    auto source = displayName.isNotEmpty() ? displayName : email;
    source = source.trim();

    if (source.isEmpty())
        return "C";

    juce::StringArray words;
    words.addTokens(source, " ._-@", "");
    words.removeEmptyStrings();

    juce::String initials;
    for (int index = 0; index < words.size() && initials.length() < 2; ++index)
        initials << words[index].substring(0, 1).toUpperCase();

    if (initials.isEmpty())
        initials = source.substring(0, 1).toUpperCase();

    return initials;
}

juce::String profileDetailText(const DesktopAuthSession::SessionData& session)
{
    auto tierId = branding::getBestPatreonTierId(session.user.entitlements);
    auto tierName = branding::getPatreonTierDisplayName(tierId);

    if (tierName.isNotEmpty())
        return session.user.email + " - " + tierName;

    return session.user.email;
}

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
    }

    return "Tracker";
}

constexpr int workspaceModeCount = 11;

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

MainComponent::TransportBar::TransportBar()
{
    setName("Transport");
    logoImage = branding::createCreationStationLogoImage(72);

    titleLabel.setText("Creation Station", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    midiStatusLabel.setText("MIDI: waiting for controller", juce::dontSendNotification);
    midiStatusLabel.setJustificationType(juce::Justification::centredLeft);
    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(midiStatusLabel);

    projectButton.setButtonText("Project: No project open");
    addAndMakeVisible(projectButton);

    playButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    pauseButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    stopButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    recordButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    loopButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    clickButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    rewindButton.setLookAndFeel(&getTransportButtonLookAndFeel());
    fastForwardButton.setLookAndFeel(&getTransportButtonLookAndFeel());

    playButton.setButtonText("play");
    pauseButton.setButtonText("pause");
    stopButton.setButtonText("stop");
    recordButton.setButtonText("record");
    loopButton.setButtonText("loop");
    clickButton.setButtonText("click");
    rewindButton.setButtonText("prev");
    fastForwardButton.setButtonText("next");

    playButton.setTooltip("Play");
    pauseButton.setTooltip("Pause");
    stopButton.setTooltip("Stop");
    recordButton.setTooltip("Record");
    loopButton.setTooltip("Loop");
    clickButton.setTooltip("Metronome click");
    rewindButton.setTooltip("Rewind");
    fastForwardButton.setTooltip("Fast forward");

    playButton.setMouseClickGrabsKeyboardFocus(false);
    pauseButton.setMouseClickGrabsKeyboardFocus(false);
    stopButton.setMouseClickGrabsKeyboardFocus(false);
    recordButton.setMouseClickGrabsKeyboardFocus(false);
    loopButton.setMouseClickGrabsKeyboardFocus(false);
    clickButton.setMouseClickGrabsKeyboardFocus(false);
    rewindButton.setMouseClickGrabsKeyboardFocus(false);
    fastForwardButton.setMouseClickGrabsKeyboardFocus(false);

    audioButton.onClick = [this]
    {
        if (onAudioRequested)
            onAudioRequested();
    };
    addAndMakeVisible(audioButton);

    tourButton.onClick = [this]
    {
        if (onTourRequested)
            onTourRequested();
    };
    addAndMakeVisible(tourButton);

    statusLabel.setText("Ready for audio work.", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff99a6b8));
    addAndMakeVisible(statusLabel);

    playButton.onClick = [this]
    {
        statusLabel.setText("Transport: play", juce::dontSendNotification);
        if (onPlay)
            onPlay();
    };
    pauseButton.onClick = [this]
    {
        statusLabel.setText("Transport: pause", juce::dontSendNotification);
        if (onPause)
            onPause();
    };
    stopButton.onClick = [this]
    {
        statusLabel.setText("Transport: stop", juce::dontSendNotification);
        if (onStop)
            onStop();
    };
    recordButton.onClick = [this]
    {
        statusLabel.setText("Transport: record armed", juce::dontSendNotification);
        if (onRecord)
            onRecord();
    };
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this]
    {
        statusLabel.setText(loopButton.getToggleState() ? "Transport: loop on" : "Transport: loop off",
                            juce::dontSendNotification);
        if (onLoopChanged)
            onLoopChanged(loopButton.getToggleState());
    };
    clickButton.setClickingTogglesState(true);
    clickButton.onClick = [this]
    {
        statusLabel.setText(clickButton.getToggleState() ? "Transport: click on" : "Transport: click off",
                            juce::dontSendNotification);
        if (onClickChanged)
            onClickChanged(clickButton.getToggleState());
    };
    rewindButton.onClick = [this]
    {
        statusLabel.setText("Transport: rewind", juce::dontSendNotification);
        if (onRewind)
            onRewind();
    };
    fastForwardButton.onClick = [this]
    {
        statusLabel.setText("Transport: fast forward", juce::dontSendNotification);
        if (onFastForward)
            onFastForward();
    };

    addAndMakeVisible(playButton);
    addAndMakeVisible(pauseButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(recordButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(clickButton);
    addAndMakeVisible(rewindButton);
    addAndMakeVisible(fastForwardButton);

    signInButton.onClick = [this]
    {
        if (onSignInRequested)
            onSignInRequested();
    };
    addAndMakeVisible(signInButton);

    projectButton.onClick = [this]
    {
        if (onProjectMenuRequested)
            onProjectMenuRequested();
    };
    addAndMakeVisible(projectButton);

    profileNameLabel.setJustificationType(juce::Justification::centredLeft);
    profileNameLabel.setFont(juce::Font(17.0f).boldened());
    profileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(profileNameLabel);

    profileDetailLabel.setJustificationType(juce::Justification::centredLeft);
    profileDetailLabel.setFont(juce::Font(13.0f));
    profileDetailLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(profileDetailLabel);

    clearProfile();
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

void MainComponent::TransportBar::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::TransportBar::setMidiStatusText(const juce::String& text)
{
    midiStatusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::TransportBar::setPlaybackVisualState(bool playing, bool recording)
{
    playButton.setToggleState(playing && ! recording, juce::dontSendNotification);
    pauseButton.setToggleState(false, juce::dontSendNotification);
    stopButton.setToggleState(! playing && ! recording, juce::dontSendNotification);
    recordButton.setToggleState(recording, juce::dontSendNotification);
    repaint();
}

void MainComponent::TransportBar::setProjectLabel(const juce::String& label)
{
    projectText = label;
    projectButton.setButtonText(projectText);
    repaint();
}

juce::Rectangle<int> MainComponent::TransportBar::getProjectButtonScreenBounds() const
{
    return localAreaToGlobal(projectButton.getBounds());
}

void MainComponent::TransportBar::setProfile(const DesktopAuthSession::SessionData& session)
{
    profileVisible = true;
    profileNameLabel.setVisible(true);
    profileDetailLabel.setVisible(true);
    signInButton.setVisible(false);

    profileNameLabel.setText(session.user.displayName.isNotEmpty() ? session.user.displayName : session.user.email,
                             juce::dontSendNotification);
    profileDetailLabel.setText(profileDetailText(session), juce::dontSendNotification);
    profileInitials = makeInitials(session.user.displayName, session.user.email);

    auto tierId = branding::getBestPatreonTierId(session.user.entitlements);
    profileTierName = branding::getPatreonTierDisplayName(tierId);
    profileBadgeImage = branding::createPatreonBadgeImage(tierId, 36);

    repaint();
}

void MainComponent::TransportBar::clearProfile()
{
    profileVisible = false;
    profileNameLabel.setText({}, juce::dontSendNotification);
    profileDetailLabel.setText({}, juce::dontSendNotification);
    profileBadgeImage = {};
    profileInitials.clear();
    profileTierName.clear();
    profileNameLabel.setVisible(false);
    profileDetailLabel.setVisible(false);
    signInButton.setButtonText("Sign In");
    signInButton.setVisible(true);
    repaint();
}

void MainComponent::TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    if (logoImage.isValid())
        g.drawImageWithin(logoImage, 14, 9, 44, 44, juce::RectanglePlacement::centred, false);
    g.setColour(juce::Colour(0xff242a36));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 1.0f,
               1.0f);

    if (! transportControlBounds.isEmpty())
    {
        auto panel = transportControlBounds.toFloat().expanded(12.0f, 9.0f);
        juce::ColourGradient glow(juce::Colour(0x4426d9ff), panel.getCentreX(), panel.getY(),
                                  juce::Colour(0x00101820), panel.getCentreX(), panel.getBottom(), false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(panel.expanded(4.0f), 18.0f);

        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(panel, 16.0f);
        g.setColour(juce::Colour(0xff36506b));
        g.drawRoundedRectangle(panel, 16.0f, 1.4f);
    }

    if (profileVisible)
    {
        auto chip = profileChipBounds.toFloat();
        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(chip.toFloat(), 16.0f);
        g.setColour(juce::Colour(0xff2c394c));
        g.drawRoundedRectangle(chip.toFloat(), 16.0f, 1.0f);

        auto avatar = chip.removeFromLeft(44).reduced(4);
        g.setColour(juce::Colour(0xff223041));
        g.fillEllipse(avatar.toFloat());
        g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(15.0f).boldened());
        g.drawText(profileInitials, avatar, juce::Justification::centred, false);

        auto badgeArea = chip.removeFromRight(38).withSizeKeepingCentre(28, 28);
        if (profileBadgeImage.isValid())
            g.drawImageWithin(profileBadgeImage, badgeArea.getX(), badgeArea.getY(), badgeArea.getWidth(), badgeArea.getHeight(),
                              juce::RectanglePlacement::centred, false);
        else
        {
            g.setColour(juce::Colour(0xfff2cc60));
            g.setFont(juce::Font(12.0f).boldened());
            g.drawText("*", badgeArea, juce::Justification::centred, false);
        }
    }
}

void MainComponent::TransportBar::mouseDown(const juce::MouseEvent& event)
{
    if (profileVisible && profileChipBounds.contains(event.getPosition()))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "View profile page");
        menu.addSeparator();
        menu.addItem(2, "Log out");

        auto chipTopLeft = localPointToGlobal(profileChipBounds.getTopLeft());
        auto chipOnScreen = juce::Rectangle<int>(chipTopLeft.x,
                                                 chipTopLeft.y,
                                                 profileChipBounds.getWidth(),
                                                 profileChipBounds.getHeight());

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(chipOnScreen),
                           [this](int result)
                           {
                               if (result == 1)
                               {
                                   if (onOpenProfilePageRequested)
                                       onOpenProfilePageRequested();
                               }
                               else if (result == 2)
                               {
                                   if (onLogoutRequested)
                                       onLogoutRequested();
                               }
                           });
    }
}

void MainComponent::TransportBar::resized()
{
    auto area = getLocalBounds().reduced(18, 10);
    area.removeFromLeft(56);
    auto profileArea = area.removeFromRight(268);

    auto topRow = area.removeFromTop(30);
    auto bottomRow = area;

    titleLabel.setBounds(topRow.removeFromLeft(290));
    midiStatusLabel.setBounds(topRow.removeFromLeft(230));

    auto transportRow = bottomRow.removeFromLeft(662);
    projectButton.setBounds(bottomRow.removeFromLeft(260));
    audioButton.setBounds(bottomRow.removeFromLeft(82));
    tourButton.setBounds(bottomRow.removeFromLeft(78));

    auto placeTransportButton = [&transportRow](juce::Button& button, int width)
    {
        button.setBounds(transportRow.removeFromLeft(width));
        transportRow.removeFromLeft(7);
    };

    placeTransportButton(rewindButton, 62);
    placeTransportButton(fastForwardButton, 62);
    placeTransportButton(stopButton, 66);
    placeTransportButton(pauseButton, 66);
    placeTransportButton(playButton, 82);
    placeTransportButton(loopButton, 64);
    placeTransportButton(clickButton, 64);
    placeTransportButton(recordButton, 82);
    statusLabel.setBounds(bottomRow.removeFromRight(220));

    auto combineBounds = [](juce::Rectangle<int> left, juce::Rectangle<int> right)
    {
        auto minX = juce::jmin(left.getX(), right.getX());
        auto minY = juce::jmin(left.getY(), right.getY());
        auto maxX = juce::jmax(left.getRight(), right.getRight());
        auto maxY = juce::jmax(left.getBottom(), right.getBottom());
        return juce::Rectangle<int>(minX, minY, maxX - minX, maxY - minY);
    };

    transportControlBounds = combineBounds(rewindButton.getBounds(), stopButton.getBounds());
    transportControlBounds = combineBounds(transportControlBounds, fastForwardButton.getBounds());
    transportControlBounds = combineBounds(transportControlBounds, pauseButton.getBounds());
    transportControlBounds = combineBounds(transportControlBounds, playButton.getBounds());
    transportControlBounds = combineBounds(transportControlBounds, loopButton.getBounds());
    transportControlBounds = combineBounds(transportControlBounds, clickButton.getBounds());
    transportControlBounds = combineBounds(transportControlBounds, recordButton.getBounds());

    auto profileContent = profileArea.reduced(12, 6);
    signInButton.setBounds(profileContent);

    auto profileTextArea = profileContent.withTrimmedLeft(48).withTrimmedRight(42);
    profileNameLabel.setBounds(profileTextArea.removeFromTop(24));
    profileDetailLabel.setBounds(profileTextArea.removeFromTop(18));
    profileChipBounds = profileArea;
    signInButton.setVisible(! profileVisible);
    projectButton.setVisible(true);
    profileNameLabel.setVisible(profileVisible);
    profileDetailLabel.setVisible(profileVisible);
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
    addAndMakeVisible(pluginNameLabel);

    catalogLabel.setText("No VST folders configured.", juce::dontSendNotification);
    catalogLabel.setJustificationType(juce::Justification::centredLeft);
    catalogLabel.setColour(juce::Label::textColourId, juce::Colour(0xff71839b));
    addAndMakeVisible(catalogLabel);

    bypassButton.onClick = [this]
    {
        if (onBypassChanged)
            onBypassChanged(bypassButton.getToggleState());
    };
    addAndMakeVisible(bypassButton);

    pathsButton.onClick = [this]
    {
        if (onManagePluginPaths)
            onManagePluginPaths();
    };
    addAndMakeVisible(pathsButton);

    openEditorButton.onClick = [this]
    {
        if (onOpenPluginEditor)
            onOpenPluginEditor();
    };
    addAndMakeVisible(openEditorButton);

    fxStackButton.onClick = [this]
    {
        if (onOpenFxStack)
            onOpenFxStack();
    };
    addAndMakeVisible(fxStackButton);

    loadButton.onClick = [this]
    {
        if (onLoadPlugin)
            onLoadPlugin();
    };
    addAndMakeVisible(loadButton);

    unloadButton.onClick = [this]
    {
        if (onUnloadPlugin)
            onUnloadPlugin();
    };
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
    auto centerArea = area.removeFromLeft(360);
    pluginNameLabel.setBounds(centerArea.removeFromTop(20));
    catalogLabel.setBounds(centerArea.removeFromTop(18));
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

    for (auto* button : { &removeButton, &upButton, &downButton, &bypassButton, &openButton })
        addAndMakeVisible(button);

    catalogLabel.setText("Available Plugins", juce::dontSendNotification);
    catalogLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb2cc));
    addAndMakeVisible(catalogLabel);

    searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colour(0xff6d7d91));
    searchBox.onTextChange = [this]
    {
        catalogModel.setFilterText(searchBox.getText());
        catalogList.updateContent();
    };
    addAndMakeVisible(searchBox);

    catalogModel.onRowDoubleClicked = [this](int row)
    {
        if (auto* entry = catalogModel.getEntry(row))
            if (onAddPlugin)
                onAddPlugin(*entry);
    };
    catalogList.setRowHeight(38);
    catalogList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d141d));
    addAndMakeVisible(catalogList);

    addButton.onClick = [this] { addSelectedCatalogEntry(); };
    insertButton.onClick = [this] { insertSelectedCatalogEntry(); };
    rescanButton.onClick = [this] { if (onRescanRequested) onRescanRequested(); };

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
    catalogModel.setEntries(entries);
    catalogList.updateContent();
}

void MainComponent::FxStackPanel::addSelectedCatalogEntry()
{
    if (auto* entry = catalogModel.getEntry(catalogList.getSelectedRow()))
        if (onAddPlugin)
            onAddPlugin(*entry);
}

void MainComponent::FxStackPanel::insertSelectedCatalogEntry()
{
    const auto slot = getSelectedSlot();
    if (auto* entry = catalogModel.getEntry(catalogList.getSelectedRow()))
        if (onInsertPlugin && slot >= 0)
            onInsertPlugin(slot, *entry);
}

void MainComponent::FxStackPanel::CatalogListBoxModel::setEntries(const juce::Array<VstPluginCatalog::Entry>& newEntries)
{
    allEntries = newEntries;
    applyFilter();
}

void MainComponent::FxStackPanel::CatalogListBoxModel::setFilterText(const juce::String& filterText)
{
    filter = filterText.trim().toLowerCase();
    applyFilter();
}

void MainComponent::FxStackPanel::CatalogListBoxModel::applyFilter()
{
    if (filter.isEmpty())
    {
        filteredEntries = allEntries;
        return;
    }

    filteredEntries.clearQuick();
    for (const auto& entry : allEntries)
        if (entry.name.toLowerCase().contains(filter))
            filteredEntries.add(entry);
}

const VstPluginCatalog::Entry* MainComponent::FxStackPanel::CatalogListBoxModel::getEntry(int row) const noexcept
{
    return juce::isPositiveAndBelow(row, filteredEntries.size()) ? &filteredEntries.getReference(row) : nullptr;
}

int MainComponent::FxStackPanel::CatalogListBoxModel::getNumRows()
{
    return filteredEntries.size();
}

void MainComponent::FxStackPanel::CatalogListBoxModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow(rowNumber, filteredEntries.size()))
        return;

    auto row = juce::Rectangle<int>(0, 0, width, height).reduced(5, 3);
    g.setColour(rowIsSelected ? juce::Colour(0xff1f5f86) : juce::Colour(0xff172332));
    g.fillRoundedRectangle(row.toFloat(), 6.0f);

    const auto& entry = filteredEntries.getReference(rowNumber);
    auto textArea = row.reduced(10, 2);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(entry.name, textArea.removeFromTop(height > 30 ? (height - 6) / 2 + 3 : height),
               juce::Justification::centredLeft, true);

    if (height > 30)
    {
        g.setColour(juce::Colour(0xff7f90a8));
        g.setFont(juce::Font(11.0f));
        g.drawText(entry.file.getFullPathName(), textArea, juce::Justification::centredLeft, true);
    }
}

void MainComponent::FxStackPanel::CatalogListBoxModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (onRowDoubleClicked)
        onRowDoubleClicked(row);
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
    catalogList.setBounds(rightColumn);
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
        transportBar.setProfile(session);
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
    transportBar.onProjectMenuRequested = [this]
    {
        showProjectMenu();
    };
    transportBar.onAudioRequested = [this]
    {
        showAudioSettings();
    };
    transportBar.onTourRequested = [this]
    {
        showTour();
    };
    if (authSession.hasValidSession())
    {
        authenticated = true;
        const auto& session = authSession.getSession();
        transportBar.setProfile(session);
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Restored your saved login.");
    }

    reportStartup("Loading storage and settings...", 0.40f);
    juce::String storageError;
    if (! projectManager.loadStorageConfiguration(storageError))
        ensureStorageRootConfigured();

    loadAppSettings();
    reportStartup("Applying audio device settings...", 0.50f);
    applySelectedAudioDeviceSettings();

    reportStartup("Scanning VST plugin folders...", 0.60f);
    rescanVstCatalog();

    reportStartup("Opening project workspace...", 0.72f);
    auto loadedAutoloadProject = false;
    if (projectManager.hasStorageRoot() && autoloadLastProject)
    {
        loadedAutoloadProject = projectManager.loadLastProject();
        if (! loadedAutoloadProject)
        {
            juce::String projectError;
            projectManager.createProject("Untitled Project", projectError);
        }
    }
    else if (projectManager.hasStorageRoot() && ! projectManager.hasProject())
    {
        juce::String projectError;
        projectManager.createProject("Untitled Project", projectError);
    }
    transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
    settingsPanel.setStoragePath(projectManager.hasStorageRoot() ? projectManager.getStorageRoot().getFullPathName() : "");
    if (projectManager.hasProject())
        settingsPanel.setProjectMetadata(projectManager.getCurrentProject());
    settingsPanel.setAutoloadEnabled(autoloadLastProject);
    settingsPanel.setAiProviderSettings(aiProviderSettings);
    aiPanel.setSelectedProvider(aiProviderSettings.providerName);
    aiPanel.setSelectedModel(aiProviderSettings.modelName);
    refreshAiModelCatalog();

    if (loadedAutoloadProject)
    {
        reportStartup("Restoring project tracks and clips...", 0.80f);
        loadSessionFromDisk();
    }

    reportStartup("Loading control-surface maps...", 0.86f);
    ControlSurfaceMappingStore controlSurfaceMappings;
    juce::String controlSurfaceError;
    if (! projectManager.loadControlSurfaceMappings(controlSurfaceMappings, controlSurfaceError)
        || controlSurfaceMappings.getProfiles().isEmpty())
    {
        controlSurfaceMappings = ControlSurfaceMappingStore::createDefaultLibrary();
        projectManager.saveControlSurfaceMappings(controlSurfaceMappings, controlSurfaceError);
    }
    auto* activePreset = controlSurfaceMappings.findProfileById(controlSurfaceMappings.getActivePresetId());
    transportBar.setMidiStatusText("Control preset: " + (activePreset != nullptr ? activePreset->displayName
                                                                                 : controlSurfaceMappings.getActivePresetId()));

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
        if (engine.isRecording())
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
    transportBar.onClickChanged = [this](bool clickEnabled)
    {
        metronomeEnabled = clickEnabled;
        engine.setMetronomeEnabled(metronomeEnabled);
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        saveAppSettings();
        transportBar.setStatusText(metronomeEnabled ? "Metronome on." : "Metronome off.");
    };
    transportBar.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    transportBar.onOpenProfilePageRequested = [this]
    {
        openLagDaemonProfile();
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
        saveSessionToDisk();
    };

    trackerPanel.onTrackArmChanged = [this](int trackIndex, bool shouldArm)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
            return;

        armedTracks[(size_t) trackIndex] = shouldArm;
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
        mixerPanel.setChannelGain(trackIndex, gain);
        midiSurface.setChannelGain(trackIndex, gain);
        saveSessionToDisk();
    };

    trackerPanel.onTrackInputChanged = [this](int trackIndex, int inputChannel)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

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

    arrangeView.onAssetPreviewRequested = [this](const juce::File& assetFile)
    {
        if (! projectManager.hasProject() || ! assetFile.existsAsFile())
            return;

        WorkstationAudioEngine::PreviewSettings settings;
        settings.startNormalized = arrangeView.getTrimStart();
        settings.endNormalized = arrangeView.getTrimEnd();
        settings.gainDecibels = arrangeView.getGainDecibels();
        settings.fadeInNormalized = arrangeView.getFadeInNormalized();
        settings.fadeOutNormalized = arrangeView.getFadeOutNormalized();
        settings.reverse = arrangeView.isReverseEnabled();
        settings.normalize = arrangeView.isNormalizeEnabled();

        juce::String errorMessage;
        if (engine.previewAssetFile(assetFile, settings, errorMessage))
            transportBar.setStatusText("Previewing slice: " + assetFile.getFileName());
        else if (errorMessage.isNotEmpty())
            transportBar.setStatusText(errorMessage);
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
        if (! projectManager.hasProject())
        {
            juce::String projectError;
            if (! projectManager.createProject("Untitled Project", projectError))
            {
                transportBar.setStatusText("Could not create a project for rendered sounds.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
        }

        juce::String errorMessage;
        auto renderedFile = projectManager.saveGeneratedAssetFile(buffer, sampleRate, suggestedName, errorMessage);
        if (renderedFile.existsAsFile())
        {
            if (engine.getTrackCount() == 0)
                addTrack();

            auto targetTrack = trackerPanel.getSelectedTrack();
            if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
                targetTrack = 0;

            juce::String clipError;
            auto clipIndex = timelineModel.addClip(cs::ClipKind::audio,
                                                   targetTrack,
                                                   suggestedName,
                                                   renderedFile.getFileName(),
                                                   "signal",
                                                   renderedFile,
                                                   timelineModel.getTransportSeconds(),
                                                   0.05,
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
        if (! projectManager.hasProject())
        {
            juce::String projectError;
            if (! projectManager.createProject("Untitled Project", projectError))
            {
                transportBar.setStatusText("Could not create a project for patch export.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
        }

        juce::String errorMessage;
        auto patchFile = projectManager.savePatchFile(patchJson, suggestedName, errorMessage);
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
        if (! projectManager.hasProject())
        {
            juce::String projectError;
            if (! projectManager.createProject("Untitled Project", projectError))
            {
                transportBar.setStatusText("Could not create a project for this sound.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
        }

        juce::String errorMessage;
        auto patchFile = projectManager.savePatchFile(patchJson, suggestedName, errorMessage);
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
        auto startDirectory = projectManager.hasProject()
            ? projectManager.getCurrentProject().dslDirectory.getChildFile("Patches")
            : projectManager.getProjectsRoot();

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

    dslPanel.onArtifactExportRequested = [this](const juce::String& artifactJson, const juce::String& suggestedName)
    {
        if (! projectManager.hasProject())
        {
            juce::String projectError;
            if (! projectManager.createProject("Untitled Project", projectError))
            {
                transportBar.setStatusText("Could not create a project for Patina artifact export.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
        }

        juce::String errorMessage;
        auto artifactFile = projectManager.savePatinaArtifactFile(artifactJson, suggestedName, errorMessage);
        if (artifactFile.existsAsFile())
        {
            saveSessionToDisk();
            transportBar.setStatusText("Exported Patina artifact: " + artifactFile.getFileName());
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    dslPanel.onArtifactSaveToLibraryRequested = [this](const juce::String& artifactJson, const juce::String& suggestedName)
    {
        if (! ensureStorageRootConfigured())
            return;

        juce::String errorMessage;
        auto artifactFile = projectManager.saveUserPatinaArtifactFile(artifactJson, suggestedName, errorMessage);
        if (artifactFile.existsAsFile())
        {
            refreshContentLibrary();
            transportBar.setStatusText("Saved Patina artifact to your library: " + artifactFile.getFileName());
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    dslPanel.onArtifactLoadRequested = [this]
    {
        auto startDirectory = projectManager.hasProject()
            ? projectManager.getCurrentProject().dslDirectory.getChildFile("Patina")
            : (projectManager.hasStorageRoot() ? projectManager.getStorageRoot() : juce::File{});

        patinaArtifactChooser = std::make_unique<juce::FileChooser>("Load a Patina artifact",
                                                                    startDirectory,
                                                                    "*.patina.json");

        patinaArtifactChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                           [this](const juce::FileChooser& chooser)
                                           {
                                               auto file = chooser.getResult();
                                               patinaArtifactChooser.reset();

                                               if (! file.existsAsFile())
                                                   return;

                                               cw::patina::ArtifactLoader loader;
                                               cw::patina::ir::Document document;
                                               juce::String errorMessage;
                                               if (! loader.loadJson(file.loadFileAsString(), document, errorMessage))
                                               {
                                                   transportBar.setStatusText(errorMessage);
                                                   return;
                                               }

                                               dslPanel.showLoadedArtifactSummary(document, file);
                                               transportBar.setStatusText("Loaded Patina artifact: " + file.getFileName());
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
        auto isOllama = providerName.toLowerCase().contains("ollama");
        auto previousEndpoint = aiProviderSettings.baseUrl.trim();
        auto previousEndpointLower = previousEndpoint.toLowerCase();
        aiProviderSettings.providerName = isOllama ? "Ollama" : "OpenAI";
        if (isOllama)
        {
            if (previousEndpoint.isEmpty() || previousEndpointLower.contains("api.openai.com"))
                aiProviderSettings.baseUrl = "http://localhost:11434";
        }
        else
        {
            if (previousEndpoint.isEmpty() || previousEndpointLower.contains("localhost:11434") || previousEndpointLower.contains("127.0.0.1:11434"))
                aiProviderSettings.baseUrl = "https://api.openai.com/v1";
        }
        aiProviderSettings.modelName = isOllama ? juce::String("llama3.2:3b") : juce::String("gpt-4.1-mini");
        aiPanel.setSelectedProvider(aiProviderSettings.providerName);
        aiPanel.setSelectedModel(aiProviderSettings.modelName);
        settingsPanel.setAiProviderSettings(aiProviderSettings);

        saveAppSettings();

        refreshAiModelCatalog();
        transportBar.setStatusText("AI provider: " + aiProviderSettings.providerName + ".");
    };

    aiPanel.onPromptSubmitted = [this](const juce::String& submittedPrompt)
    {
        refreshAiContextStore();
        pendingAiPrompt = submittedPrompt;

        CreationStationContextEngine::RetrievalRequest request;
        request.prompt = pendingAiPrompt;
        request.workspaceMode = workspaceModeName(activeMode).toLowerCase();
        request.projectName = projectManager.hasProject() ? projectManager.getDisplayLabel() : juce::String();
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
    settingsPanel.onChangeStorageRequested = [this] { chooseStorageRoot(true); };
    settingsPanel.onProjectMetadataChanged = [this](const ProjectManager::ProjectInfo& metadata)
    {
        juce::String errorMessage;
        if (! projectManager.updateProjectMetadata(metadata, errorMessage))
        {
            transportBar.setStatusText(errorMessage);
            return;
        }

        settingsPanel.setProjectMetadata(projectManager.getCurrentProject());
        transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
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
    settingsPanel.onAutoloadChanged = [this](bool enabled)
    {
        autoloadLastProject = enabled;
        saveAppSettings();
    };
    settingsPanel.onAiProviderSettingsChanged = [this](const AiProviderSettings& settings)
    {
        aiProviderSettings = settings;
        aiPanel.setSelectedProvider(aiProviderSettings.providerName);
        aiPanel.setSelectedModel(aiProviderSettings.modelName);

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

        projectManager.getContentDirectory().revealToUser();
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
                                                                   projectManager.getStorageRoot(),
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

    contentPanel.onOpenProjectAssetRequested = [this](const ProjectManager::ProjectAsset& asset)
    {
        openProjectAsset(asset);
    };

    contentPanel.onPlaceProjectAssetRequested = [this](const ProjectManager::ProjectAsset& asset)
    {
        placeProjectAssetOnTracker(asset);
    };

    contentPanel.onExportProjectAssetRequested = [this](const ProjectManager::ProjectAsset& asset)
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
        if (pluginEditorWindow != nullptr)
            pluginEditorWindow.reset();

        if (pluginRackBar.isTrackContext())
            engine.unloadTrackPlugin(pluginRackBar.getTrackIndex());
        else
            engine.unloadMasterPlugin();

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

        if (pluginEditorWindow != nullptr)
        {
            pluginEditorWindow->toFront(true);
            return;
        }

        auto* editor = isTrackContext ? engine.createTrackPluginEditor(pluginRackBar.getTrackIndex())
                                      : engine.createMasterPluginEditor();

        if (editor != nullptr)
        {
            auto windowTitle = isTrackContext ? ("Track " + juce::String(pluginRackBar.getTrackIndex() + 1) + " Editor")
                                              : "Master Editor";
            auto window = std::make_unique<ManagedDocumentWindow>(windowTitle,
                                                                  juce::Colour(0xff11151c),
                                                                  juce::DocumentWindow::closeButton,
                                                                  [this]
                                                                  {
                                                                      pluginEditorWindow.reset();
                                                                  });
            window->setUsingNativeTitleBar(true);
            window->setResizable(true, true);
            window->setContentOwned(editor, true);
            window->centreWithSize(900, 650);
            window->setVisible(true);
            pluginEditorWindow = std::move(window);
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

        if (pluginEditorWindow != nullptr)
        {
            pluginEditorWindow->toFront(true);
            return;
        }

        if (auto* editor = engine.createGraphVstPluginEditor())
        {
            auto window = std::make_unique<ManagedDocumentWindow>("Patch VST Editor",
                                                                  juce::Colour(0xff11151c),
                                                                  juce::DocumentWindow::closeButton,
                                                                  [this]
                                                                  {
                                                                      pluginEditorWindow.reset();
                                                                  });
            window->setUsingNativeTitleBar(true);
            window->setResizable(true, true);
            window->setContentOwned(editor, true);
            window->centreWithSize(900, 650);
            window->setVisible(true);
            pluginEditorWindow = std::move(window);
        }
    };

    pluginsPanel.onAddPathRequested = [this]
    {
        configureVstSearchPaths();
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
            engine.setTrackGain(index, value);

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
        if (pluginEditorWindow != nullptr)
            pluginEditorWindow.reset();
        refreshVisibleBank();
    };

    mixerPanel.onPanChanged = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
            engine.setTrackPan(index, value);

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
            constexpr int modeCount = static_cast<int>(WorkspaceMode::settings) + 1;
            modeIndex = (modeIndex + step + modeCount) % modeCount;
            setWorkspaceMode(static_cast<WorkspaceMode>(modeIndex));
        };

        auto setMode = [this](WorkspaceMode mode)
        {
            setWorkspaceMode(mode);
        };

        if (button == "cursor_left")
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() - 1 : mixerPanel.getBankOffset();
            selectTrack(juce::jmax(0, trackIndex));
        }
        else if (button == "cursor_right")
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() + 1 : mixerPanel.getBankOffset();
            if (engine.getTrackCount() > 0)
                selectTrack(juce::jlimit(0, engine.getTrackCount() - 1, trackIndex));
        }
        else if (button == "cursor_up")
        {
            advanceMode(-1);
        }
        else if (button == "cursor_down")
        {
            advanceMode(1);
        }
        else if (button == "assign_track")
        {
            setMode(WorkspaceMode::mix);
        }
        else if (button == "assign_send")
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == "assign_pan")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "assign_plugin")
        {
            setMode(WorkspaceMode::node);
        }
        else if (button == "assign_eq")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "assign_instrument")
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
        else if (button == "transport_cycle")
        {
            transportBar.loopButton.triggerClick();
        }
        else if (button == "transport_solo")
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
        else if (button == "transport_marker")
        {
            transportBar.setStatusText("Marker pressed.");
        }
        else if (button == "transport_nudge")
        {
            transportBar.setStatusText("Nudge pressed.");
        }
        else if (button == "transport_drop")
        {
            transportBar.setStatusText("Drop pressed.");
        }
        else if (button == "transport_replace")
        {
            transportBar.setStatusText("Replace pressed.");
        }
        else if (button == "zoom")
        {
            if (pluginRackBar.isTrackContext() && engine.hasTrackPlugin(pluginRackBar.getTrackIndex()))
            {
                if (pluginEditorWindow != nullptr)
                    pluginEditorWindow.reset();

                auto* editor = engine.createTrackPluginEditor(pluginRackBar.getTrackIndex());
                if (editor != nullptr)
                {
                    auto window = std::make_unique<ManagedDocumentWindow>("Track Editor",
                                                                          juce::Colour(0xff11151c),
                                                                          juce::DocumentWindow::closeButton,
                                                                          [this]
                                                                          {
                                                                              pluginEditorWindow.reset();
                                                                          });
                    window->setUsingNativeTitleBar(true);
                    window->setResizable(true, true);
                    window->setContentOwned(editor, true);
                    window->centreWithSize(900, 650);
                    window->setVisible(true);
                    pluginEditorWindow = std::move(window);
                }
            }
            else
            {
                setWorkspaceMode(WorkspaceMode::node);
            }
        }
        else if (button == "scrub")
        {
            engine.setGraphEnabled(! engine.isGraphEnabled());
            graphPanel.setEnabled(engine.isGraphEnabled());
            setWorkspaceMode(WorkspaceMode::node);
        }
        else if (button == "user_a")
        {
            saveSessionToDisk();
            transportBar.setStatusText("Session saved from X-Touch");
        }
        else if (button == "user_b")
        {
            loadSessionFromDisk();
            refreshVisibleBank();
            transportBar.setStatusText("Session reloaded from X-Touch");
        }
    };

    midiSurface.onFaderMoved = [this](int index, float value)
    {
        engine.setTrackGain(index, value);

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
            engine.setTrackPan(index, value);

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

    midiSurface.onTransportCommand = [this](XTouchControlSurface::TransportCommand command)
    {
        auto safeBar = transportBarSafe;

        switch (command)
        {
            case XTouchControlSurface::TransportCommand::play:
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
                if (engine.isRecording())
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
    pluginEditorWindow.reset();
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

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Save changes?")
        .withMessage("This project has unsaved changes. Save before closing?")
        .withButton("Save")
        .withButton("Don't Save")
        .withButton("Cancel");

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::AlertWindow::showAsync(options,
                                 [safeThis, onDecision](int result)
                                 {
                                     if (safeThis == nullptr)
                                     {
                                         if (onDecision)
                                             onDecision(false);
                                         return;
                                     }

                                     if (result == 1)
                                     {
                                         if (! safeThis->projectManager.hasProject())
                                         {
                                             safeThis->createNewProject();
                                             safeThis->transportBar.setStatusText("Create the project, then close again to save and quit.");
                                             if (onDecision)
                                                 onDecision(false);
                                             return;
                                         }

                                         safeThis->saveProject();
                                         if (onDecision)
                                             onDecision(! safeThis->projectDirty);
                                         return;
                                     }

                                     if (result == 2)
                                     {
                                         if (onDecision)
                                             onDecision(true);
                                         return;
                                     }

                                     if (onDecision)
                                         onDecision(false);
                       });
}

void MainComponent::timerCallback()
{
    refreshTrackInputSources();

    if (layoutDirty && juce::Time::getMillisecondCounterHiRes() * 0.001 - layoutLastChangeWallSeconds > 0.75)
        saveLayoutToDisk();

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

    for (int index = 0; index < engine.getTrackCount(); ++index)
        trackerPanel.setTrackLevel(index, engine.getTrackLevel(index));
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
                                                                    static_cast<int>(WorkspaceMode::settings),
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
    if (! projectManager.hasStorageRoot())
        return;

    auto layoutFile = projectManager.getLayoutPackageFile("last-used");
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
    if (! projectManager.hasStorageRoot())
        return;

    auto layoutFile = projectManager.getLayoutPackageFile("last-used");
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
        transportBar.setProfile(session);
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
                                                          juce::DocumentWindow::closeButton,
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
        engine.unloadTrackPlugin(pluginRackBar.getTrackIndex(), slotIndex);
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
                                                          juce::DocumentWindow::closeButton,
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

void MainComponent::refreshFxStackWindow()
{
    if (fxStackPanel == nullptr || ! pluginRackBar.isTrackContext())
        return;

    const auto trackIndex = pluginRackBar.getTrackIndex();
    fxStackPanel->setTrackName("Track " + juce::String(trackIndex + 1) + " - " + engine.getTrackName(trackIndex));
    fxStackPanel->setPlugins(engine.getTrackPluginNames(trackIndex),
                             engine.getTrackPluginBypassStates(trackIndex));
}

void MainComponent::openTrackPluginEditor(int trackIndex, int slotIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    if (pluginEditorWindow != nullptr)
        pluginEditorWindow.reset();

    auto* editor = engine.createTrackPluginEditor(trackIndex, slotIndex);
    if (editor == nullptr)
        return;

    auto pluginNames = engine.getTrackPluginNames(trackIndex);
    auto pluginName = juce::isPositiveAndBelow(slotIndex, pluginNames.size()) ? pluginNames[slotIndex]
                                                                              : "Plugin";
    auto window = std::make_unique<ManagedDocumentWindow>("Track " + juce::String(trackIndex + 1) + " - " + pluginName,
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::closeButton,
                                                          [this]
                                                          {
                                                              pluginEditorWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(editor, true);
    window->centreWithSize(900, 650);
    window->setVisible(true);
    pluginEditorWindow = std::move(window);
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

void MainComponent::editControlSurfaceMappings()
{
    if (! ensureStorageRootConfigured())
        return;

    auto mappingsFile = projectManager.getControlSurfaceMappingsFile();
    ControlSurfaceMappingStore mappings;
    juce::String errorMessage;

    if (! projectManager.loadControlSurfaceMappings(mappings, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    if (mappings.getProfiles().isEmpty())
    {
        mappings = ControlSurfaceMappingStore::createDefaultLibrary();

        if (! projectManager.saveControlSurfaceMappings(mappings, errorMessage))
        {
            transportBar.setStatusText(errorMessage);
            return;
        }
    }

    if (! mappingsFile.startAsProcess())
        mappingsFile.revealToUser();

    transportBar.setStatusText("Opened control surface mappings.");
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

    if (pluginEditorWindow != nullptr)
        pluginEditorWindow.reset();

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

    if (! projectManager.hasProject())
    {
        juce::String errorMessage;
        if (! projectManager.createProject("Untitled Project", errorMessage))
        {
            transportBar.setStatusText("Could not create a project for imported sounds.");
            return;
        }

        transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
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
                                      auto imported = projectManager.importAssetFile(sourceFile, errorMessage);
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
    if (! projectManager.hasProject())
    {
        arrangeView.setAssetFiles({});
        contentPanel.setProjectAssets({});
        refreshAiContextStore();
        return;
    }

    auto assetFiles = projectManager.listAssetFiles();
    arrangeView.setAssetFiles(assetFiles);
    contentPanel.setProjectAssets(projectManager.listProjectAssets());
    refreshAiContextStore();
}

void MainComponent::refreshContentLibrary()
{
    if (! projectManager.hasStorageRoot())
    {
        contentPanel.setStoragePath({});
        contentPanel.setItems({});
        contentPanel.setTutorialItems({});
        contentPanel.setStatusText("Choose a local storage location to initialize the content library.");
        refreshAiContextStore();
        return;
    }

    contentPanel.setStoragePath(projectManager.getStorageRoot().getFullPathName());

    juce::String errorMessage;
    if (! contentLibrary.loadFromStorage(projectManager.getBuiltInContentDirectory(),
                                         projectManager.getDownloadedContentDirectory(),
                                         projectManager.getUserContentDirectory(),
                                         projectManager.getContentManifestFile(),
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

    if (! projectManager.hasStorageRoot())
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

    collect(projectManager.getBuiltInTutorialDirectory(), true);
    collect(projectManager.getUserTutorialDirectory(), false);
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
    if (! projectManager.hasStorageRoot())
    {
        contextEngine.clearDocuments();
        return;
    }

    juce::String errorMessage;
    if (! contextStore.rebuild(projectManager,
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
    if (projectManager.hasStorageRoot())
    {
        auto builtInDirectory = projectManager.getBuiltInTutorialDirectory();
        auto sampleFile = builtInDirectory.getChildFile("getting-started-demo.nalm");
        auto builtInSource = cw::tutorial::getBuiltInGettingStartedTutorialSource();
        auto vstDemoFile = builtInDirectory.getChildFile("vst-node-demo.nalm");
        auto vstDemoSource = cw::tutorial::getBuiltInVstNodeDemoTutorialSource();

        if (! sampleFile.existsAsFile() || sampleFile.loadFileAsString() != builtInSource)
            sampleFile.replaceWithText(builtInSource);

        if (! vstDemoFile.existsAsFile() || vstDemoFile.loadFileAsString() != vstDemoSource)
            vstDemoFile.replaceWithText(vstDemoSource);

        auto userFile = projectManager.getUserTutorialDirectory().getChildFile("getting-started-demo.nalm");

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

    auto providerIsOllama = aiProviderSettings.providerName.toLowerCase().contains("ollama");
    if (! providerIsOllama && aiProviderSettings.apiKey.trim().isEmpty())
    {
        aiPanel.setAssistantResponse("Enter your OpenAI API key in Settings first.");
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

        OpenAiChatClient::ChatResult result;
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
    if (! projectManager.hasStorageRoot())
    {
        settingsPanel.setAvailableAiModels({}, "Choose a storage folder first.");
        aiPanel.setAvailableModels({}, "Choose a storage folder first.");
        return;
    }

    auto providerIsOllama = aiProviderSettings.providerName.toLowerCase().contains("ollama");
    if (! providerIsOllama && aiProviderSettings.apiKey.trim().isEmpty())
    {
        settingsPanel.setAvailableAiModels({}, "Enter your OpenAI API key, then refresh the list.");
        aiPanel.setAvailableModels({}, "Enter your OpenAI API key, then refresh the list.");
        return;
    }

    juce::StringArray modelIds;
    juce::String errorMessage;
    if (! modelCatalogClient.fetchModelIds(aiProviderSettings.baseUrl,
                                           aiProviderSettings.providerName,
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

    if (! projectManager.hasStorageRoot())
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

    auto destination = projectManager.getDownloadedContentDirectory()
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
        if (! projectManager.hasProject())
        {
            contentPanel.setStatusText("Open or create a project before importing library audio into Foley.");
            return;
        }

        juce::String errorMessage;
        auto importedFile = projectManager.importAssetFile(item.file, errorMessage);
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

void MainComponent::openProjectAsset(const ProjectManager::ProjectAsset& asset)
{
    if (! asset.file.existsAsFile())
    {
        contentPanel.setStatusText("That project asset is missing on disk.");
        return;
    }

    if (asset.type == "signalPatch")
    {
        juce::String errorMessage;
        cw::PatchDocument document;
        if (! cw::parsePatchDocumentJson(asset.file.loadFileAsString(), document, errorMessage))
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
        transportBar.setStatusText("Opened project sound: " + asset.name);
        return;
    }

    if (asset.type == "audioFile" || asset.type == "render")
    {
        juce::String errorMessage;
        if (! engine.previewAssetFile(asset.file, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        transportBar.setStatusText("Previewing project asset: " + asset.name);
        return;
    }

    asset.file.revealToUser();
    transportBar.setStatusText("Revealed project asset: " + asset.name);
}

void MainComponent::placeProjectAssetOnTracker(const ProjectManager::ProjectAsset& asset)
{
    if (! asset.file.existsAsFile())
    {
        contentPanel.setStatusText("That project asset is missing on disk.");
        return;
    }

    if (asset.type != "audioFile" && asset.type != "render")
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
    auto sourceTool = asset.type == "render" ? "signal" : "project-audio";
    auto clipIndex = timelineModel.addClip(cs::ClipKind::audio,
                                           targetTrack,
                                           asset.name,
                                           asset.file.getFileName(),
                                           sourceTool,
                                           asset.file,
                                           timelineModel.getTransportSeconds(),
                                           0.05,
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
    transportBar.setStatusText("Placed project asset on Tracker: " + asset.name);
}

void MainComponent::exportProjectAssetRaw(const ProjectManager::ProjectAsset& asset)
{
    if (! asset.file.existsAsFile())
    {
        contentPanel.setStatusText("That project asset is missing on disk.");
        return;
    }

    if (asset.type != "audioFile" && asset.type != "render")
    {
        contentPanel.setStatusText("Only project WAV/render audio can be exported raw right now.");
        return;
    }

    rawAssetExportChooser = std::make_unique<juce::FileChooser>("Export raw project audio",
                                                                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                    .getChildFile(asset.file.getFileName()),
                                                                "*.wav",
                                                                true);
    auto chooser = rawAssetExportChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser, asset](const juce::FileChooser& result)
                         {
                             auto destination = result.getResult();
                             if (chooser == rawAssetExportChooser.get())
                                 rawAssetExportChooser.reset();

                             if (destination.getFullPathName().isEmpty())
                                 return;

                             if (destination.getFileExtension().isEmpty())
                                 destination = destination.withFileExtension(asset.file.getFileExtension());

                             if (destination.existsAsFile() && ! destination.deleteFile())
                             {
                                 contentPanel.setStatusText("Could not replace the existing export file.");
                                 return;
                             }

                             if (! asset.file.copyFileTo(destination))
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

    if (! projectManager.hasProject())
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

    auto renderName = projectManager.getCurrentProject().slug + "-full-mix";
    auto renderFile = projectManager.saveRenderFile(renderedMix, settings.sampleRate, renderName, errorMessage);
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

    auto defaultName = projectManager.hasProject() ? projectManager.getCurrentProject().slug + "-full-mix.wav"
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
    if (! projectManager.hasProject())
        return;

    juce::String errorMessage;
    if (! engine.setFoleyArrangement(arrangeView.createState(), projectManager.getCurrentProject().assetsDirectory, errorMessage)
        && errorMessage.isNotEmpty())
    {
        transportBar.setStatusText(errorMessage);
    }
}

void MainComponent::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "New Project");
    menu.addItem(2, "New Project From Template...");
    menu.addItem(3, "Open Project...");
    menu.addSeparator();
    menu.addItem(4, "Save Project");
    menu.addItem(5, "Save Project As...");
    menu.addItem(6, "Save Project As Template...");
    menu.addItem(7, "Open Project Folder");
    menu.addSeparator();
    menu.addItem(8, "Render Full Mix to Project");
    menu.addItem(9, "Export Full Mix as WAV...");
    menu.addSeparator();
    menu.addItem(10, "Autoload Last Project", true, autoloadLastProject);

    auto screenArea = transportBar.getProjectButtonScreenBounds();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenArea),
                       [this](int result)
                       {
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
                               case 10:
                               {
                                   autoloadLastProject = ! autoloadLastProject;
                                   settingsPanel.setAutoloadEnabled(autoloadLastProject);
                                   saveAppSettings();
                                   break;
                               }
                               default: break;
                           }
                       });
}

void MainComponent::createNewProject()
{
    guardUnsavedProjectChange("creating a new project", [this] { beginCreateNewProject(); });
}

void MainComponent::beginCreateNewProject()
{
    if (! ensureStorageRootConfigured())
        return;

    auto* nameEditor = new juce::AlertWindow("New Project",
                                             "Give the project a name.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", "Untitled Project");
    nameEditor->addButton("Create", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto name = dialog->getTextEditorContents("projectName").trim();
        juce::String errorMessage;
        if (! options->projectManager.createProject(name, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        options->transportBar.setProjectLabel("Project: " + options->projectManager.getDisplayLabel());
        options->settingsPanel.setProjectMetadata(options->projectManager.getCurrentProject());
        options->refreshProjectAssets();
        options->saveSessionToDisk(true);
        options->transportBar.setStatusText("Project saved: " + options->projectManager.getProjectPackageFile().getFileName());
    }), true);
}

void MainComponent::openProject()
{
    guardUnsavedProjectChange("opening another project", [this] { beginOpenProject(); });
}

void MainComponent::beginOpenProject()
{
    if (! ensureStorageRootConfigured())
        return;

    projectChooser = std::make_unique<juce::FileChooser>("Open a Creation Station project",
                                                         projectManager.getProjectsRoot(),
                                                         "*.csp",
                                                         true);

    auto chooser = projectChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser](const juce::FileChooser& result)
                         {
                             auto selected = result.getResult();
                             if (! selected.exists())
                                 return;

                             juce::String errorMessage;
                             juce::ValueTree restoredState;
                             auto opened = selected.isDirectory()
                                               ? projectManager.openProject(selected, errorMessage)
                                               : projectManager.openProjectPackage(selected, restoredState, errorMessage);

                             if (! opened)
                             {
                                 juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                        "Project Error",
                                                                        errorMessage);
                                 return;
                             }

                             if (restoredState.isValid())
                             {
                                 remapTemplateStateFilesToCurrentProject(restoredState);
                                 projectManager.saveProjectState(restoredState);
                             }

                             transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
                             settingsPanel.setProjectMetadata(projectManager.getCurrentProject());
                             refreshProjectAssets();
                             loadSessionFromDisk();
                         });
}

void MainComponent::saveProject()
{
    if (! projectManager.hasProject())
    {
        createNewProject();
        return;
    }

    saveSessionToDisk(true);
    if (! projectDirty)
        transportBar.setStatusText("Project saved: " + projectManager.getProjectPackageFile().getFileName());
}

void MainComponent::saveProjectAs()
{
    if (! projectManager.hasProject())
    {
        createNewProject();
        return;
    }

    auto* nameEditor = new juce::AlertWindow("Save Project As",
                                             "Give the copied project a new name.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", projectManager.getCurrentProject().name + " Copy");
    nameEditor->addButton("Save As", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto state = options->createProjectStateForSave();
        auto projectName = dialog->getTextEditorContents("projectName").trim();
        juce::String errorMessage;
        if (! options->projectManager.saveCurrentProjectAs(projectName, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        options->remapTemplateStateFilesToCurrentProject(state);
        options->projectManager.saveProjectState(state);
        options->transportBar.setProjectLabel("Project: " + options->projectManager.getDisplayLabel());
        options->settingsPanel.setProjectMetadata(options->projectManager.getCurrentProject());
        options->refreshProjectAssets();
        options->loadSessionFromDisk();
        options->saveSessionToDisk(true);
        options->transportBar.setStatusText("Saved project as: " + options->projectManager.getProjectPackageFile().getFileName());
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
                                                         projectManager.getTemplatesRoot(),
                                                         "*.cst",
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
                                 juce::ValueTree restoredState;
                                 juce::String errorMessage;
                                 if (! options->projectManager.createProjectFromTemplate(templateFile, projectName, restoredState, errorMessage))
                                 {
                                     juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                            "Template Error",
                                                                            errorMessage);
                                     return;
                                 }

                                 if (restoredState.isValid())
                                 {
                                     options->remapTemplateStateFilesToCurrentProject(restoredState);
                                     options->projectManager.saveProjectState(restoredState);
                                 }

                                 options->transportBar.setProjectLabel("Project: " + options->projectManager.getDisplayLabel());
                                 options->settingsPanel.setProjectMetadata(options->projectManager.getCurrentProject());
                                 options->refreshProjectAssets();
                                 options->loadSessionFromDisk();
                                 options->saveSessionToDisk(true);
                                 options->transportBar.setStatusText("Project created from template.");
                             }), true);
                         });
}

void MainComponent::saveProjectAsTemplate()
{
    if (! projectManager.hasProject())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Template Needs A Project",
                                               "Create or open a project first, then save it as a template.");
        return;
    }

    auto* nameEditor = new juce::AlertWindow("Save Project As Template",
                                             "Name this reusable studio setup.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("templateName", projectManager.getCurrentProject().name + " Template");
    nameEditor->addButton("Save Template", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto templateName = dialog->getTextEditorContents("templateName").trim();
        auto state = options->createProjectStateForSave();
        juce::File templateFile;
        juce::String errorMessage;
        if (! options->projectManager.saveTemplatePackage(state, templateName, templateFile, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Template Error",
                                                   errorMessage);
            return;
        }

        options->transportBar.setStatusText("Template saved: " + templateFile.getFileName());
    }), true);
}

void MainComponent::guardUnsavedProjectChange(const juce::String& actionName, const std::function<void()>& action)
{
    if (! projectDirty)
    {
        if (action)
            action();
        return;
    }

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Save changes?")
        .withMessage("Save this project before " + actionName + "?")
        .withButton("Save")
        .withButton("Don't Save")
        .withButton("Cancel");

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showAsync(options,
                                 [safeThis, action](int result)
                                 {
                                     if (safeThis == nullptr)
                                         return;

                                     if (result == 1)
                                     {
                                         if (! safeThis->projectManager.hasProject())
                                         {
                                             safeThis->beginCreateNewProject();
                                             safeThis->transportBar.setStatusText("Create the project first, then choose that action again.");
                                             return;
                                         }

                                         safeThis->saveProject();
                                         if (! safeThis->projectDirty && action)
                                             action();
                                         return;
                                     }

                                     if (result == 2 && action)
                                         action();
                                 });
}

juce::String MainComponent::createRecordingTakeName() const
{
    return "Take-" + makeRecordingTimestamp() + ".wav";
}

void MainComponent::refreshRecentTakes()
{
    if (! projectManager.hasProject())
    {
        recordView.setRecentTakes({});
        arrangeView.setRecordedClips({});
        refreshProjectAssets();
        return;
    }

    auto audioDirectory = projectManager.getCurrentProject().audioDirectory;
    juce::Array<juce::File> takeFiles;
    audioDirectory.findChildFiles(takeFiles, juce::File::findFiles, false, "*.wav");

    juce::StringArray names;
    for (int index = 0; index < juce::jmin(10, takeFiles.size()); ++index)
        names.add(takeFiles[(size_t) index].getFileName());

    recordView.setRecentTakes(names);
    arrangeView.setRecordedClips(names);
    refreshProjectAssets();
}

bool MainComponent::startRecordingSession()
{
    if (! ensureStorageRootConfigured())
        return false;

    if (! projectManager.hasProject())
    {
        juce::String errorMessage;
        if (! projectManager.createProject("Untitled Project", errorMessage))
        {
            transportBar.setStatusText("Could not create a project for recording.");
            return false;
        }

        transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
        refreshProjectAssets();
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
    const auto timestamp = makeRecordingTimestamp();

    for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) trackIndex])
            continue;

        auto trackName = engine.getTrackName(trackIndex).retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        trackName = trackName.trim().replace(" ", "-");

        if (trackName.isEmpty())
            trackName = "Track-" + juce::String(trackIndex + 1).paddedLeft('0', 2);

        WorkstationAudioEngine::RecordingTarget target;
        target.trackIndex = trackIndex;
        target.file = projectManager.getCurrentProject().audioDirectory.getChildFile("Take-" + timestamp
                                                                                    + "-T" + juce::String(trackIndex + 1).paddedLeft('0', 2)
                                                                                    + "-" + trackName
                                                                                    + ".wav");
        recordingTargets.add(target);
    }

    juce::String errorMessage;
    if (! engine.startRecordingToFiles(recordingTargets, errorMessage))
    {
        transportBar.setStatusText("Record failed: " + errorMessage);
        return false;
    }

    for (const auto& target : recordingTargets)
        timelineModel.beginRecordingClip(target.trackIndex, target.file);

    trackerPanel.refreshTimelineView();
    transportBar.setStatusText("Recording " + juce::String(recordingTargets.size()) + " track(s).");
    recordView.setRecordingState(true, recordingTargets.size() == 1 ? recordingTargets[0].file.getFileName()
                                                                    : juce::String(recordingTargets.size()) + " tracks");
    refreshRecentTakes();
    return true;
}

void MainComponent::stopRecordingSession()
{
    if (! engine.isRecording())
        return;

    auto takeFiles = engine.getRecordingFiles();
    engine.stopRecording();
    timelineModel.finishRecordingClip(timelineModel.getTransportSeconds());
    activeRecordingTrack = -1;
    trackerPanel.refreshTimelineView();
    midiSurface.setTransportState(false, false);
    transportBar.setStatusText("Recording stopped: " + juce::String(takeFiles.size()) + " track(s).");
    recordView.setRecordingState(false, takeFiles.size() == 1 ? takeFiles[0].getFileName()
                                                              : juce::String(takeFiles.size()) + " tracks");
    refreshRecentTakes();
    saveSessionToDisk();
}

void MainComponent::revealProjectFolder()
{
    if (! projectManager.hasProject())
        return;

    projectManager.getCurrentProject().rootDirectory.revealToUser();
}

bool MainComponent::chooseStorageRoot(bool promptWhenAlreadyConfigured)
{
    if (projectManager.hasStorageRoot() && ! promptWhenAlreadyConfigured)
        return true;

    if (storageRootChooser != nullptr)
        return false;

    transportBar.setStatusText("Choose a local storage folder for Creation Station.");
    storageRootChooser = std::make_unique<juce::FileChooser>("Choose a local storage folder for Creation Station",
                                                             juce::File{},
                                                             juce::String{},
                                                             true);

    storageRootChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [this](const juce::FileChooser& chooser)
                                    {
                                        auto selectedDirectory = chooser.getResult();
                                        storageRootChooser.reset();

                                        if (! selectedDirectory.isDirectory())
                                        {
                                            transportBar.setStatusText("Storage location is required before the studio can save projects or content.");
                                            return;
                                        }

                                        juce::String errorMessage;
                                        if (! projectManager.setStorageRoot(selectedDirectory, errorMessage))
                                        {
                                            transportBar.setStatusText(errorMessage);
                                            return;
                                        }

                                        settingsPanel.setStoragePath(projectManager.getStorageRoot().getFullPathName());
                                        loadAppSettings();
                                        applySelectedAudioDeviceSettings();
                                        settingsPanel.setAutoloadEnabled(autoloadLastProject);
                                        settingsPanel.setAiProviderSettings(aiProviderSettings);

                                        if (autoloadLastProject)
                                        {
                                            if (! projectManager.loadLastProject())
                                            {
                                                juce::String projectError;
                                                projectManager.createProject("Untitled Project", projectError);
                                            }
                                        }
                                        else
                                        {
                                            juce::String projectError;
                                            projectManager.createProject("Untitled Project", projectError);
                                        }

                                        transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
                                        rescanVstCatalog();
                                        refreshProjectAssets();
                                        refreshContentLibrary();
                                        refreshRecentTakes();
                                        transportBar.setStatusText("Storage set to " + projectManager.getStorageRoot().getFullPathName());
                                    });

    return false;
}

bool MainComponent::ensureStorageRootConfigured()
{
    if (projectManager.hasStorageRoot())
        return true;

    transportBar.setStatusText("Choose a local storage folder for Creation Station.");
    if (! chooseStorageRoot())
    {
        transportBar.setStatusText("Storage location is required before the studio can save projects or content.");
        return false;
    }

    return true;
}



juce::File MainComponent::getSessionFile() const
{
    if (! projectManager.hasStorageRoot())
        return {};

    return projectManager.getConfigDirectory().getChildFile("session.xml");
}

juce::File MainComponent::getAppSettingsFile() const
{
    if (! projectManager.hasStorageRoot())
        return {};

    return projectManager.getConfigDirectory().getChildFile("settings.xml");
}

void MainComponent::saveAppSettings()
{
    auto settingsFile = getAppSettingsFile();
    if (settingsFile.getFullPathName().isEmpty())
        return;

    juce::ValueTree state("CreationStationSettings");
    state.setProperty("formatVersion", 1, nullptr);
    state.setProperty("autoloadLastProject", autoloadLastProject, nullptr);
    state.setProperty("metronomeEnabled", metronomeEnabled, nullptr);
    state.setProperty("audioSystem", selectedStudioAudioSystem, nullptr);
    state.setProperty("audioInputDevice", selectedStudioInputDevice, nullptr);
    state.setProperty("audioOutputDevice", selectedStudioOutputDevice, nullptr);
    state.setProperty("aiProviderName", aiProviderSettings.providerName, nullptr);
    state.setProperty("aiBaseUrl", aiProviderSettings.baseUrl, nullptr);
    state.setProperty("aiModelName", aiProviderSettings.modelName, nullptr);
    state.setProperty("aiApiKey", aiProviderSettings.apiKey, nullptr);
    state.addChild(studioIOModel.createState(), -1, nullptr);

    juce::ValueTree vstPathsState("VstSearchPaths");
    for (const auto& path : vstPluginCatalog.getSearchPaths())
    {
        juce::ValueTree pathState("Path");
        pathState.setProperty("value", path, nullptr);
        vstPathsState.addChild(pathState, -1, nullptr);
    }
    state.addChild(vstPathsState, -1, nullptr);

    settingsFile.getParentDirectory().createDirectory();
    if (auto xml = state.createXml())
        xml->writeTo(settingsFile);
}

void MainComponent::loadAppSettings()
{
    auto settingsFile = getAppSettingsFile();
    if (! settingsFile.existsAsFile())
    {
        autoloadLastProject = projectManager.shouldAutoloadLastProject();
        if (projectManager.loadAiProviderSettings(aiProviderSettings))
            settingsPanel.setAiProviderSettings(aiProviderSettings);
        vstPluginCatalog.setSearchPaths(projectManager.loadVstSearchPaths());
        transportBar.clickButton.setToggleState(metronomeEnabled, juce::dontSendNotification);
        engine.setMetronomeEnabled(metronomeEnabled);
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
    autoloadLastProject = (bool) state.getProperty("autoloadLastProject", autoloadLastProject);
    metronomeEnabled = (bool) state.getProperty("metronomeEnabled", metronomeEnabled);
    aiProviderSettings.providerName = state.getProperty("aiProviderName", aiProviderSettings.providerName).toString();
    aiProviderSettings.baseUrl = state.getProperty("aiBaseUrl", aiProviderSettings.baseUrl).toString();
    aiProviderSettings.modelName = state.getProperty("aiModelName", aiProviderSettings.modelName).toString();
    aiProviderSettings.apiKey = state.getProperty("aiApiKey", aiProviderSettings.apiKey).toString();

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

    settingsPanel.setAiProviderSettings(aiProviderSettings);
    transportBar.clickButton.setToggleState(metronomeEnabled, juce::dontSendNotification);
    engine.setMetronomeEnabled(metronomeEnabled);
    engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
}

void MainComponent::applySelectedAudioDeviceSettings()
{
    if (selectedStudioAudioSystem.isNotEmpty())
        deviceManager.setCurrentAudioDeviceType(selectedStudioAudioSystem, true);

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
    if (! projectManager.hasProject())
        return;

    juce::StringPairArray filesByName;
    const auto& project = projectManager.getCurrentProject();
    for (const auto& directory : { project.audioDirectory, project.assetsDirectory, project.dslDirectory, project.rendersDirectory })
    {
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, true, "*");
        for (const auto& file : files)
        {
            auto key = file.getFileName().toLowerCase();
            if (! filesByName.containsKey(key))
                filesByName.set(key, file.getFullPathName());
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
    if (! userInitiated)
    {
        projectDirty = true;
        return;
    }

    if (! projectManager.hasStorageRoot())
        return;

    auto state = createProjectStateForSave();

    projectManager.saveProjectState(state);

    auto sessionFile = getSessionFile();
    sessionFile.getParentDirectory().createDirectory();
    if (auto xml = state.createXml())
        xml->writeTo(sessionFile);

    juce::String packageError;
    if (! projectManager.saveProjectPackage(state, packageError))
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
}

bool MainComponent::buildTrackerPlaybackTargets(juce::Array<WorkstationAudioEngine::PlaybackClipTarget>& targets,
                                                double& durationSeconds,
                                                juce::String& errorMessage) const
{
    targets.clear();
    durationSeconds = 0.0;

    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.recording || ! clip.file.existsAsFile())
            continue;

        WorkstationAudioEngine::PlaybackClipTarget target;
        target.trackIndex = clip.trackIndex;
        target.file = clip.file;
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
    if (! projectManager.hasStorageRoot())
        return;

    auto state = projectManager.loadProjectState();
    if (! state.isValid())
        return;

    loadAppSettings();
    applySelectedAudioDeviceSettings();

    juce::String errorMessage;
    engine.restoreSessionState(state, errorMessage);

    if (auto timelineState = state.getChildWithName("Timeline"); timelineState.isValid())
        timelineModel.restoreState(timelineState);

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
    if (savedMode > static_cast<int>(WorkspaceMode::settings))
        savedMode = static_cast<int>(WorkspaceMode::tracker);

    setWorkspaceMode(static_cast<WorkspaceMode>(juce::jlimit(0, static_cast<int>(WorkspaceMode::settings), savedMode)));

    refreshInsertRack();
    transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
    refreshRecentTakes();
    refreshFoleyArrangement();
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
        trackerPanel.setTrackInput(index, studioIOModel.getInputIndexForChannel(engine.getTrackInputChannel(index)));
        trackerPanel.setTrackFxSummary(index, engine.getTrackPluginCount(index));
        engine.setTrackRecordingArmed(index, juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index]);
        engine.setTrackMonitoringEnabled(index, juce::isPositiveAndBelow(index, (int) monitoredTracks.size()) && monitoredTracks[(size_t) index]);
        trackerPanel.setTrackLevel(index, engine.getTrackLevel(index));
    }
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
