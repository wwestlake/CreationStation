#include "SignalLabPanel.h"

namespace
{
juce::Colour signalPanelColour() { return juce::Colour(0xff11151c); }
juce::Colour signalCardColour() { return juce::Colour(0xff1a2030); }
juce::Colour signalAccentColour() { return juce::Colour(0xff6fa8ff); }

float clamp01(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float sampleAutomation(const std::array<float, 4>& values, float t)
{
    auto clampedT = clamp01(t);
    auto scaled = clampedT * 3.0f;
    auto leftIndex = juce::jlimit(0, 2, (int) std::floor(scaled));
    auto rightIndex = juce::jlimit(1, 3, leftIndex + 1);
    auto localT = scaled - (float) leftIndex;
    return juce::jmap(localT, values[(size_t) leftIndex], values[(size_t) rightIndex]);
}

float applyBrightnessFilter(float input, float& state, float brightness, double sampleRate)
{
    auto cutoffHz = juce::jmap(juce::jlimit(0.02f, 1.0f, brightness), 180.0f, 12000.0f);
    auto alpha = juce::jlimit(0.001f, 0.99f, (float) (juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
    state += alpha * (input - state);
    return state;
}
}

SignalLabPanel::EnvelopeEditor::EnvelopeEditor()
{
    setRepaintsOnMouseActivity(true);
}

void SignalLabPanel::EnvelopeEditor::setRecipe(const SignalRecipe& newRecipe)
{
    recipe = newRecipe;
    repaint();
}

juce::Rectangle<float> SignalLabPanel::EnvelopeEditor::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(12.0f, 14.0f);
}

juce::Point<float> SignalLabPanel::EnvelopeEditor::toScreen(float normalizedX, float normalizedY) const
{
    auto plot = getPlotArea();
    return { plot.getX() + normalizedX * plot.getWidth(),
             plot.getBottom() - normalizedY * plot.getHeight() };
}

juce::Point<float> SignalLabPanel::EnvelopeEditor::getAttackPoint() const
{
    return toScreen(recipe.attackPosition, 1.0f);
}

juce::Point<float> SignalLabPanel::EnvelopeEditor::getSustainPoint() const
{
    return toScreen(recipe.sustainPosition, recipe.sustainLevel);
}

juce::Point<float> SignalLabPanel::EnvelopeEditor::getReleasePoint() const
{
    return toScreen(recipe.releasePosition, recipe.sustainLevel);
}

void SignalLabPanel::EnvelopeEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto plot = getPlotArea();

    g.setColour(signalCardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    g.setColour(juce::Colour(0xff263140));
    for (int index = 0; index <= 4; ++index)
    {
        auto y = juce::jmap((float) index / 4.0f, plot.getBottom(), plot.getY());
        g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }

    for (int index = 0; index <= 8; ++index)
    {
        auto x = juce::jmap((float) index / 8.0f, plot.getX(), plot.getRight());
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
    }

    juce::Path envelopePath;
    auto start = toScreen(0.0f, 0.0f);
    auto attack = getAttackPoint();
    auto sustain = getSustainPoint();
    auto release = getReleasePoint();
    auto end = toScreen(1.0f, 0.0f);

    envelopePath.startNewSubPath(start);
    envelopePath.lineTo(attack);
    envelopePath.lineTo(sustain);
    envelopePath.lineTo(release);
    envelopePath.lineTo(end);

    g.setColour(signalAccentColour().withAlpha(0.15f));
    juce::Path fillPath(envelopePath);
    fillPath.lineTo(end.x, plot.getBottom());
    fillPath.lineTo(plot.getX(), plot.getBottom());
    fillPath.closeSubPath();
    g.fillPath(fillPath);

    g.setColour(signalAccentColour());
    g.strokePath(envelopePath, juce::PathStrokeType(2.5f));

    auto drawHandle = [&g](juce::Point<float> point, juce::Colour colour)
    {
        g.setColour(colour);
        g.fillEllipse(point.x - 5.5f, point.y - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawEllipse(point.x - 5.5f, point.y - 5.5f, 11.0f, 11.0f, 1.0f);
    };

    drawHandle(attack, juce::Colour(0xff7dd36f));
    drawHandle(sustain, juce::Colour(0xfff2cc60));
    drawHandle(release, juce::Colour(0xffd48a5f));

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("Envelope", getLocalBounds().reduced(12, 6).removeFromTop(18), juce::Justification::centredLeft, false);
}

void SignalLabPanel::EnvelopeEditor::mouseDown(const juce::MouseEvent& event)
{
    auto position = event.position;
    auto attack = getAttackPoint();
    auto sustain = getSustainPoint();
    auto release = getReleasePoint();

    if (position.getDistanceFrom(attack) < 16.0f)
        dragTarget = DragTarget::attack;
    else if (position.getDistanceFrom(sustain) < 16.0f)
        dragTarget = DragTarget::sustain;
    else if (position.getDistanceFrom(release) < 16.0f)
        dragTarget = DragTarget::release;
    else
        dragTarget = DragTarget::none;
}

void SignalLabPanel::EnvelopeEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (dragTarget == DragTarget::none)
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    auto normalizedY = clamp01((plot.getBottom() - event.position.y) / plot.getHeight());

    switch (dragTarget)
    {
        case DragTarget::attack:
            recipe.attackPosition = juce::jlimit(0.02f, recipe.sustainPosition - 0.05f, normalizedX);
            break;
        case DragTarget::sustain:
            recipe.sustainPosition = juce::jlimit(recipe.attackPosition + 0.05f, recipe.releasePosition - 0.05f, normalizedX);
            recipe.sustainLevel = juce::jlimit(0.05f, 1.0f, normalizedY);
            break;
        case DragTarget::release:
            recipe.releasePosition = juce::jlimit(recipe.sustainPosition + 0.05f, 0.98f, normalizedX);
            break;
        case DragTarget::none:
            break;
    }

    if (onEnvelopeChanged)
        onEnvelopeChanged(recipe.attackPosition, recipe.sustainPosition, recipe.releasePosition, recipe.sustainLevel);

    repaint();
}

void SignalLabPanel::ScopePanel::setBuffer(const juce::AudioBuffer<float>& buffer)
{
    displayBuffer = buffer;
    repaint();
}

void SignalLabPanel::ScopePanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(signalCardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    auto plot = getLocalBounds().reduced(12, 16);
    g.setColour(juce::Colour(0xff263140));
    g.drawHorizontalLine(plot.getCentreY(), (float) plot.getX(), (float) plot.getRight());

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("Oscilloscope", plot.removeFromTop(18), juce::Justification::centredLeft, false);

    if (displayBuffer.getNumSamples() <= 0)
        return;

    auto waveformArea = plot.reduced(0, 8);
    juce::Path path;
    auto numSamples = juce::jmin(displayBuffer.getNumSamples(), 2048);
    auto* channelData = displayBuffer.getReadPointer(0);

    for (int index = 0; index < numSamples; ++index)
    {
        auto x = juce::jmap((float) index / (float) juce::jmax(1, numSamples - 1), (float) waveformArea.getX(), (float) waveformArea.getRight());
        auto y = juce::jmap(channelData[index], 1.0f, -1.0f, (float) waveformArea.getY(), (float) waveformArea.getBottom());
        if (index == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(signalAccentColour());
    g.strokePath(path, juce::PathStrokeType(1.75f));
}

void SignalLabPanel::SpectrumPanel::setBuffer(const juce::AudioBuffer<float>& buffer, double)
{
    magnitudes.clearQuick();

    if (buffer.getNumSamples() <= 0)
    {
        repaint();
        return;
    }

    constexpr int fftOrder = 11;
    constexpr int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft(fftOrder);
    juce::HeapBlock<float> fftData(fftSize * 2);
    juce::FloatVectorOperations::clear(fftData.get(), fftSize * 2);

    auto samplesToCopy = juce::jmin(buffer.getNumSamples(), fftSize);
    auto* input = buffer.getReadPointer(0);

    for (int index = 0; index < samplesToCopy; ++index)
        fftData[index] = input[index] * (0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float) index / (float) juce::jmax(1, samplesToCopy - 1)));

    fft.performFrequencyOnlyForwardTransform(fftData.get());

    auto bins = fftSize / 2;
    magnitudes.resize(bins);
    for (int index = 0; index < bins; ++index)
        magnitudes.set(index, juce::Decibels::gainToDecibels(fftData[index] / (float) fftSize, -100.0f));

    repaint();
}

void SignalLabPanel::SpectrumPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(signalCardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    auto plot = getLocalBounds().reduced(12, 16);
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("Frequency Analyzer", plot.removeFromTop(18), juce::Justification::centredLeft, false);

    if (magnitudes.isEmpty())
        return;

    auto spectrumArea = plot.reduced(0, 8);
    juce::Path path;

    for (int index = 1; index < magnitudes.size(); ++index)
    {
        auto normalizedX = std::log10(1.0f + 9.0f * (float) index / (float) juce::jmax(1, magnitudes.size() - 1));
        auto x = juce::jmap(normalizedX, 0.0f, 1.0f, (float) spectrumArea.getX(), (float) spectrumArea.getRight());
        auto y = juce::jmap(magnitudes[index], -100.0f, 0.0f, (float) spectrumArea.getBottom(), (float) spectrumArea.getY());
        if (index == 1)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xff7dd36f));
    g.strokePath(path, juce::PathStrokeType(1.75f));
}

SignalLabPanel::AutomationLaneEditor::AutomationLaneEditor(const juce::String& title, juce::Colour accent, float midline)
    : laneTitle(title), laneAccent(accent), defaultMidline(midline)
{
    values.fill(defaultMidline);
    setRepaintsOnMouseActivity(true);
}

void SignalLabPanel::AutomationLaneEditor::setValues(const std::array<float, 4>& newValues)
{
    values = newValues;
    repaint();
}

juce::Rectangle<float> SignalLabPanel::AutomationLaneEditor::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(12.0f, 14.0f);
}

juce::Point<float> SignalLabPanel::AutomationLaneEditor::getPoint(int index) const
{
    auto plot = getPlotArea();
    auto x = juce::jmap((float) index / 3.0f, plot.getX(), plot.getRight());
    auto y = juce::jmap(values[(size_t) index], 0.0f, 1.0f, plot.getBottom(), plot.getY());
    return { x, y };
}

void SignalLabPanel::AutomationLaneEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto plot = getPlotArea();

    g.setColour(signalCardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    g.setColour(juce::Colour(0xff263140));
    for (int index = 0; index <= 4; ++index)
    {
        auto y = juce::jmap((float) index / 4.0f, plot.getBottom(), plot.getY());
        g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }

    for (int index = 0; index <= 3; ++index)
    {
        auto x = juce::jmap((float) index / 3.0f, plot.getX(), plot.getRight());
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
    }

    juce::Path path;
    for (int index = 0; index < 4; ++index)
    {
        auto point = getPoint(index);
        if (index == 0)
            path.startNewSubPath(point);
        else
            path.lineTo(point);
    }

    g.setColour(laneAccent.withAlpha(0.15f));
    juce::Path fillPath(path);
    fillPath.lineTo(plot.getRight(), plot.getBottom());
    fillPath.lineTo(plot.getX(), plot.getBottom());
    fillPath.closeSubPath();
    g.fillPath(fillPath);

    g.setColour(laneAccent);
    g.strokePath(path, juce::PathStrokeType(2.0f));

    for (int index = 0; index < 4; ++index)
    {
        auto point = getPoint(index);
        g.setColour(laneAccent);
        g.fillEllipse(point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawEllipse(point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f, 1.0f);
    }

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText(laneTitle, getLocalBounds().reduced(12, 6).removeFromTop(18), juce::Justification::centredLeft, false);
}

void SignalLabPanel::AutomationLaneEditor::mouseDown(const juce::MouseEvent& event)
{
    dragIndex = -1;
    auto bestDistance = 18.0f;
    for (int index = 0; index < 4; ++index)
    {
        auto distance = event.position.getDistanceFrom(getPoint(index));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            dragIndex = index;
        }
    }
}

void SignalLabPanel::AutomationLaneEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (dragIndex < 0)
        return;

    auto plot = getPlotArea();
    auto normalizedY = clamp01((plot.getBottom() - event.position.y) / plot.getHeight());
    values[(size_t) dragIndex] = normalizedY;

    if (onValuesChanged)
        onValuesChanged(values);

    repaint();
}

SignalLabPanel::SignalLabPanel()
{
    setName("Signal Lab");
    runtimePlayer.prepare(recipe.sampleRate, 512);

    titleLabel.setText("Signal Lab", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Forge tones from oscillators, noise, and envelope motion, then inspect the waveform and spectrum.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa7b6cb));
    addAndMakeVisible(statusLabel);

    nameLabel.setText("Sound Name", juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    nameEditor.setText(recipe.name, juce::dontSendNotification);
    nameEditor.onTextChange = [this]
    {
        if (suppressCallbacks)
            return;

        recipe.name = nameEditor.getText().trim();
        updateStatusText();
    };
    addAndMakeVisible(nameEditor);

    templateLabel.setText("Template", juce::dontSendNotification);
    templateLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(templateLabel);

    templateSelector.addItem("Custom", 1);
    templateSelector.addItem("Soft Keys", 2);
    templateSelector.addItem("Triangle Lead", 3);
    templateSelector.addItem("Noisy Pluck", 4);
    templateSelector.addItem("Drone Pad", 5);
    templateSelector.addItem("Impact Tone", 6);
    templateSelector.setSelectedId(1, juce::dontSendNotification);
    templateSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;

        auto selected = templateSelector.getText();
        if (selected == "Custom" || selected.isEmpty())
            return;

        applyTemplate(selected);
    };
    addAndMakeVisible(templateSelector);

    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);
    };

    setupLabel(frequencyLabel, "Base Frequency");
    setupLabel(durationLabel, "Duration");
    setupLabel(pitchLabel, "Pitch Sweep");
    setupLabel(brightnessLabel, "Brightness");
    setupLabel(sineLabel, "Sine");
    setupLabel(sawLabel, "Saw");
    setupLabel(squareLabel, "Square");
    setupLabel(triangleLabel, "Triangle");
    setupLabel(noiseLabel, "Noise");

    configureSlider(frequencySlider, 30.0, 2400.0, 1.0);
    configureSlider(durationSlider, 0.1, 6.0, 0.01);
    configureSlider(pitchSlider, -24.0, 24.0, 0.1);
    configureSlider(brightnessSlider, 0.0, 1.0, 0.001);
    configureSlider(sineSlider, 0.0, 1.0, 0.001);
    configureSlider(sawSlider, 0.0, 1.0, 0.001);
    configureSlider(squareSlider, 0.0, 1.0, 0.001);
    configureSlider(triangleSlider, 0.0, 1.0, 0.001);
    configureSlider(noiseSlider, 0.0, 1.0, 0.001);

    frequencySlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.baseFrequencyHz = (float) frequencySlider.getValue(); regenerateSignal(); } };
    durationSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.durationSeconds = durationSlider.getValue(); regenerateSignal(); } };
    pitchSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.pitchSweepSemitones = (float) pitchSlider.getValue(); regenerateSignal(); } };
    brightnessSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.brightness = (float) brightnessSlider.getValue(); regenerateSignal(); } };
    sineSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.sineLevel = (float) sineSlider.getValue(); regenerateSignal(); } };
    sawSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.sawLevel = (float) sawSlider.getValue(); regenerateSignal(); } };
    squareSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.squareLevel = (float) squareSlider.getValue(); regenerateSignal(); } };
    triangleSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.triangleLevel = (float) triangleSlider.getValue(); regenerateSignal(); } };
    noiseSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.noiseLevel = (float) noiseSlider.getValue(); regenerateSignal(); } };

    envelopeEditor.onEnvelopeChanged = [this](float attackPosition, float sustainPosition, float releasePosition, float sustainLevel)
    {
        recipe.attackPosition = attackPosition;
        recipe.sustainPosition = sustainPosition;
        recipe.releasePosition = releasePosition;
        recipe.sustainLevel = sustainLevel;
        regenerateSignal();
    };

    pitchAutomationEditor.onValuesChanged = [this](const std::array<float, 4>& values)
    {
        recipe.pitchAutomation = values;
        regenerateSignal();
    };

    gainAutomationEditor.onValuesChanged = [this](const std::array<float, 4>& values)
    {
        recipe.gainAutomation = values;
        regenerateSignal();
    };

    previewButton.onClick = [this]
    {
        if (onPreviewRequested && generatedBuffer.getNumSamples() > 0)
            onPreviewRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    };
    addAndMakeVisible(previewButton);

    renderButton.onClick = [this]
    {
        if (onRenderRequested && generatedBuffer.getNumSamples() > 0)
            onRenderRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    };
    addAndMakeVisible(renderButton);

    exportPatchButton.onClick = [this]
    {
        if (onPatchExportRequested == nullptr)
            return;

        auto patchDocument = buildPatchDocument(recipe);
        onPatchExportRequested(cw::serialisePatchDocumentJson(patchDocument), recipe.name);
    };
    addAndMakeVisible(exportPatchButton);

    savePatchButton.onClick = [this]
    {
        if (onPatchSaveToLibraryRequested == nullptr)
            return;

        auto patchDocument = buildPatchDocument(recipe);
        onPatchSaveToLibraryRequested(cw::serialisePatchDocumentJson(patchDocument), recipe.name);
    };
    addAndMakeVisible(savePatchButton);

    loadPatchButton.onClick = [this]
    {
        if (onPatchLoadRequested)
            onPatchLoadRequested();
    };
    addAndMakeVisible(loadPatchButton);

    addAndMakeVisible(envelopeEditor);
    addAndMakeVisible(pitchAutomationEditor);
    addAndMakeVisible(gainAutomationEditor);
    addAndMakeVisible(scopePanel);
    addAndMakeVisible(spectrumPanel);

    refreshControlsFromRecipe();
    regenerateSignal();
}

juce::ValueTree SignalLabPanel::createState() const
{
    juce::ValueTree state("SignalLab");
    state.setProperty("name", recipe.name, nullptr);
    state.setProperty("sampleRate", recipe.sampleRate, nullptr);
    state.setProperty("durationSeconds", recipe.durationSeconds, nullptr);
    state.setProperty("baseFrequencyHz", recipe.baseFrequencyHz, nullptr);
    state.setProperty("brightness", recipe.brightness, nullptr);
    state.setProperty("sineLevel", recipe.sineLevel, nullptr);
    state.setProperty("sawLevel", recipe.sawLevel, nullptr);
    state.setProperty("squareLevel", recipe.squareLevel, nullptr);
    state.setProperty("triangleLevel", recipe.triangleLevel, nullptr);
    state.setProperty("noiseLevel", recipe.noiseLevel, nullptr);
    state.setProperty("pitchSweepSemitones", recipe.pitchSweepSemitones, nullptr);
    state.setProperty("attackPosition", recipe.attackPosition, nullptr);
    state.setProperty("sustainPosition", recipe.sustainPosition, nullptr);
    state.setProperty("releasePosition", recipe.releasePosition, nullptr);
    state.setProperty("sustainLevel", recipe.sustainLevel, nullptr);
    for (int index = 0; index < 4; ++index)
    {
        state.setProperty("pitchAutomation" + juce::String(index), recipe.pitchAutomation[(size_t) index], nullptr);
        state.setProperty("gainAutomation" + juce::String(index), recipe.gainAutomation[(size_t) index], nullptr);
    }
    return state;
}

void SignalLabPanel::restoreState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    recipe.name = state.getProperty("name", recipe.name).toString();
    recipe.sampleRate = (double) state.getProperty("sampleRate", recipe.sampleRate);
    recipe.durationSeconds = (double) state.getProperty("durationSeconds", recipe.durationSeconds);
    recipe.baseFrequencyHz = (float) state.getProperty("baseFrequencyHz", recipe.baseFrequencyHz);
    recipe.brightness = (float) state.getProperty("brightness", recipe.brightness);
    recipe.sineLevel = (float) state.getProperty("sineLevel", recipe.sineLevel);
    recipe.sawLevel = (float) state.getProperty("sawLevel", recipe.sawLevel);
    recipe.squareLevel = (float) state.getProperty("squareLevel", recipe.squareLevel);
    recipe.triangleLevel = (float) state.getProperty("triangleLevel", recipe.triangleLevel);
    recipe.noiseLevel = (float) state.getProperty("noiseLevel", recipe.noiseLevel);
    recipe.pitchSweepSemitones = (float) state.getProperty("pitchSweepSemitones", recipe.pitchSweepSemitones);
    recipe.attackPosition = (float) state.getProperty("attackPosition", recipe.attackPosition);
    recipe.sustainPosition = (float) state.getProperty("sustainPosition", recipe.sustainPosition);
    recipe.releasePosition = (float) state.getProperty("releasePosition", recipe.releasePosition);
    recipe.sustainLevel = (float) state.getProperty("sustainLevel", recipe.sustainLevel);
    for (int index = 0; index < 4; ++index)
    {
        recipe.pitchAutomation[(size_t) index] = (float) state.getProperty("pitchAutomation" + juce::String(index), recipe.pitchAutomation[(size_t) index]);
        recipe.gainAutomation[(size_t) index] = (float) state.getProperty("gainAutomation" + juce::String(index), recipe.gainAutomation[(size_t) index]);
    }

    refreshControlsFromRecipe();
    regenerateSignal();
}

bool SignalLabPanel::loadPatchDocument(const cw::PatchDocument& document, juce::String& errorMessage)
{
    if (document.type != "instrument")
    {
        errorMessage = "Signal Lab can only load instrument patches right now.";
        return false;
    }

    recipe = {};
    recipe.name = document.name.isNotEmpty() ? document.name : recipe.name;

    for (const auto& parameter : document.parameters)
    {
        if (parameter.id == "baseFrequency")
            recipe.baseFrequencyHz = (float) parameter.defaultValue;
        else if (parameter.id == "brightness")
            recipe.brightness = (float) parameter.defaultValue;
        else if (parameter.id == "noiseLevel")
            recipe.noiseLevel = (float) parameter.defaultValue;
    }

    recipe.sineLevel = 0.0f;
    recipe.sawLevel = 0.0f;
    recipe.squareLevel = 0.0f;
    recipe.triangleLevel = 0.0f;

    for (const auto& source : document.sources)
    {
        if (source.kind == "oscillator")
        {
            if (source.waveform == "sine")
                recipe.sineLevel = (float) source.level;
            else if (source.waveform == "saw")
                recipe.sawLevel = (float) source.level;
            else if (source.waveform == "square")
                recipe.squareLevel = (float) source.level;
            else if (source.waveform == "triangle")
                recipe.triangleLevel = (float) source.level;
        }
        else if (source.kind == "noise")
        {
            recipe.noiseLevel = (float) source.level;
        }
    }

    for (const auto& node : document.nodes)
    {
        if (node.kind == "envelope")
        {
            recipe.attackPosition = (float) node.properties.getWithDefault("attackPosition", recipe.attackPosition);
            recipe.sustainPosition = (float) node.properties.getWithDefault("sustainPosition", recipe.sustainPosition);
            recipe.releasePosition = (float) node.properties.getWithDefault("releasePosition", recipe.releasePosition);
            recipe.sustainLevel = (float) node.properties.getWithDefault("sustainLevel", recipe.sustainLevel);
        }
    }

    auto fillAutomation = [](std::array<float, 4>& target, const cw::PatchAutomationLane& lane)
    {
        target.fill(0.5f);
        for (int index = 0; index < juce::jmin(4, lane.points.size()); ++index)
            target[(size_t) index] = (float) lane.points.getReference(index).value;
    };

    recipe.pitchAutomation = { 0.5f, 0.5f, 0.5f, 0.5f };
    recipe.gainAutomation = { 1.0f, 0.85f, 0.7f, 0.0f };

    for (const auto& lane : document.automationLanes)
    {
        if (lane.id == "pitchMotion" || lane.targetParameter == "pitchOffsetSemitones")
            fillAutomation(recipe.pitchAutomation, lane);
        else if (lane.id == "gainMotion" || lane.targetParameter == "outputGain")
            fillAutomation(recipe.gainAutomation, lane);
    }

    refreshControlsFromRecipe();
    regenerateSignal();
    return true;
}

void SignalLabPanel::applyAiTemplate(const juce::String& templateName)
{
    auto selected = templateName.trim();
    if (selected.isEmpty())
        return;

    if (selected == "Custom")
        return;

    applyTemplate(selected);
}

bool SignalLabPanel::previewCurrentSignal()
{
    if (generatedBuffer.getNumSamples() <= 0 || onPreviewRequested == nullptr)
        return false;

    onPreviewRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    return true;
}

void SignalLabPanel::paint(juce::Graphics& g)
{
    g.fillAll(signalPanelColour());
}

void SignalLabPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    subtitleLabel.setBounds(area.removeFromTop(22));
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    auto topBar = area.removeFromTop(30);
    nameLabel.setBounds(topBar.removeFromLeft(100));
    nameEditor.setBounds(topBar.removeFromLeft(190));
    topBar.removeFromLeft(10);
    templateLabel.setBounds(topBar.removeFromLeft(70));
    templateSelector.setBounds(topBar.removeFromLeft(150));
    topBar.removeFromLeft(10);
    previewButton.setBounds(topBar.removeFromLeft(130));
    topBar.removeFromLeft(10);
    renderButton.setBounds(topBar.removeFromLeft(140));
    topBar.removeFromLeft(10);
    exportPatchButton.setBounds(topBar.removeFromLeft(120));
    topBar.removeFromLeft(10);
    savePatchButton.setBounds(topBar.removeFromLeft(130));
    topBar.removeFromLeft(10);
    loadPatchButton.setBounds(topBar.removeFromLeft(110));

    area.removeFromTop(10);

    auto controlArea = area.removeFromLeft(320);
    auto addRow = [&](juce::Label& label, juce::Slider& slider)
    {
        label.setBounds(controlArea.removeFromTop(20));
        slider.setBounds(controlArea.removeFromTop(38));
        controlArea.removeFromTop(6);
    };

    addRow(frequencyLabel, frequencySlider);
    addRow(durationLabel, durationSlider);
    addRow(pitchLabel, pitchSlider);
    addRow(brightnessLabel, brightnessSlider);
    addRow(sineLabel, sineSlider);
    addRow(sawLabel, sawSlider);
    addRow(squareLabel, squareSlider);
    addRow(triangleLabel, triangleSlider);
    addRow(noiseLabel, noiseSlider);

    area.removeFromLeft(12);
    auto upperVisuals = area.removeFromTop(200);
    envelopeEditor.setBounds(upperVisuals.removeFromLeft(area.getWidth() / 2));
    upperVisuals.removeFromLeft(12);
    scopePanel.setBounds(upperVisuals);
    area.removeFromTop(12);
    auto automationArea = area.removeFromTop(220);
    pitchAutomationEditor.setBounds(automationArea.removeFromTop(104));
    automationArea.removeFromTop(12);
    gainAutomationEditor.setBounds(automationArea.removeFromTop(104));
    area.removeFromTop(12);
    spectrumPanel.setBounds(area);
}

void SignalLabPanel::configureSlider(juce::Slider& slider, double min, double max, double step)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 22);
    slider.setRange(min, max, step);
    addAndMakeVisible(slider);
}

void SignalLabPanel::regenerateSignal()
{
    auto patchDocument = buildPatchDocument(recipe);
    juce::String errorMessage;
    if (! runtimePlayer.renderPatchToBuffer(patchDocument, recipe.durationSeconds, generatedBuffer, errorMessage))
        generatedBuffer = buildSignalBuffer(recipe);

    envelopeEditor.setRecipe(recipe);
    pitchAutomationEditor.setValues(recipe.pitchAutomation);
    gainAutomationEditor.setValues(recipe.gainAutomation);
    scopePanel.setBuffer(generatedBuffer);
    spectrumPanel.setBuffer(generatedBuffer, recipe.sampleRate);
    updateStatusText();
}

juce::AudioBuffer<float> SignalLabPanel::buildSignalBuffer(const SignalRecipe& activeRecipe) const
{
    auto numSamples = juce::jmax(1, juce::roundToInt(activeRecipe.durationSeconds * activeRecipe.sampleRate));
    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.clear();

    juce::Random random(0x5349474E);
    auto totalLevel = activeRecipe.sineLevel + activeRecipe.sawLevel + activeRecipe.squareLevel + activeRecipe.triangleLevel + activeRecipe.noiseLevel;
    auto normalizer = totalLevel > 0.0f ? (0.9f / totalLevel) : 0.0f;
    auto releaseStart = juce::jmax(activeRecipe.sustainPosition + 0.01f, activeRecipe.releasePosition);
    float filterState = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
        auto pitchAutomationValue = sampleAutomation(activeRecipe.pitchAutomation, t);
        auto gainAutomationValue = sampleAutomation(activeRecipe.gainAutomation, t);
        auto pitchSemitones = activeRecipe.pitchSweepSemitones * (t - 0.5f) * 2.0f
                            + juce::jmap(pitchAutomationValue, 0.0f, 1.0f, -12.0f, 12.0f);
        auto frequency = activeRecipe.baseFrequencyHz * std::pow(2.0f, pitchSemitones / 12.0f);
        auto phase = juce::MathConstants<float>::twoPi * frequency * ((float) sample / (float) activeRecipe.sampleRate);

        auto sine = std::sin(phase);
        auto saw = 2.0f * (phase / juce::MathConstants<float>::twoPi - std::floor(0.5f + phase / juce::MathConstants<float>::twoPi));
        auto square = std::sin(phase) >= 0.0f ? 1.0f : -1.0f;
        auto triangle = std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
        auto noise = random.nextFloat() * 2.0f - 1.0f;

        float envelope = 0.0f;
        if (t <= activeRecipe.attackPosition)
            envelope = juce::jmap(t, 0.0f, juce::jmax(0.001f, activeRecipe.attackPosition), 0.0f, 1.0f);
        else if (t <= activeRecipe.sustainPosition)
            envelope = juce::jmap(t, activeRecipe.attackPosition, juce::jmax(activeRecipe.attackPosition + 0.001f, activeRecipe.sustainPosition), 1.0f, activeRecipe.sustainLevel);
        else if (t <= releaseStart)
            envelope = activeRecipe.sustainLevel;
        else
            envelope = juce::jmap(t, releaseStart, 1.0f, activeRecipe.sustainLevel, 0.0f);

        auto sampleValue = normalizer * envelope * gainAutomationValue
                         * (activeRecipe.sineLevel * sine
                            + activeRecipe.sawLevel * saw
                            + activeRecipe.squareLevel * square
                            + activeRecipe.triangleLevel * triangle
                            + activeRecipe.noiseLevel * noise);

        sampleValue = applyBrightnessFilter(sampleValue, filterState, activeRecipe.brightness, activeRecipe.sampleRate);

        buffer.setSample(0, sample, sampleValue);
        buffer.setSample(1, sample, sampleValue);
    }

    return buffer;
}

cw::PatchDocument SignalLabPanel::buildPatchDocument(const SignalRecipe& activeRecipe) const
{
    cw::PatchDocument document;
    document.patchId = cw::makePatchId(activeRecipe.name);
    document.name = activeRecipe.name;
    document.type = "instrument";
    document.description = "Signal Lab exported instrument patch.";
    document.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    document.updatedAt = document.createdAt;

    document.parameters.add({ "baseFrequency", "Base Frequency", "float", activeRecipe.baseFrequencyHz, 30.0, 2400.0, "hz" });
    document.parameters.add({ "brightness", "Brightness", "float", activeRecipe.brightness, 0.0, 1.0, "normalized" });
    document.parameters.add({ "pitchOffsetSemitones", "Pitch Offset", "float", 0.0, -12.0, 12.0, "semitones" });
    document.parameters.add({ "outputGain", "Output Gain", "float", 1.0, 0.0, 1.0, "normalized" });
    document.parameters.add({ "noiseLevel", "Noise Level", "float", activeRecipe.noiseLevel, 0.0, 1.0, "normalized" });

    auto makeLane = [](const juce::String& id,
                       const juce::String& name,
                       const juce::String& target,
                       double rangeMin,
                       double rangeMax,
                       const std::array<float, 4>& values)
    {
        cw::PatchAutomationLane lane;
        lane.id = id;
        lane.name = name;
        lane.targetParameter = target;
        lane.rangeMin = rangeMin;
        lane.rangeMax = rangeMax;
        for (int index = 0; index < 4; ++index)
            lane.points.add({ index / 3.0, values[(size_t) index] });
        return lane;
    };

    document.automationLanes.add(makeLane("pitchMotion", "Pitch Motion", "pitchOffsetSemitones", -12.0, 12.0, activeRecipe.pitchAutomation));
    document.automationLanes.add(makeLane("gainMotion", "Gain Motion", "outputGain", 0.0, 1.0, activeRecipe.gainAutomation));

    if (activeRecipe.sineLevel > 0.0f)
        document.sources.add({ "osc1", "oscillator", "sine", {}, activeRecipe.sineLevel, "baseFrequency" });
    if (activeRecipe.sawLevel > 0.0f)
        document.sources.add({ "osc2", "oscillator", "saw", {}, activeRecipe.sawLevel, "baseFrequency" });
    if (activeRecipe.squareLevel > 0.0f)
        document.sources.add({ "osc3", "oscillator", "square", {}, activeRecipe.squareLevel, "baseFrequency" });
    if (activeRecipe.triangleLevel > 0.0f)
        document.sources.add({ "osc4", "oscillator", "triangle", {}, activeRecipe.triangleLevel, "baseFrequency" });
    if (activeRecipe.noiseLevel > 0.0f)
        document.sources.add({ "noise1", "noise", {}, "white", activeRecipe.noiseLevel, {} });

    cw::PatchNode mixNode;
    mixNode.id = "mix1";
    mixNode.kind = "mix";
    document.nodes.add(mixNode);

    cw::PatchNode envelopeNode;
    envelopeNode.id = "env1";
    envelopeNode.kind = "envelope";
    envelopeNode.properties.set("attackPosition", activeRecipe.attackPosition);
    envelopeNode.properties.set("sustainPosition", activeRecipe.sustainPosition);
    envelopeNode.properties.set("releasePosition", activeRecipe.releasePosition);
    envelopeNode.properties.set("sustainLevel", activeRecipe.sustainLevel);
    document.nodes.add(envelopeNode);

    for (const auto& source : document.sources)
        document.connections.add({ source.id, "mix1" });
    document.connections.add({ "mix1", "env1" });

    document.output.channelMode = "stereo";
    document.output.gain = 0.9;
    document.output.pan = 0.0;

    return document;
}

void SignalLabPanel::applyTemplate(const juce::String& templateName)
{
    if (templateName == "Soft Keys")
    {
        recipe.name = "Soft Keys";
        recipe.durationSeconds = 1.8;
        recipe.baseFrequencyHz = 220.0f;
        recipe.brightness = 0.34f;
        recipe.sineLevel = 0.72f;
        recipe.sawLevel = 0.0f;
        recipe.squareLevel = 0.0f;
        recipe.triangleLevel = 0.28f;
        recipe.noiseLevel = 0.02f;
        recipe.pitchSweepSemitones = 0.0f;
        recipe.attackPosition = 0.10f;
        recipe.sustainPosition = 0.38f;
        recipe.releasePosition = 0.86f;
        recipe.sustainLevel = 0.70f;
        recipe.pitchAutomation = { 0.5f, 0.5f, 0.5f, 0.5f };
        recipe.gainAutomation = { 1.0f, 0.88f, 0.72f, 0.0f };
    }
    else if (templateName == "Triangle Lead")
    {
        recipe.name = "Triangle Lead";
        recipe.durationSeconds = 1.2;
        recipe.baseFrequencyHz = 330.0f;
        recipe.brightness = 0.72f;
        recipe.sineLevel = 0.20f;
        recipe.sawLevel = 0.12f;
        recipe.squareLevel = 0.0f;
        recipe.triangleLevel = 0.68f;
        recipe.noiseLevel = 0.0f;
        recipe.pitchSweepSemitones = 1.5f;
        recipe.attackPosition = 0.04f;
        recipe.sustainPosition = 0.22f;
        recipe.releasePosition = 0.78f;
        recipe.sustainLevel = 0.62f;
        recipe.pitchAutomation = { 0.55f, 0.50f, 0.48f, 0.50f };
        recipe.gainAutomation = { 1.0f, 0.92f, 0.76f, 0.0f };
    }
    else if (templateName == "Noisy Pluck")
    {
        recipe.name = "Noisy Pluck";
        recipe.durationSeconds = 0.9;
        recipe.baseFrequencyHz = 196.0f;
        recipe.brightness = 0.88f;
        recipe.sineLevel = 0.25f;
        recipe.sawLevel = 0.25f;
        recipe.squareLevel = 0.10f;
        recipe.triangleLevel = 0.20f;
        recipe.noiseLevel = 0.20f;
        recipe.pitchSweepSemitones = -4.0f;
        recipe.attackPosition = 0.02f;
        recipe.sustainPosition = 0.12f;
        recipe.releasePosition = 0.52f;
        recipe.sustainLevel = 0.20f;
        recipe.pitchAutomation = { 0.78f, 0.58f, 0.46f, 0.50f };
        recipe.gainAutomation = { 1.0f, 0.58f, 0.22f, 0.0f };
    }
    else if (templateName == "Drone Pad")
    {
        recipe.name = "Drone Pad";
        recipe.durationSeconds = 3.2;
        recipe.baseFrequencyHz = 110.0f;
        recipe.brightness = 0.42f;
        recipe.sineLevel = 0.44f;
        recipe.sawLevel = 0.18f;
        recipe.squareLevel = 0.06f;
        recipe.triangleLevel = 0.26f;
        recipe.noiseLevel = 0.06f;
        recipe.pitchSweepSemitones = 2.0f;
        recipe.attackPosition = 0.18f;
        recipe.sustainPosition = 0.46f;
        recipe.releasePosition = 0.92f;
        recipe.sustainLevel = 0.78f;
        recipe.pitchAutomation = { 0.48f, 0.52f, 0.46f, 0.50f };
        recipe.gainAutomation = { 0.84f, 1.0f, 0.92f, 0.0f };
    }
    else if (templateName == "Impact Tone")
    {
        recipe.name = "Impact Tone";
        recipe.durationSeconds = 1.1;
        recipe.baseFrequencyHz = 90.0f;
        recipe.brightness = 0.93f;
        recipe.sineLevel = 0.36f;
        recipe.sawLevel = 0.18f;
        recipe.squareLevel = 0.14f;
        recipe.triangleLevel = 0.12f;
        recipe.noiseLevel = 0.20f;
        recipe.pitchSweepSemitones = -8.0f;
        recipe.attackPosition = 0.01f;
        recipe.sustainPosition = 0.10f;
        recipe.releasePosition = 0.48f;
        recipe.sustainLevel = 0.18f;
        recipe.pitchAutomation = { 0.92f, 0.64f, 0.42f, 0.50f };
        recipe.gainAutomation = { 1.0f, 0.52f, 0.18f, 0.0f };
    }

    refreshControlsFromRecipe();
    regenerateSignal();
}

void SignalLabPanel::refreshControlsFromRecipe()
{
    suppressCallbacks = true;
    nameEditor.setText(recipe.name, juce::dontSendNotification);
    if (templateSelector.getText() != recipe.name)
        templateSelector.setSelectedId(1, juce::dontSendNotification);
    frequencySlider.setValue(recipe.baseFrequencyHz, juce::dontSendNotification);
    durationSlider.setValue(recipe.durationSeconds, juce::dontSendNotification);
    pitchSlider.setValue(recipe.pitchSweepSemitones, juce::dontSendNotification);
    brightnessSlider.setValue(recipe.brightness, juce::dontSendNotification);
    sineSlider.setValue(recipe.sineLevel, juce::dontSendNotification);
    sawSlider.setValue(recipe.sawLevel, juce::dontSendNotification);
    squareSlider.setValue(recipe.squareLevel, juce::dontSendNotification);
    triangleSlider.setValue(recipe.triangleLevel, juce::dontSendNotification);
    noiseSlider.setValue(recipe.noiseLevel, juce::dontSendNotification);
    suppressCallbacks = false;
    envelopeEditor.setRecipe(recipe);
    pitchAutomationEditor.setValues(recipe.pitchAutomation);
    gainAutomationEditor.setValues(recipe.gainAutomation);
}

void SignalLabPanel::updateStatusText()
{
    auto sampleCount = generatedBuffer.getNumSamples();
    auto text = "Ready: " + recipe.name
              + "  |  " + juce::String(recipe.durationSeconds, 2) + " s"
              + "  |  " + juce::String((int) recipe.baseFrequencyHz) + " Hz"
              + "  |  " + juce::String(sampleCount) + " samples"
              + "  |  automation: pitch + gain";
    statusLabel.setText(text, juce::dontSendNotification);
}
