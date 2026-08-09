#include "SignalLabPanel.h"

namespace
{
class SignalLabNodeWindow final : public juce::DocumentWindow
{
public:
    SignalLabNodeWindow(const juce::String& title, std::function<void()> onClose)
        : juce::DocumentWindow(title, juce::Colour(0xff11151c), juce::DocumentWindow::allButtons),
          onCloseRequested(std::move(onClose))
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
    }

    void closeButtonPressed() override
    {
        if (onCloseRequested)
            onCloseRequested();
    }

private:
    std::function<void()> onCloseRequested;
};

class SimpleNodeEditorContent final : public juce::Component
{
public:
    void addTextBlock(const juce::String& title, const juce::String& body)
    {
        auto* titleLabel = labels.add(new juce::Label());
        titleLabel->setText(title, juce::dontSendNotification);
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel->setFont(juce::Font(15.0f).boldened());
        addAndMakeVisible(titleLabel);

        auto* bodyLabel = labels.add(new juce::Label());
        bodyLabel->setText(body, juce::dontSendNotification);
        bodyLabel->setColour(juce::Label::textColourId, juce::Colour(0xffb8c5d8));
        bodyLabel->setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(bodyLabel);

        rows.add({ titleLabel, 24 });
        rows.add({ bodyLabel, 54 });
    }

    juce::Slider& addSliderRow(const juce::String& labelText, double min, double max, double step)
    {
        auto* label = labels.add(new juce::Label());
        label->setText(labelText, juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);

        auto* slider = sliders.add(new juce::Slider());
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 22);
        slider->setRange(min, max, step);
        addAndMakeVisible(slider);

        rows.add({ label, 20 });
        rows.add({ slider, 34 });
        return *slider;
    }

    juce::ComboBox& addComboRow(const juce::String& labelText, std::initializer_list<juce::String> items)
    {
        auto* label = labels.add(new juce::Label());
        label->setText(labelText, juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);

        auto* combo = combos.add(new juce::ComboBox());
        int id = 1;
        for (const auto& item : items)
            combo->addItem(item, id++);
        addAndMakeVisible(combo);

        rows.add({ label, 20 });
        rows.add({ combo, 28 });
        return *combo;
    }

    // Takes ownership of a heap-allocated component -- every call site hands
    // this a freshly-`new`'d component with nothing else holding a pointer
    // to it, so it must be owned here or it leaks for the life of the app
    // every time a node tool window is opened.
    void addCustomComponent(juce::Component* component, int height)
    {
        addAndMakeVisible(component);
        rows.add({ component, height });
        ownedCustomComponents.add(component);
    }

    juce::TextButton& addButtonRow(const juce::String& buttonText)
    {
        auto* button = buttons.add(new juce::TextButton(buttonText));
        addAndMakeVisible(button);
        rows.add({ button, 28 });
        return *button;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        for (auto& row : rows)
        {
            row.component->setBounds(area.removeFromTop(row.height));
            area.removeFromTop(8);
        }
    }

private:
    struct Row { juce::Component* component; int height; };
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<juce::Component> ownedCustomComponents;
    juce::Array<Row> rows;
};

struct CompileErrorRow
{
    juce::String nodeId;
    juce::String message;
};

class CompileErrorListContent final : public juce::Component
{
public:
    void setErrors(const juce::Array<CompileErrorRow>& newErrors, std::function<void(const juce::String&)> onErrorClickedIn)
    {
        onErrorClicked = std::move(onErrorClickedIn);
        errorButtons.clear();

        if (newErrors.isEmpty())
        {
            if (okLabel == nullptr)
            {
                okLabel = std::make_unique<juce::Label>();
                okLabel->setColour(juce::Label::textColourId, juce::Colour(0xff8fd978));
                okLabel->setFont(juce::Font(14.0f).boldened());
                addAndMakeVisible(*okLabel);
            }
            okLabel->setText("No problems found -- the graph is ready to play.", juce::dontSendNotification);
            okLabel->setVisible(true);
        }
        else if (okLabel != nullptr)
        {
            okLabel->setVisible(false);
        }

        for (auto& error : newErrors)
        {
            auto* button = errorButtons.add(new juce::TextButton());
            button->setButtonText(error.message);
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a1a1e));
            button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff9a9a));
            auto nodeId = error.nodeId;
            button->onClick = [this, nodeId]
            {
                if (onErrorClicked)
                    onErrorClicked(nodeId);
            };
            addAndMakeVisible(button);
        }

        resized();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        if (okLabel != nullptr && okLabel->isVisible())
            okLabel->setBounds(area.removeFromTop(24));

        for (auto* button : errorButtons)
        {
            button->setBounds(area.removeFromTop(30));
            area.removeFromTop(6);
        }
    }

private:
    std::unique_ptr<juce::Label> okLabel;
    juce::OwnedArray<juce::TextButton> errorButtons;
    std::function<void(const juce::String&)> onErrorClicked;
};

// A real mixing-board look for the Mixer node's tool window: one vertical
// fader per channel instead of the horizontal label/slider rows every other
// node's editor uses. A channel whose weight port has a Get-variable wired
// into it is shown at the wired value but disabled -- manual control is
// disabled while wired, same rule as every other parameter in this app.
class MixerFaderBank final : public juce::Component
{
public:
    struct ChannelState
    {
        juce::String label;
        float value = 1.0f;
        bool wired = false;
        double wiredValue = 0.0;
    };

    void setChannels(const juce::Array<ChannelState>& channels, std::function<void(int channelIndex, float newValue)> onChannelChangedIn)
    {
        onChannelChanged = std::move(onChannelChangedIn);
        sliders.clear();
        labels.clear();

        for (int index = 0; index < channels.size(); ++index)
        {
            auto& state = channels.getReference(index);

            auto* slider = sliders.add(new juce::Slider());
            slider->setSliderStyle(juce::Slider::LinearVertical);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 20);
            slider->setRange(0.0, 1.5, 0.001);
            slider->setValue(state.wired ? state.wiredValue : (double) state.value, juce::dontSendNotification);
            slider->setEnabled(! state.wired);
            if (! state.wired)
            {
                auto channelIndex = index;
                slider->onValueChange = [this, channelIndex, slider = slider]
                {
                    if (onChannelChanged)
                        onChannelChanged(channelIndex, (float) slider->getValue());
                };
            }
            addAndMakeVisible(slider);

            auto* label = labels.add(new juce::Label());
            label->setText(state.label, juce::dontSendNotification);
            label->setJustificationType(juce::Justification::centred);
            label->setColour(juce::Label::textColourId, state.wired ? juce::Colour(0xff8fd978) : juce::Colours::white);
            label->setFont(juce::Font(11.0f));
            addAndMakeVisible(label);
        }

        resized();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        if (sliders.isEmpty())
            return;

        auto channelWidth = juce::jmax(44, area.getWidth() / sliders.size());
        for (int index = 0; index < sliders.size(); ++index)
        {
            auto column = area.removeFromLeft(channelWidth);
            labels[index]->setBounds(column.removeFromBottom(18));
            sliders[index]->setBounds(column.reduced(6, 0));
        }
    }

private:
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    std::function<void(int, float)> onChannelChanged;
};

struct AutomationTargetSpec
{
    const char* parameterId;
    const char* title;
    double rangeMin;
    double rangeMax;
    double defaultValue;
    juce::Colour accent;
};

juce::Colour signalPanelColour() { return juce::Colour(0xff11151c); }
juce::Colour signalCardColour() { return juce::Colour(0xff1a2030); }
juce::Colour signalAccentColour() { return juce::Colour(0xff6fa8ff); }
juce::Colour signalSourceColour() { return juce::Colour(0xff67d8ff); }
juce::Colour signalMixColour() { return juce::Colour(0xff8fd978); }
juce::Colour signalFilterColour() { return juce::Colour(0xffffbd66); }
juce::Colour signalEnvelopeColour() { return juce::Colour(0xffc78bff); }
juce::Colour signalOutputColour() { return juce::Colour(0xff7ea6ff); }
constexpr float kMinFilterCutoffHz = 40.0f;
constexpr float kMaxFilterCutoffHz = 16000.0f;

const std::array<AutomationTargetSpec, 16>& getAutomationTargetSpecs()
{
    static const std::array<AutomationTargetSpec, 16> specs
    {{
        { "pitchOffsetSemitones", "Pitch Motion", -12.0, 12.0, 0.0, juce::Colour(0xffb37df0) },
        { "outputGain", "Gain Motion", 0.0, 1.0, 1.0, juce::Colour(0xff7dd36f) },
        { "filterCutoff", "Filter Motion", 0.0, 1.0, 0.5, juce::Colour(0xffffad5a) },
        { "filterResonance", "Resonance Motion", 0.30, 8.0, 0.90, juce::Colour(0xff5ad1ff) },
        { "filterEnvelopeAmount", "Filter Envelope", -1.0, 1.0, 0.35, juce::Colour(0xfff5c06a) },
        { "noiseLevel", "Noise Motion", 0.0, 1.0, 0.10, juce::Colour(0xffff7aa2) },
        { "baseFrequency", "Base Frequency", 30.0, 2400.0, 180.0, juce::Colour(0xff8ee58f) },
        { "sineLevel", "Sine Level", 0.0, 1.0, 0.65, juce::Colour(0xff75d6ff) },
        { "sawLevel", "Saw Level", 0.0, 1.0, 0.15, juce::Colour(0xffff9a5a) },
        { "squareLevel", "Square Level", 0.0, 1.0, 0.08, juce::Colour(0xffff6f6f) },
        { "triangleLevel", "Triangle Level", 0.0, 1.0, 0.12, juce::Colour(0xffa8e67a) },
        { "macroHardness", "Hardness", 0.0, 1.0, 0.50, juce::Colour(0xffff8c66) },
        { "macroWeight", "Weight", 0.0, 1.0, 0.50, juce::Colour(0xff7f9cff) },
        { "macroAir", "Air", 0.0, 1.0, 0.50, juce::Colour(0xff8ce6ff) },
        { "macroGrit", "Grit", 0.0, 1.0, 0.25, juce::Colour(0xffff6b7a) },
        { "macroSize", "Size", 0.0, 1.0, 0.50, juce::Colour(0xffbca0ff) }
    }};

    return specs;
}

const AutomationTargetSpec& getTargetSpec(const juce::String& parameterId)
{
    for (const auto& spec : getAutomationTargetSpecs())
        if (parameterId == spec.parameterId)
            return spec;

    return getAutomationTargetSpecs()[0];
}

// Which of the modulatable parameters (see getAutomationTargetSpecs) live on
// which node's inspector panel -- used to expose each editable parameter as
// its own graph input port, one per slider the user can already move by hand.
juce::StringArray nodeParameterIds(const juce::String& nodeType)
{
    if (nodeType == "filter")
        return { "filterCutoff", "filterResonance", "filterEnvelopeAmount" };
    if (nodeType == "output")
        return {};
    if (nodeType == "sine")     return { "sineLevel" };
    if (nodeType == "saw")      return { "sawLevel" };
    if (nodeType == "square")   return { "squareLevel" };
    if (nodeType == "triangle") return { "triangleLevel" };
    if (nodeType == "noise")    return { "noiseLevel" };
    if (nodeType == "mix")      return {};
    return {};
}

// Short label for a parameter port -- the inspector already carries the full
// name, so the graph-side label just needs to disambiguate at a glance.
juce::String shortParamLabel(const juce::String& parameterId)
{
    if (parameterId == "filterCutoff") return "Cutoff";
    if (parameterId == "filterResonance") return "Reso";
    if (parameterId == "filterEnvelopeAmount") return "Env Amt";
    if (parameterId == "pitchOffsetSemitones") return "Pitch";
    if (parameterId == "baseFrequency") return "Pitch";
    if (parameterId == "macroHardness") return "Hardness";
    if (parameterId == "macroWeight") return "Weight";
    if (parameterId == "macroAir") return "Air";
    if (parameterId == "macroGrit") return "Grit";
    if (parameterId == "macroSize") return "Size";
    if (parameterId.endsWith("Level")) return "Level";
    return parameterId;
}

bool usesLogScale(const juce::String& parameterId)
{
    return parameterId == "filterCutoff" || parameterId == "baseFrequency";
}

double normalizeLaneValue(const cw::PatchAutomationLane& lane, double value)
{
    auto clampedValue = juce::jlimit(lane.rangeMin, lane.rangeMax, value);
    if (usesLogScale(lane.targetParameter))
    {
        auto safeMin = juce::jmax(0.0001, lane.rangeMin);
        auto safeMax = juce::jmax(safeMin + 0.0001, lane.rangeMax);
        auto logMin = std::log(safeMin);
        auto logMax = std::log(safeMax);
        return juce::jlimit(0.0, 1.0, (std::log(clampedValue) - logMin) / juce::jmax(0.0001, logMax - logMin));
    }

    return juce::jmap(clampedValue, lane.rangeMin, lane.rangeMax, 0.0, 1.0);
}

double denormalizeLaneValue(const cw::PatchAutomationLane& lane, double normalizedValue)
{
    auto clampedValue = juce::jlimit(0.0, 1.0, normalizedValue);
    if (usesLogScale(lane.targetParameter))
    {
        auto safeMin = juce::jmax(0.0001, lane.rangeMin);
        auto safeMax = juce::jmax(safeMin + 0.0001, lane.rangeMax);
        auto logMin = std::log(safeMin);
        auto logMax = std::log(safeMax);
        return std::exp(logMin + (logMax - logMin) * clampedValue);
    }

    return juce::jmap(clampedValue, 0.0, 1.0, lane.rangeMin, lane.rangeMax);
}

juce::String formatLaneValue(const cw::PatchAutomationLane& lane, double value)
{
    if (lane.targetParameter == "pitchOffsetSemitones")
        return juce::String(value, 1) + " st";
    if (lane.targetParameter == "filterCutoff" || lane.targetParameter == "baseFrequency")
        return juce::String(juce::roundToInt(value)) + " Hz";
    if (lane.targetParameter == "filterResonance")
        return "Q " + juce::String(value, 2);
    if (lane.targetParameter == "filterEnvelopeAmount")
        return juce::String(value, 2);

    return juce::String(value, 2);
}

juce::String formatLaneRange(const cw::PatchAutomationLane& lane)
{
    return formatLaneValue(lane, lane.rangeMin) + " -> " + formatLaneValue(lane, lane.rangeMax);
}

float clamp01(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

double clampPointValue(double value)
{
    return juce::jlimit(0.0, 1.0, value);
}

float cutoffToNormalized(float cutoffHz)
{
    auto safeCutoff = juce::jlimit(kMinFilterCutoffHz, kMaxFilterCutoffHz, cutoffHz);
    auto logMin = std::log(kMinFilterCutoffHz);
    auto logMax = std::log(kMaxFilterCutoffHz);
    return clamp01((std::log(safeCutoff) - logMin) / juce::jmax(0.0001f, logMax - logMin));
}

float normalizedToCutoff(float normalized)
{
    auto clamped = clamp01(normalized);
    auto logMin = std::log(kMinFilterCutoffHz);
    auto logMax = std::log(kMaxFilterCutoffHz);
    return std::exp(logMin + (logMax - logMin) * clamped);
}

juce::String formatDurationText(double seconds)
{
    auto clampedSeconds = juce::jmax(0.1, seconds);
    auto totalMilliseconds = static_cast<int64_t>(std::llround(clampedSeconds * 1000.0));
    auto totalSeconds = totalMilliseconds / 1000;
    auto milliseconds = totalMilliseconds % 1000;
    auto hours = totalSeconds / 3600;
    auto minutes = (totalSeconds % 3600) / 60;
    auto remainingSeconds = totalSeconds % 60;

    if (hours > 0)
        return juce::String(hours) + ":" + juce::String(minutes).paddedLeft('0', 2) + ":" + juce::String(remainingSeconds).paddedLeft('0', 2);

    if (minutes > 0)
        return juce::String(minutes) + ":" + juce::String(remainingSeconds).paddedLeft('0', 2);

    if (milliseconds == 0)
        return juce::String(clampedSeconds, clampedSeconds < 10.0 ? 2 : 1) + " s";

    return juce::String(clampedSeconds, clampedSeconds < 10.0 ? 2 : 1) + " s";
}

double parseDurationText(const juce::String& text)
{
    auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return 0.1;

    auto colonParts = juce::StringArray::fromTokens(trimmed, ":", "");
    colonParts.removeEmptyStrings();
    if (colonParts.size() == 2 || colonParts.size() == 3)
    {
        double seconds = 0.0;
        for (int index = 0; index < colonParts.size(); ++index)
        {
            auto token = colonParts[index].trim();
            if (! token.containsOnly("0123456789."))
                return juce::jmax(0.1, trimmed.getDoubleValue());

            seconds = seconds * 60.0 + token.getDoubleValue();
        }

        return juce::jlimit(0.1, 3600.0, seconds);
    }

    auto normalized = trimmed.toLowerCase().removeCharacters("abcdefghijklmnopqrstuvwxyz");
    return juce::jlimit(0.1, 3600.0, normalized.getDoubleValue());
}

float applyBrightnessFilter(float input, float& state, float brightness, double sampleRate)
{
    auto cutoffHz = juce::jmap(juce::jlimit(0.02f, 1.0f, brightness), 180.0f, 12000.0f);
    auto alpha = juce::jlimit(0.001f, 0.99f, (float) (juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
    state += alpha * (input - state);
    return state;
}

float applyCurveMode(float localT, const juce::String& curveMode)
{
    auto t = clamp01(localT);
    if (curveMode == "stepped")
        return t < 1.0f ? 0.0f : 1.0f;
    if (curveMode == "smooth")
        return t * t * (3.0f - 2.0f * t);
    return t;
}

cw::PatchAutomationPoint makePoint(double time, double value, const juce::String& curve)
{
    cw::PatchAutomationPoint point;
    point.time = time;
    point.value = value;
    point.curve = curve;
    return point;
}

juce::Array<cw::PatchAutomationPoint> makeDefaultEnvelopePoints(const juce::String& curveMode)
{
    juce::Array<cw::PatchAutomationPoint> points;
    points.add(makePoint(0.0, 0.0, curveMode));
    points.add(makePoint(0.12, 1.0, curveMode));
    points.add(makePoint(0.42, 0.48, curveMode));
    points.add(makePoint(0.82, 0.48, curveMode));
    points.add(makePoint(1.0, 0.0, curveMode));
    return points;
}

cw::PatchAutomationLane makeLaneForSpec(const AutomationTargetSpec& spec, const juce::String& curveMode)
{
    cw::PatchAutomationLane lane;
    lane.id = juce::String(spec.parameterId) + "Lane";
    lane.name = spec.title;
    lane.targetParameter = spec.parameterId;
    lane.interpolation = curveMode;
    lane.startTime = 0.0;
    lane.endTime = 1.0;
    lane.rangeMin = spec.rangeMin;
    lane.rangeMax = spec.rangeMax;
    lane.points.add(makePoint(0.0, spec.defaultValue, curveMode));
    lane.points.add(makePoint(0.33, spec.defaultValue, curveMode));
    lane.points.add(makePoint(0.66, spec.defaultValue, curveMode));
    lane.points.add(makePoint(1.0, spec.defaultValue, curveMode));
    return lane;
}

juce::Array<cw::PatchAutomationLane> makeDefaultAutomationLanes(const juce::String& curveMode)
{
    juce::Array<cw::PatchAutomationLane> lanes;
    lanes.add(makeLaneForSpec(getTargetSpec("pitchOffsetSemitones"), curveMode));
    lanes.add(makeLaneForSpec(getTargetSpec("outputGain"), curveMode));
    lanes.add(makeLaneForSpec(getTargetSpec("filterCutoff"), curveMode));
    return lanes;
}

void sortPoints(juce::Array<cw::PatchAutomationPoint>& points)
{
    std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) { return left.time < right.time; });
}

void ensureEnvelopePoints(juce::Array<cw::PatchAutomationPoint>& points, const juce::String& curveMode)
{
    if (points.isEmpty())
        points = makeDefaultEnvelopePoints(curveMode);

    sortPoints(points);
    if (points.getFirst().time > 0.0)
        points.insert(0, makePoint(0.0, 0.0, curveMode));
    points.getReference(0).time = 0.0;
    points.getReference(0).value = 0.0;
    points.getReference(0).curve = curveMode;

    if (points.getLast().time < 1.0)
        points.add(makePoint(1.0, 0.0, curveMode));
    points.getReference(points.size() - 1).time = 1.0;
    points.getReference(points.size() - 1).value = 0.0;
    points.getReference(points.size() - 1).curve = curveMode;

    for (int index = 1; index < points.size() - 1; ++index)
    {
        auto minTime = points.getReference(index - 1).time + 0.02;
        auto maxTime = points.getReference(index + 1).time - 0.02;
        points.getReference(index).time = juce::jlimit(minTime, maxTime, points.getReference(index).time);
        points.getReference(index).value = clampPointValue(points.getReference(index).value);
        if (points.getReference(index).curve.isEmpty())
            points.getReference(index).curve = curveMode;
    }
}

void ensureLane(cw::PatchAutomationLane& lane, const juce::String& curveMode)
{
    auto spec = getTargetSpec(lane.targetParameter);
    if (lane.id.isEmpty())
        lane.id = juce::String(spec.parameterId) + "Lane";
    if (lane.name.isEmpty())
        lane.name = spec.title;
    if (lane.targetParameter.isEmpty())
        lane.targetParameter = spec.parameterId;
    if (lane.interpolation.isEmpty())
        lane.interpolation = curveMode;
    lane.startTime = juce::jlimit(0.0, 1.0, lane.startTime);
    lane.endTime = juce::jlimit(lane.startTime + 0.001, 1.0, lane.endTime <= lane.startTime ? 1.0 : lane.endTime);
    if (lane.rangeMax <= lane.rangeMin)
    {
        lane.rangeMin = spec.rangeMin;
        lane.rangeMax = spec.rangeMax;
    }
    if (lane.points.isEmpty())
        lane = makeLaneForSpec(spec, curveMode);

    sortPoints(lane.points);
    lane.points.getReference(0).time = 0.0;
    lane.points.getReference(lane.points.size() - 1).time = 1.0;
    for (int index = 0; index < lane.points.size(); ++index)
    {
        if (lane.points.getReference(index).curve.isEmpty())
            lane.points.getReference(index).curve = lane.interpolation;

        lane.points.getReference(index).value = juce::jlimit(lane.rangeMin, lane.rangeMax, lane.points.getReference(index).value);

        if (index > 0 && index < lane.points.size() - 1)
        {
            auto minTime = lane.points.getReference(index - 1).time + 0.02;
            auto maxTime = lane.points.getReference(index + 1).time - 0.02;
            lane.points.getReference(index).time = juce::jlimit(minTime, maxTime, lane.points.getReference(index).time);
        }
    }
}

void ensureRecipe(SignalLabPanel::SignalRecipe& recipe)
{
    ensureEnvelopePoints(recipe.envelopePoints, recipe.envelopeCurveMode);
    if (recipe.automationLanes.isEmpty())
        recipe.automationLanes = makeDefaultAutomationLanes(recipe.automationCurveMode);

    for (auto& lane : recipe.automationLanes)
    {
        if (lane.interpolation.isEmpty())
            lane.interpolation = recipe.automationCurveMode;
        ensureLane(lane, recipe.automationCurveMode);
    }
}

double sampleLane(const juce::Array<cw::PatchAutomationPoint>& points, const juce::String& interpolation, double t, double fallbackValue)
{
    if (points.isEmpty())
        return fallbackValue;
    if (points.size() == 1)
        return points.getFirst().value;

    auto clampedT = juce::jlimit(0.0, 1.0, t);
    for (int index = 0; index < points.size() - 1; ++index)
    {
        const auto& left = points.getReference(index);
        const auto& right = points.getReference(index + 1);
        if (clampedT >= left.time && clampedT <= right.time)
        {
            auto localRange = juce::jmax(0.0001, right.time - left.time);
            auto localT = (clampedT - left.time) / localRange;
            auto curve = right.curve.isNotEmpty() ? right.curve : interpolation;
            return juce::jmap((double) applyCurveMode((float) localT, curve), left.value, right.value);
        }
    }

    return points.getLast().value;
}

double sampleTargetLanes(const juce::Array<cw::PatchAutomationLane>& lanes,
                         const juce::String& targetParameter,
                         double t,
                         double fallbackValue)
{
    double sum = 0.0;
    int count = 0;
    for (const auto& lane : lanes)
    {
        if (lane.targetParameter == targetParameter)
        {
            auto localStart = juce::jlimit(0.0, 1.0, lane.startTime);
            auto localEnd = juce::jlimit(localStart + 0.001, 1.0, lane.endTime <= lane.startTime ? 1.0 : lane.endTime);
            if (t < localStart || t > localEnd)
                continue;

            auto localT = (t - localStart) / juce::jmax(0.0001, localEnd - localStart);
            sum += sampleLane(lane.points, lane.interpolation, localT, fallbackValue);
            ++count;
        }
    }

    return count > 0 ? (sum / (double) count) : fallbackValue;
}

juce::String serialisePointsJson(const juce::Array<cw::PatchAutomationPoint>& points)
{
    juce::Array<juce::var> values;
    for (const auto& point : points)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("time", point.time);
        object->setProperty("value", point.value);
        object->setProperty("curve", point.curve);
        values.add(juce::var(object));
    }

    return juce::JSON::toString(juce::var(values), true);
}

juce::Array<cw::PatchAutomationPoint> parsePointsJson(const juce::String& jsonText, const juce::String& curveMode)
{
    juce::Array<cw::PatchAutomationPoint> points;
    auto parsed = juce::JSON::parse(jsonText);
    auto* values = parsed.getArray();
    if (values == nullptr)
        return points;

    for (const auto& value : *values)
    {
        if (auto* object = value.getDynamicObject())
        {
            cw::PatchAutomationPoint point;
            point.time = (double) object->getProperty("time");
            point.value = (double) object->getProperty("value");
            point.curve = object->getProperty("curve").toString();
            if (point.curve.isEmpty())
                point.curve = curveMode;
            points.add(point);
        }
    }

    return points;
}

void setEnvelopeFromLegacy(SignalLabPanel::SignalRecipe& recipe,
                           float attackPosition,
                           float sustainPosition,
                           float releasePosition,
                           float sustainLevel)
{
    recipe.envelopePoints.clear();
    recipe.envelopePoints.add(makePoint(0.0, 0.0, recipe.envelopeCurveMode));
    recipe.envelopePoints.add(makePoint(attackPosition, 1.0, recipe.envelopeCurveMode));
    recipe.envelopePoints.add(makePoint(sustainPosition, sustainLevel, recipe.envelopeCurveMode));
    recipe.envelopePoints.add(makePoint(releasePosition, sustainLevel, recipe.envelopeCurveMode));
    recipe.envelopePoints.add(makePoint(1.0, 0.0, recipe.envelopeCurveMode));
    ensureEnvelopePoints(recipe.envelopePoints, recipe.envelopeCurveMode);
}

void setLaneValues(SignalLabPanel::SignalRecipe& recipe, const juce::String& parameterId, const std::array<float, 4>& values)
{
    for (auto& lane : recipe.automationLanes)
    {
        if (lane.targetParameter == parameterId)
        {
            auto spec = getTargetSpec(parameterId);
            lane.rangeMin = spec.rangeMin;
            lane.rangeMax = spec.rangeMax;
            lane.interpolation = recipe.automationCurveMode;
            lane.points.clear();
            lane.points.add(makePoint(0.0, values[0], recipe.automationCurveMode));
            lane.points.add(makePoint(0.33, values[1], recipe.automationCurveMode));
            lane.points.add(makePoint(0.66, values[2], recipe.automationCurveMode));
            lane.points.add(makePoint(1.0, values[3], recipe.automationCurveMode));
            ensureLane(lane, recipe.automationCurveMode);
            return;
        }
    }

    auto lane = makeLaneForSpec(getTargetSpec(parameterId), recipe.automationCurveMode);
    lane.targetParameter = parameterId;
    lane.points.clear();
    lane.points.add(makePoint(0.0, values[0], recipe.automationCurveMode));
    lane.points.add(makePoint(0.33, values[1], recipe.automationCurveMode));
    lane.points.add(makePoint(0.66, values[2], recipe.automationCurveMode));
    lane.points.add(makePoint(1.0, values[3], recipe.automationCurveMode));
    ensureLane(lane, recipe.automationCurveMode);
    recipe.automationLanes.add(lane);
}

double getLegacyAttackPosition(const juce::Array<cw::PatchAutomationPoint>& points)
{
    return points.size() > 1 ? points.getReference(1).time : 0.12;
}

double getLegacySustainPosition(const juce::Array<cw::PatchAutomationPoint>& points)
{
    return points.size() > 2 ? points.getReference(2).time : 0.42;
}

double getLegacyReleasePosition(const juce::Array<cw::PatchAutomationPoint>& points)
{
    return points.size() > 3 ? points.getReference(points.size() - 2).time : 0.82;
}

double getLegacySustainLevel(const juce::Array<cw::PatchAutomationPoint>& points)
{
    return points.size() > 2 ? points.getReference(2).value : 0.48;
}

juce::String graphNodeTitle(const juce::String& type)
{
    if (type == "value") return "Value";
    if (type == "valueGet") return "Get";
    if (type == "valueSet") return "Set";
    if (type == "sine") return "Sine Osc";
    if (type == "saw") return "Saw Osc";
    if (type == "square") return "Square Osc";
    if (type == "triangle") return "Triangle Osc";
    if (type == "noise") return "Noise";
    if (type == "mix") return "Mixer";
    if (type == "filter") return "Filter";
    if (type == "envelope") return "Envelope";
    if (type == "output") return "Sink";
    if (type == "scope") return "Oscilloscope";
    if (type == "analyzer") return "Frequency Analyzer";
    if (type == "midiFader") return "Fader Control";
    if (type == "midiButton") return "Button Control";
    return type;
}

juce::String graphNodeTypeBadge(const juce::String& type)
{
    if (type == "output") return "SINK";
    return type.toUpperCase();
}

juce::String timelineNodeTitle(const cw::PatchAutomationLane& lane)
{
    auto title = lane.name.isNotEmpty() ? lane.name : graphNodeTitle(lane.targetParameter);
    return title + " Timeline";
}

juce::Colour graphNodeAccent(const juce::String& type)
{
    if (type == "filter") return signalFilterColour();
    if (type == "envelope") return signalEnvelopeColour();
    if (type == "output") return signalOutputColour();
    if (type == "mix") return signalMixColour();
    if (type == "scope") return juce::Colour(0xff66e0ff);
    if (type == "analyzer") return juce::Colour(0xffff91c1);
    if (type == "value" || type == "valueGet" || type == "valueSet") return juce::Colour(0xffffd166);
    if (type == "midiFader" || type == "midiButton") return juce::Colour(0xffff5c8a);
    return signalSourceColour();
}
}

SignalLabPanel::SignalRecipe::SignalRecipe()
{
    envelopePoints = makeDefaultEnvelopePoints(envelopeCurveMode);
    automationLanes = makeDefaultAutomationLanes(automationCurveMode);
}

SignalLabPanel::EnvelopeEditor::EnvelopeEditor()
{
    setRepaintsOnMouseActivity(true);
}

void SignalLabPanel::EnvelopeEditor::setRecipe(const SignalRecipe& newRecipe)
{
    recipe = newRecipe;
    ensureRecipe(recipe);
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

juce::Point<float> SignalLabPanel::EnvelopeEditor::getPoint(int index) const
{
    const auto& point = recipe.envelopePoints.getReference(index);
    return toScreen((float) point.time, (float) point.value);
}

int SignalLabPanel::EnvelopeEditor::findPointAt(juce::Point<float> position) const
{
    auto bestDistance = 18.0f;
    auto bestIndex = -1;
    for (int index = 0; index < recipe.envelopePoints.size(); ++index)
    {
        auto distance = position.getDistanceFrom(getPoint(index));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
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
    for (int index = 0; index < recipe.envelopePoints.size(); ++index)
    {
        auto point = getPoint(index);
        if (index == 0)
            envelopePath.startNewSubPath(point);
        else
            envelopePath.lineTo(point);
    }

    auto end = toScreen(1.0f, 0.0f);
    g.setColour(signalAccentColour().withAlpha(0.15f));
    juce::Path fillPath(envelopePath);
    fillPath.lineTo(end.x, plot.getBottom());
    fillPath.lineTo(plot.getX(), plot.getBottom());
    fillPath.closeSubPath();
    g.fillPath(fillPath);

    g.setColour(signalAccentColour());
    g.strokePath(envelopePath, juce::PathStrokeType(2.5f));

    for (int index = 0; index < recipe.envelopePoints.size(); ++index)
    {
        auto point = getPoint(index);
        auto colour = index == 0 || index == recipe.envelopePoints.size() - 1 ? juce::Colour(0xff7dd36f)
                                                                               : juce::Colour(0xfff2cc60);
        g.setColour(colour);
        g.fillEllipse(point.x - 5.5f, point.y - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawEllipse(point.x - 5.5f, point.y - 5.5f, 11.0f, 11.0f, 1.0f);
    }

    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("Envelope (double-click to add, right-click to remove)", getLocalBounds().reduced(12, 6).removeFromTop(18), juce::Justification::centredLeft, false);
}

void SignalLabPanel::EnvelopeEditor::mouseDown(const juce::MouseEvent& event)
{
    dragIndex = findPointAt(event.position);

    if (dragIndex >= 0 && onGestureBegin)
        onGestureBegin();

    if (event.mods.isPopupMenu() && dragIndex > 0 && dragIndex < recipe.envelopePoints.size() - 1)
    {
        if (onDiscreteEditRequested)
            onDiscreteEditRequested("Remove envelope point");
        recipe.envelopePoints.remove(dragIndex);
        dragIndex = -1;
        ensureEnvelopePoints(recipe.envelopePoints, recipe.envelopeCurveMode);
        if (onEnvelopeChanged)
            onEnvelopeChanged(recipe.envelopePoints);
        repaint();
    }
}

void SignalLabPanel::EnvelopeEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (dragIndex < 0)
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    auto normalizedY = clamp01((plot.getBottom() - event.position.y) / plot.getHeight());

    auto& point = recipe.envelopePoints.getReference(dragIndex);
    if (dragIndex == 0 || dragIndex == recipe.envelopePoints.size() - 1)
    {
        point.value = 0.0;
        point.time = dragIndex == 0 ? 0.0 : 1.0;
    }
    else
    {
        auto minTime = recipe.envelopePoints.getReference(dragIndex - 1).time + 0.02;
        auto maxTime = recipe.envelopePoints.getReference(dragIndex + 1).time - 0.02;
        point.time = juce::jlimit((double) minTime, (double) maxTime, (double) normalizedX);
        point.value = juce::jlimit(0.0, 1.0, (double) normalizedY);
    }

    if (onEnvelopeChanged)
        onEnvelopeChanged(recipe.envelopePoints);

    repaint();
}

void SignalLabPanel::EnvelopeEditor::mouseUp(const juce::MouseEvent&)
{
    if (onGestureEnd)
        onGestureEnd();
}

void SignalLabPanel::EnvelopeEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    auto normalizedY = clamp01((plot.getBottom() - event.position.y) / plot.getHeight());

    if (onDiscreteEditRequested)
        onDiscreteEditRequested("Add envelope point");
    recipe.envelopePoints.add(makePoint(normalizedX, normalizedY, recipe.envelopeCurveMode));
    ensureEnvelopePoints(recipe.envelopePoints, recipe.envelopeCurveMode);
    if (onEnvelopeChanged)
        onEnvelopeChanged(recipe.envelopePoints);
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

void SignalLabPanel::AutomationLaneEditor::setLane(const cw::PatchAutomationLane& newLane, juce::Colour accentColour)
{
    lane = newLane;
    ensureLane(lane, lane.interpolation);
    laneAccent = accentColour;
    repaint();
}

juce::Rectangle<float> SignalLabPanel::AutomationLaneEditor::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(12.0f, 14.0f);
}

juce::Point<float> SignalLabPanel::AutomationLaneEditor::getPoint(int index) const
{
    auto plot = getPlotArea();
    const auto& point = lane.points.getReference(index);
    auto normalizedValue = (float) normalizeLaneValue(lane, point.value);
    auto x = juce::jmap((float) point.time, plot.getX(), plot.getRight());
    auto y = juce::jmap(normalizedValue, 0.0f, 1.0f, plot.getBottom(), plot.getY());
    return { x, y };
}

int SignalLabPanel::AutomationLaneEditor::findPointAt(juce::Point<float> position) const
{
    auto bestDistance = 18.0f;
    auto bestIndex = -1;
    for (int index = 0; index < lane.points.size(); ++index)
    {
        auto distance = position.getDistanceFrom(getPoint(index));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

double SignalLabPanel::AutomationLaneEditor::pointValueFromY(float y) const
{
    auto plot = getPlotArea();
    auto normalizedY = clamp01((plot.getBottom() - y) / plot.getHeight());
    return denormalizeLaneValue(lane, normalizedY);
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

    for (int index = 0; index <= 8; ++index)
    {
        auto x = juce::jmap((float) index / 8.0f, plot.getX(), plot.getRight());
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
    }

    juce::Path path;
    for (int index = 0; index < lane.points.size(); ++index)
    {
        if (index == 0)
        {
            path.startNewSubPath(getPoint(index));
            continue;
        }

        const auto& left = lane.points.getReference(index - 1);
        const auto& right = lane.points.getReference(index);
        auto leftPoint = getPoint(index - 1);
        auto span = juce::jmax(0.0001, right.time - left.time);
        auto curveMode = right.curve.isNotEmpty() ? right.curve : lane.interpolation;
        constexpr int subdivisions = 20;
        for (int step = 1; step <= subdivisions; ++step)
        {
            auto localT = (float) step / (float) subdivisions;
            auto curvedT = applyCurveMode(localT, curveMode);
            auto x = juce::jmap(localT, leftPoint.x, getPoint(index).x);
            auto value = juce::jmap((double) curvedT, left.value, right.value);
            auto normalizedValue = (float) normalizeLaneValue(lane, value);
            auto y = juce::jmap(normalizedValue, 0.0f, 1.0f, plot.getBottom(), plot.getY());
            juce::ignoreUnused(span);
            path.lineTo(x, y);
        }
    }

    g.setColour(laneAccent.withAlpha(0.15f));
    juce::Path fillPath(path);
    fillPath.lineTo(plot.getRight(), plot.getBottom());
    fillPath.lineTo(plot.getX(), plot.getBottom());
    fillPath.closeSubPath();
    g.fillPath(fillPath);

    g.setColour(laneAccent);
    g.strokePath(path, juce::PathStrokeType(2.0f));

    for (int index = 0; index < lane.points.size(); ++index)
    {
        auto point = getPoint(index);
        g.setColour(laneAccent);
        g.fillEllipse(point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawEllipse(point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f, 1.0f);
    }

    auto header = getLocalBounds().reduced(12, 6).removeFromTop(18);
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText(lane.name, header.removeFromLeft(190), juce::Justification::centredLeft, false);
    g.setFont(juce::Font(11.0f));
    g.setColour(juce::Colour(0xff8ea0b7));
    g.drawText(formatLaneRange(lane), header, juce::Justification::centredRight, false);

    auto footer = getLocalBounds().reduced(12, 10).removeFromBottom(16);
    g.setColour(juce::Colour(0xff708198));
    g.drawText("Start", footer.removeFromLeft(40), juce::Justification::centredLeft, false);
    g.drawText((lane.interpolation.isNotEmpty() ? lane.interpolation : juce::String("linear")).toUpperCase(),
               footer.removeFromRight(88),
               juce::Justification::centredRight,
               false);
    g.drawText("End", footer.removeFromRight(32), juce::Justification::centredRight, false);

    if (dragIndex >= 0 && dragIndex < lane.points.size())
    {
        auto point = getPoint(dragIndex);
        auto bubble = juce::Rectangle<float>(0.0f, 0.0f, 88.0f, 20.0f).withCentre({ point.x, point.y - 16.0f });
        bubble.setPosition(juce::jlimit(8.0f, (float) getWidth() - bubble.getWidth() - 8.0f, bubble.getX()),
                           juce::jlimit(24.0f, (float) getHeight() - bubble.getHeight() - 8.0f, bubble.getY()));
        g.setColour(juce::Colour(0xdd0f1724));
        g.fillRoundedRectangle(bubble, 6.0f);
        g.setColour(laneAccent.withAlpha(0.8f));
        g.drawRoundedRectangle(bubble, 6.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f).boldened());
        g.drawText(formatLaneValue(lane, lane.points.getReference(dragIndex).value), bubble.toNearestInt(), juce::Justification::centred, false);
    }
}

void SignalLabPanel::AutomationLaneEditor::mouseDown(const juce::MouseEvent& event)
{
    dragIndex = findPointAt(event.position);

    if (dragIndex >= 0 && onGestureBegin)
        onGestureBegin();

    if (event.mods.isPopupMenu() && dragIndex > 0 && dragIndex < lane.points.size() - 1)
    {
        if (onDiscreteEditRequested)
            onDiscreteEditRequested("Remove motion point");
        lane.points.remove(dragIndex);
        dragIndex = -1;
        ensureLane(lane, lane.interpolation);
        if (onLaneChanged)
            onLaneChanged(lane);
        repaint();
    }
}

void SignalLabPanel::AutomationLaneEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (dragIndex < 0)
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    auto& point = lane.points.getReference(dragIndex);
    point.value = pointValueFromY(event.position.y);
    point.curve = lane.interpolation;
    if (dragIndex == 0 || dragIndex == lane.points.size() - 1)
        point.time = dragIndex == 0 ? 0.0 : 1.0;
    else
    {
        auto minTime = lane.points.getReference(dragIndex - 1).time + 0.02;
        auto maxTime = lane.points.getReference(dragIndex + 1).time - 0.02;
        point.time = juce::jlimit((double) minTime, (double) maxTime, (double) normalizedX);
    }

    if (onLaneChanged)
        onLaneChanged(lane);

    repaint();
}

void SignalLabPanel::AutomationLaneEditor::mouseUp(const juce::MouseEvent&)
{
    if (onGestureEnd)
        onGestureEnd();
}

void SignalLabPanel::AutomationLaneEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    if (onDiscreteEditRequested)
        onDiscreteEditRequested("Add motion point");
    lane.points.add(makePoint(normalizedX, pointValueFromY(event.position.y), lane.interpolation));
    ensureLane(lane, lane.interpolation);
    if (onLaneChanged)
        onLaneChanged(lane);
    repaint();
}

void SignalLabPanel::NodeGraphCanvas::paint(juce::Graphics& g)
{
    g.fillAll(signalPanelColour());

    auto bounds = getLocalBounds();
    g.setColour(juce::Colour(0xff243244));
    for (int x = 0; x < bounds.getWidth(); x += 32)
        g.drawVerticalLine(x, 0.0f, (float) bounds.getHeight());
    for (int y = 0; y < bounds.getHeight(); y += 32)
        g.drawHorizontalLine(y, 0.0f, (float) bounds.getWidth());

    for (int index = 0; index < owner.graphNodes.size(); ++index)
    {
        auto boundsF = owner.graphToCanvas(owner.getGraphNodeBounds(index)).toFloat();
        auto& node = owner.graphNodes.getReference(index);
        auto selected = index == owner.selectedGraphNodeIndex;
        g.setColour(signalCardColour());
        g.fillRoundedRectangle(boundsF, 14.0f);
        g.setColour(selected ? juce::Colours::white : node.accent);
        g.drawRoundedRectangle(boundsF, 14.0f, selected ? 3.0f : 2.0f);

        auto badgeBounds = juce::Rectangle<float>(boundsF.getCentreX() - 42.0f, boundsF.getY() + 12.0f, 84.0f, 20.0f);
        g.setColour(node.accent.withAlpha(0.22f));
        g.fillRoundedRectangle(badgeBounds, 9.0f);
        g.setColour(node.accent);
        g.setFont(juce::Font(13.0f));
        g.drawText(graphNodeTypeBadge(node.type), badgeBounds.toNearestInt(), juce::Justification::centred, false);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(16.0f).boldened());
        g.drawText(node.title, juce::Rectangle<int>((int) boundsF.getX() + 12, (int) boundsF.getY() + 38, (int) boundsF.getWidth() - 24, 22), juce::Justification::centredLeft, true);

        juce::String detail;
        if (node.type == "sine" || node.type == "saw" || node.type == "square" || node.type == "triangle")
            detail = "Level " + juce::String(node.oscillatorLevel, 2) + " • " + juce::String(juce::roundToInt(node.oscillatorFrequencyHz)) + " Hz";
        else if (node.type == "noise") detail = "Level " + juce::String(node.oscillatorLevel, 2);
        else if (node.type == "filter") detail = node.filterMode + " • " + juce::String(juce::roundToInt(node.filterCutoffHz)) + " Hz";
        else if (node.type == "envelope") detail = node.envelopeCurveMode + " curve";
        else if (node.type == "output") detail = formatDurationText(owner.recipe.durationSeconds) + " • " + (owner.recipe.sinkMode == "wave" ? "Wave" : "Audio");
        else if (node.type == "value" || node.type == "valueGet" || node.type == "valueSet")
        {
            detail = node.type == "valueSet" ? "Driven by automation" : "Reads current value";
            for (auto& variable : owner.localControls)
            {
                if (variable.id == node.targetParameter)
                {
                    detail = variable.name + " • " + juce::String(variable.value, 2)
                            + (node.type == "valueSet" ? " (setter)" : "");
                    break;
                }
            }
        }
        else if (node.type == "midiFader" || node.type == "midiButton")
        {
            if (! node.midiLearned)
                detail = "Not learned — double-click, Learn";
            else
            {
                detail = (node.midiDeviceLabel.isNotEmpty() ? node.midiDeviceLabel : "Any device") + " • "
                        + (node.midiIsController ? "CC " : "Note ") + juce::String(node.midiNumber)
                        + " • " + juce::String(node.midiLiveValue, 2);
                if (node.type == "midiButton")
                    detail += node.midiButtonMode == "toggle" ? " (toggle)" : " (momentary)";
            }
        }
        else if (node.type == "scope") detail = "2 traces • " + juce::String(owner.probeSettings.scopeTimebaseMs, 1) + " ms";
        else if (node.type == "analyzer") detail = juce::String(juce::roundToInt(owner.probeSettings.analyzerMinHz)) + "-" + juce::String(juce::roundToInt(owner.probeSettings.analyzerMaxHz)) + " Hz";
        else if (node.type == "timeline")
        {
            detail = node.targetParameter;
            for (const auto& lane : owner.recipe.automationLanes)
            {
                if (lane.id == node.id)
                {
                    detail = juce::String((int) std::round(owner.recipe.durationSeconds * 1000.0 * lane.startTime))
                           + " ms - "
                           + juce::String((int) std::round(owner.recipe.durationSeconds * 1000.0 * lane.endTime))
                           + " ms";
                    break;
                }
            }
        }
        else if (node.type == "mix") detail = "Weighted sum • " + juce::String(node.mixerInputVolumes.size()) + " channels";
        else detail = "Auto sum bus";

        g.setColour(juce::Colour(0xff9db0c8));
        g.setFont(juce::Font(12.0f));
        g.drawText(detail, juce::Rectangle<int>((int) boundsF.getX() + 12, (int) boundsF.getBottom() - 28, (int) boundsF.getWidth() - 24, 16), juce::Justification::centredLeft, true);

        if (node.type == "scope" && owner.getDisplayBufferForNode(node.id).getNumSamples() > 8)
        {
            auto& tapBuffer = owner.getDisplayBufferForNode(node.id);
            auto mini = juce::Rectangle<float>(boundsF.getX() + 12.0f, boundsF.getY() + 64.0f, boundsF.getWidth() - 24.0f, 18.0f);
            g.setColour(juce::Colour(0xff0f141c));
            g.fillRoundedRectangle(mini, 4.0f);
            auto drawTrace = [&](int channel, juce::Colour colour, float gain)
            {
                juce::Path path;
                auto* data = tapBuffer.getReadPointer(juce::jmin(channel, tapBuffer.getNumChannels() - 1));
                auto samples = tapBuffer.getNumSamples();
                auto step = juce::jmax(1, samples / juce::jmax(1, (int) mini.getWidth()));
                bool started = false;
                int plotIndex = 0;
                for (int sample = 0; sample < samples && plotIndex < (int) mini.getWidth(); sample += step, ++plotIndex)
                {
                    auto x = mini.getX() + (float) plotIndex;
                    auto y = mini.getCentreY() - juce::jlimit(-1.0f, 1.0f, data[sample] * gain) * (mini.getHeight() * 0.42f);
                    if (! started) { path.startNewSubPath(x, y); started = true; }
                    else path.lineTo(x, y);
                }
                g.setColour(colour);
                g.strokePath(path, juce::PathStrokeType(1.2f));
            };
            drawTrace(0, juce::Colour(0xff66e0ff), (float) owner.probeSettings.scopeGainA);
            drawTrace(1, juce::Colour(0xffff9ac9), (float) owner.probeSettings.scopeGainB);
        }
        else if (node.type == "analyzer" && owner.getDisplayBufferForNode(node.id).getNumSamples() > 32)
        {
            auto& tapBuffer = owner.getDisplayBufferForNode(node.id);
            auto mini = juce::Rectangle<float>(boundsF.getX() + 12.0f, boundsF.getY() + 62.0f, boundsF.getWidth() - 24.0f, 20.0f);
            g.setColour(juce::Colour(0xff0f141c));
            g.fillRoundedRectangle(mini, 4.0f);
            auto* data = tapBuffer.getReadPointer(0);
            auto bins = 24;
            for (int bin = 0; bin < bins; ++bin)
            {
                auto start = bin * tapBuffer.getNumSamples() / bins;
                auto end = juce::jmin(tapBuffer.getNumSamples(), (bin + 1) * tapBuffer.getNumSamples() / bins);
                float peak = 0.0f;
                for (int i = start; i < end; ++i)
                    peak = juce::jmax(peak, std::abs(data[i]));
                auto height = peak * mini.getHeight();
                auto barWidth = mini.getWidth() / (float) bins;
                g.setColour(juce::Colour(0xffff91c1));
                g.fillRect(mini.getX() + bin * barWidth, mini.getBottom() - height, juce::jmax(1.0f, barWidth - 1.0f), height);
            }
        }

        for (auto& port : owner.getNodePorts(index))
        {
            auto centre = owner.graphToCanvas(port.position);
            if (port.isExec)
            {
                juce::Rectangle<float> box(centre.x - 6.0f, centre.y - 6.0f, 12.0f, 12.0f);
                g.setColour(juce::Colours::white);
                g.drawRect(box, 1.5f);
                juce::Path triangle;
                if (port.isOutput)
                {
                    triangle.addTriangle(box.getRight() + 2.0f, box.getCentreY(),
                                          box.getRight() + 9.0f, box.getCentreY() - 4.0f,
                                          box.getRight() + 9.0f, box.getCentreY() + 4.0f);
                }
                else
                {
                    triangle.addTriangle(box.getX() - 9.0f, box.getCentreY() - 4.0f,
                                          box.getX() - 9.0f, box.getCentreY() + 4.0f,
                                          box.getX() - 2.0f, box.getCentreY());
                }
                g.setColour(juce::Colours::white);
                g.fillPath(triangle);
            }
            else
            {
                auto colour = SignalLabPanel::portValueColour(port.valueType);
                g.setColour(colour);
                g.fillEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                g.drawEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 1.0f);

                if (port.label.isNotEmpty())
                {
                    g.setColour(juce::Colour(0xffb9c4d6));
                    g.setFont(juce::Font(12.0f));
                    auto justification = port.isOutput ? juce::Justification::centredLeft : juce::Justification::centredRight;
                    auto labelX = port.isOutput ? centre.x + 9.0f : centre.x - 81.0f;
                    g.drawText(port.label, juce::Rectangle<int>((int) labelX, (int) centre.y - 8, 72, 16), justification, true);
                }
            }
        }

        if (node.type == "mix")
        {
            auto buttonBounds = owner.graphToCanvas(owner.getMixerAddInputButtonBounds(index)).toFloat();
            g.setColour(juce::Colour(0xff232c3d));
            g.fillRoundedRectangle(buttonBounds, 4.0f);
            g.setColour(juce::Colour(0xff5a6b85));
            g.drawRoundedRectangle(buttonBounds, 4.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f));
            g.drawText("+ Input", buttonBounds, juce::Justification::centred, false);
        }
    }

    for (int connectionIndex = 0; connectionIndex < owner.graphConnections.size(); ++connectionIndex)
    {
        auto& connection = owner.graphConnections.getReference(connectionIndex);
        auto colour = connection.isExec ? juce::Colours::white : SignalLabPanel::portValueColour(connection.valueType);
        auto selected = connectionIndex == owner.selectedConnectionIndex;

        auto path = owner.buildConnectionPath(connectionIndex);
        g.setColour(colour.withAlpha(selected ? 1.0f : 0.85f));
        g.strokePath(path, juce::PathStrokeType(selected ? 3.5f : 2.5f));

        for (auto& waypoint : connection.waypoints)
        {
            auto centre = owner.graphToCanvas(waypoint).toFloat();
            g.setColour(juce::Colour(0xff171d28));
            g.fillEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);
            g.setColour(colour);
            g.drawEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 2.0f);
        }
    }

    if (wireDragging)
    {
        auto fromPoint = owner.graphToCanvas(wireDragPort.position);
        auto toPoint = wireDragCurrentPoint.toFloat();
        auto colour = wireDragPort.isExec ? juce::Colours::white : SignalLabPanel::portValueColour(wireDragPort.valueType);
        auto handle = juce::jmax(24.0f, std::abs(toPoint.x - fromPoint.x) * 0.4f);
        juce::Path path;
        path.startNewSubPath(fromPoint);
        path.cubicTo(fromPoint.translated(handle, 0.0f), toPoint.translated(-handle, 0.0f), toPoint);
        g.setColour(colour.withAlpha(0.65f));
        g.strokePath(path, juce::PathStrokeType(2.5f));
    }
}

void SignalLabPanel::NodeGraphCanvas::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (event.mods.isRightButtonDown())
    {
        auto hit = owner.findGraphNodeAt(event.getPosition());
        if (hit >= 0)
        {
            owner.setSelectedGraphNodeIndex(hit);
            owner.showNodeContextMenu(hit, event.getPosition());
            return;
        }

        int waypointIndex = -1;
        auto connectionHit = owner.findConnectionAt(event.getPosition(), &waypointIndex);
        if (connectionHit >= 0)
        {
            owner.selectedConnectionIndex = connectionHit;
            owner.showConnectionContextMenu(connectionHit, waypointIndex, event.getPosition());
            return;
        }

        owner.showCanvasActionMenu(event.getPosition());
        return;
    }

    auto portHit = owner.findPortAt(event.getPosition());
    if (portHit.found)
    {
        wireDragging = true;
        wireDragNodeIndex = portHit.nodeIndex;
        wireDragPort = portHit.port;
        wireDragCurrentPoint = event.getPosition();
        owner.setSelectedGraphNodeIndex(-1);
        owner.selectedConnectionIndex = -1;
        repaint();
        return;
    }

    int waypointIndex = -1;
    auto connectionHit = owner.findConnectionAt(event.getPosition(), &waypointIndex);
    if (connectionHit >= 0 && waypointIndex >= 0)
    {
        waypointDragConnectionIndex = connectionHit;
        waypointDragIndex = waypointIndex;
        owner.selectedConnectionIndex = connectionHit;
        repaint();
        return;
    }

    dragNodeIndex = owner.findGraphNodeAt(event.getPosition());
    if (dragNodeIndex >= 0 && owner.graphNodes.getReference(dragNodeIndex).type == "mix"
        && owner.graphToCanvas(owner.getMixerAddInputButtonBounds(dragNodeIndex)).contains(event.getPosition()))
    {
        owner.addMixerInput(dragNodeIndex);
        dragNodeIndex = -1;
        dragMoved = true;
        repaint();
        return;
    }

    dragMoved = false;
    owner.setSelectedGraphNodeIndex(dragNodeIndex);
    if (dragNodeIndex >= 0)
    {
        owner.selectedConnectionIndex = -1;
        dragOffset = event.getPosition() - owner.graphToCanvas(owner.getGraphNodeBounds(dragNodeIndex)).getPosition();
    }
    else if (connectionHit >= 0)
    {
        owner.selectedConnectionIndex = connectionHit;
    }
    else
    {
        owner.selectedConnectionIndex = -1;
        panning = true;
        panAnchor = event.getScreenPosition().roundToInt();
        viewportAnchor = { owner.graphViewport.getViewPositionX(), owner.graphViewport.getViewPositionY() };
    }
    repaint();
}

void SignalLabPanel::NodeGraphCanvas::mouseDrag(const juce::MouseEvent& event)
{
    if (wireDragging)
    {
        wireDragCurrentPoint = event.getPosition();
        repaint();
        return;
    }

    if (waypointDragConnectionIndex >= 0)
    {
        if (waypointDragConnectionIndex < owner.graphConnections.size())
        {
            auto& connection = owner.graphConnections.getReference(waypointDragConnectionIndex);
            if (waypointDragIndex >= 0 && waypointDragIndex < connection.waypoints.size())
                connection.waypoints.getReference(waypointDragIndex) = owner.canvasToGraph(event.getPosition());
        }
        repaint();
        return;
    }

    if (panning)
    {
        auto delta = event.getScreenPosition().roundToInt() - panAnchor;
        owner.graphViewport.setViewPosition(viewportAnchor.x - delta.x, viewportAnchor.y - delta.y);
        return;
    }

    if (dragNodeIndex < 0)
        return;

    auto& node = owner.graphNodes.getReference(dragNodeIndex);
    if (node.locked)
        return;

    dragMoved = true;
    node.position = owner.canvasToGraph(event.getPosition() - dragOffset);
    auto halfWidth = juce::jmax(300, owner.canvasWorkspaceSize.x / 2);
    auto halfHeight = juce::jmax(220, owner.canvasWorkspaceSize.y / 2);
    node.position.x = juce::jlimit(-halfWidth, halfWidth, node.position.x);
    node.position.y = juce::jlimit(-halfHeight, halfHeight, node.position.y);
    owner.noteInteraction();
    owner.updateCanvasWorkspace();
    repaint();
}

void SignalLabPanel::NodeGraphCanvas::mouseUp(const juce::MouseEvent& event)
{
    if (wireDragging)
    {
        wireDragging = false;
        owner.tryCompleteConnection(wireDragNodeIndex, wireDragPort, event.getPosition());
        wireDragNodeIndex = -1;
        repaint();
        return;
    }

    if (waypointDragConnectionIndex >= 0)
    {
        waypointDragConnectionIndex = -1;
        waypointDragIndex = -1;
        return;
    }

    if (panning)
    {
        panning = false;
        return;
    }

    if (! dragMoved && dragNodeIndex < 0)
        owner.showCanvasActionMenu(event.getPosition());

    dragNodeIndex = -1;
}

void SignalLabPanel::NodeGraphCanvas::mouseDoubleClick(const juce::MouseEvent& event)
{
    auto hit = owner.findGraphNodeAt(event.getPosition());
    owner.setSelectedGraphNodeIndex(hit);
    if (hit >= 0)
        owner.openNodeEditorForSelection();
}

void SignalLabPanel::NodeGraphCanvas::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! event.mods.isCtrlDown())
    {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }

    auto oldZoom = owner.canvasZoom;
    owner.canvasZoom = juce::jlimit(0.35f, 2.5f, owner.canvasZoom + wheel.deltaY * 0.15f);
    if (std::abs(owner.canvasZoom - oldZoom) < 0.0001f)
        return;

    auto cursorInViewport = event.getEventRelativeTo(&owner.graphViewport).position;
    auto canvasPointBefore = owner.graphViewport.getViewPosition().toFloat() + cursorInViewport;
    auto graphPointBefore = owner.canvasToGraph(canvasPointBefore);
    owner.updateCanvasWorkspace();
    auto newCanvasPoint = owner.graphToCanvas(graphPointBefore);
    owner.graphViewport.setViewPosition((int) std::round(newCanvasPoint.x - cursorInViewport.x),
                                        (int) std::round(newCanvasPoint.y - cursorInViewport.y));
}

bool SignalLabPanel::NodeGraphCanvas::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (owner.selectedConnectionIndex >= 0)
            owner.removeConnection(owner.selectedConnectionIndex);
        else
            owner.removeSelectedGraphNode();
        return true;
    }

    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        owner.openNodeEditorForSelection();
        return true;
    }

    return false;
}

SignalLabPanel::FloatingWindow::FloatingWindow(SignalLabPanel& ownerRef, Kind kindToUse)
    : owner(ownerRef), kind(kindToUse)
{
    setInterceptsMouseClicks(true, true);
}

SignalLabPanel::NodeToolboxPane::NodeToolboxPane()
{
    // Title and Add button live outside the pane (SignalLabPanel's fixed
    // "Variables" header + addLocalControlButton) so they stay put while
    // this pane's row list scrolls beneath them.
}

void SignalLabPanel::NodeToolboxPane::VariableButton::mouseDown(const juce::MouseEvent& event)
{
    juce::TextButton::mouseDown(event);
    mouseDownScreenPosition = event.getMouseDownScreenPosition();
    dragStarted = false;
}

void SignalLabPanel::NodeToolboxPane::VariableButton::mouseDrag(const juce::MouseEvent& event)
{
    juce::TextButton::mouseDrag(event);
    auto screenPoint = event.getScreenPosition().roundToInt();
    if (! dragStarted && mouseDownScreenPosition.getDistanceFrom(screenPoint) > 6)
    {
        dragStarted = true;
        if (onDragStarted)
            onDragStarted(variableId, screenPoint);
    }

    if (dragStarted && onDragMoved)
        onDragMoved(variableId, screenPoint);
}

void SignalLabPanel::NodeToolboxPane::VariableButton::mouseUp(const juce::MouseEvent& event)
{
    juce::TextButton::mouseUp(event);
    if (dragStarted && onDragEnded)
        onDragEnded(variableId, event.getScreenPosition().roundToInt());
    dragStarted = false;
}

void SignalLabPanel::NodeToolboxPane::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff131922));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(0xff283243));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 10.0f, 1.0f);
}

void SignalLabPanel::NodeToolboxPane::resized()
{
    auto area = getLocalBounds().reduced(10);
    for (int index = 0; index < variableButtons.size(); ++index)
    {
        auto row = area.removeFromTop(42);
        removeButtons[index]->setBounds(row.removeFromRight(28));
        row.removeFromRight(6);
        variableButtons[index]->setBounds(row);
        area.removeFromTop(6);
    }
}

int SignalLabPanel::NodeToolboxPane::getRequiredHeight() const
{
    return 20 + localVariables.size() * (42 + 6);
}

void SignalLabPanel::NodeToolboxPane::setVariables(const juce::Array<LocalControlVariable>& variables)
{
    localVariables = variables;
    while (variableButtons.size() < localVariables.size())
    {
        auto* button = variableButtons.add(new VariableButton({}));
        addAndMakeVisible(button);
        auto* removeButton = removeButtons.add(new juce::TextButton("x"));
        addAndMakeVisible(removeButton);
    }

    for (int index = 0; index < variableButtons.size(); ++index)
    {
        auto visible = index < localVariables.size();
        variableButtons[index]->setVisible(visible);
        removeButtons[index]->setVisible(visible);
        if (! visible)
            continue;

        auto variable = localVariables.getReference(index);
        variableButtons[index]->variableId = variable.id;
        auto text = variable.name + "\n" + variable.valueType;
        variableButtons[index]->setButtonText(text);
        variableButtons[index]->onClick = [this, variableId = variable.id]
        {
            if (onVariableSelected)
                onVariableSelected(variableId);
        };
        removeButtons[index]->onClick = [this, variableId = variable.id]
        {
            if (onVariableRemoveRequested)
                onVariableRemoveRequested(variableId);
        };
        variableButtons[index]->onDragStarted = [this](const juce::String& variableId, juce::Point<int> screenPoint)
        {
            if (onVariableDragStarted)
                onVariableDragStarted(variableId, screenPoint);
        };
        variableButtons[index]->onDragMoved = [this](const juce::String& variableId, juce::Point<int> screenPoint)
        {
            if (onVariableDragMoved)
                onVariableDragMoved(variableId, screenPoint);
        };
        variableButtons[index]->onDragEnded = [this](const juce::String& variableId, juce::Point<int> screenPoint)
        {
            if (onVariableDragEnded)
                onVariableDragEnded(variableId, screenPoint);
        };
    }

    setSize(getWidth(), getRequiredHeight());
    resized();
}

SignalLabPanel::NodeSearchPanel::NodeSearchPanel()
{
    searchEditor.setFont(juce::Font(11.0f));
    searchEditor.setTextToShowWhenEmpty("Search nodes...", juce::Colour(0xff72839b));
    searchEditor.onTextChange = [this] { refreshResults(); };
    addAndMakeVisible(searchEditor);
}

void SignalLabPanel::NodeSearchPanel::setEntries(juce::Array<Entry> entries)
{
    allEntries = std::move(entries);
    refreshResults();
}

void SignalLabPanel::NodeSearchPanel::refreshResults()
{
    auto query = searchEditor.getText().trim().toLowerCase();
    visibleEntries.clear();
    for (auto& entry : allEntries)
    {
        if (query.isEmpty() || entry.label.toLowerCase().contains(query))
            visibleEntries.add(entry);
    }

    while (resultButtons.size() < visibleEntries.size())
    {
        auto* button = resultButtons.add(new juce::TextButton());
        button->setLookAndFeel(&compactLookAndFeel);
        addAndMakeVisible(button);
    }

    for (int index = 0; index < resultButtons.size(); ++index)
    {
        auto visible = index < visibleEntries.size();
        resultButtons[index]->setVisible(visible);
        if (! visible)
            continue;

        auto entry = visibleEntries.getReference(index);
        resultButtons[index]->setButtonText(entry.label);
        resultButtons[index]->onClick = [this, entry]
        {
            if (onEntryChosen)
                onEntryChosen(entry.type, entry.payload);
            if (onDismissRequested)
                onDismissRequested();
        };
    }

    resized();
}

void SignalLabPanel::NodeSearchPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171d28));
}

void SignalLabPanel::NodeSearchPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    searchEditor.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);
    for (auto* button : resultButtons)
    {
        if (! button->isVisible())
            continue;
        button->setBounds(area.removeFromTop(20));
        area.removeFromTop(2);
    }
}

void SignalLabPanel::FloatingWindow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xf0182230));
    g.fillRoundedRectangle(bounds, 14.0f);
    g.setColour(kind == Kind::NodeEditor ? signalAccentColour() : juce::Colour(0xfff2cc60));
    g.drawRoundedRectangle(bounds, 14.0f, 2.0f);

    auto header = bounds.removeFromTop(34.0f);
    g.setColour((kind == Kind::NodeEditor ? signalAccentColour() : juce::Colour(0xfff2cc60)).withAlpha(0.18f));
    g.fillRoundedRectangle(header, 14.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText(kind == Kind::NodeEditor ? "Node Editor" : "Control Pad",
               header.reduced(12.0f, 0.0f).toNearestInt(),
               juce::Justification::centredLeft,
               false);
}

void SignalLabPanel::SectionPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff131922));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(0xff283243));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 10.0f, 1.0f);
}

void SignalLabPanel::FloatingWindow::mouseDown(const juce::MouseEvent& event)
{
    dragAnchor = event.getPosition();
    startBounds = getBounds();
}

void SignalLabPanel::FloatingWindow::mouseDrag(const juce::MouseEvent& event)
{
    auto delta = event.getPosition() - dragAnchor;
    auto moved = startBounds.translated(delta.x, delta.y);
    auto limits = owner.getLocalBounds().reduced(8);
    moved.setPosition(juce::jlimit(limits.getX(), juce::jmax(limits.getX(), limits.getRight() - moved.getWidth()), moved.getX()),
                      juce::jlimit(limits.getY(), juce::jmax(limits.getY(), limits.getBottom() - moved.getHeight()), moved.getY()));
    if (kind == Kind::NodeEditor)
        owner.nodeEditorBounds = moved;
    else
        owner.controlPadBounds = moved;
    owner.layoutFloatingWindows();
}

SignalLabPanel::SignalLabPanel()
{
    setName("Signal Lab");
    runtimePlayer.prepare(recipe.sampleRate, 512);
    ensureRecipe(recipe);

    titleLabel.setText("Signal Lab", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Forge tones from oscillators, filters, motion, and envelope shaping, then inspect the waveform and spectrum.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa7b6cb));
    addAndMakeVisible(statusLabel);

    signalMenuButton.onClick = [this]
    {
        showSignalMenu();
    };
    addAndMakeVisible(signalMenuButton);

    compileButton.onClick = [this] { compileGraph(); };
    addAndMakeVisible(compileButton);

    playButton.onClick = [this] { triggerTransportPlay(); };
    addAndMakeVisible(playButton);

    stopButton.onClick = [this] { stopTransport(); };
    addAndMakeVisible(stopButton);

    repeatButton.setClickingTogglesState(true);
    repeatButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5f93ff));
    repeatButton.setTooltip("Play again after a delay, on its own pace -- not a seamless loop. Set the gap with the seconds field next to it.");
    repeatButton.onClick = [this]
    {
        repeatEnabled = repeatButton.getToggleState();
        if (repeatEnabled)
            triggerTransportPlay();
    };
    addAndMakeVisible(repeatButton);

    configureSlider(repeatDelaySlider, 0.0, 10.0, 0.1);
    repeatDelaySlider.setValue(repeatDelaySeconds, juce::dontSendNotification);
    repeatDelaySlider.setTextValueSuffix(" s");
    repeatDelaySlider.setTooltip("Seconds to wait between repeats (0 = back to back)");
    repeatDelaySlider.onValueChange = [this] { repeatDelaySeconds = repeatDelaySlider.getValue(); };

    toolboxPane.onAddVariableRequested = [this]
    {
        captureUndoCheckpoint("Add graph variable");
        LocalControlVariable control;
        control.id = "localControl" + juce::String(localControls.size() + 1);
        control.name = "Value " + juce::String(localControls.size() + 1);
        control.targetParameter = "outputGain";
        control.value = 1.0f;
        localControls.add(control);
        rebuildLocalControlChrome();
        refreshVariablePanel();
    };
    toolboxPane.onPlaceVariableRequested = [this](const juce::String& variableId)
    {
        auto dropPoint = juce::Point<int>(220, 80 + graphNodes.size() * 18);
        bool isAutomated = false;
        for (auto& variable : localControls)
            if (variable.id == variableId)
                isAutomated = variable.exposedToAutomation;
        auto nodeType = (isAutomated && ! hasSetterNodeForVariable(variableId)) ? "valueSet" : "valueGet";
        addGraphNode(nodeType, dropPoint, variableId);
    };
    toolboxPane.onVariableSelected = [this](const juce::String& variableId)
    {
        for (int index = 0; index < localControls.size(); ++index)
            if (localControls.getReference(index).id == variableId)
                selectedLocalControlIndex = index;
        refreshSelectedVariableEditor();
    };
    toolboxPane.onVariableRemoveRequested = [this](const juce::String& variableId)
    {
        captureUndoCheckpoint("Remove graph variable");
        for (int index = 0; index < localControls.size(); ++index)
        {
            if (localControls.getReference(index).id == variableId)
            {
                localControls.remove(index);
                break;
            }
        }
        if (selectedLocalControlIndex >= localControls.size())
            selectedLocalControlIndex = localControls.size() - 1;
        rebuildLocalControlChrome();
        refreshVariablePanel();
        nodeGraphCanvas.repaint();
    };
    variablesViewport.setViewedComponent(&toolboxPane, false);
    variablesViewport.setScrollBarsShown(true, false);
    variablesViewport.setScrollBarThickness(10);
    addAndMakeVisible(variablesViewport);

    variableDetailsViewport.setViewedComponent(&variableDetailsContent, false);
    variableDetailsViewport.setScrollBarsShown(true, false);
    variableDetailsViewport.setScrollBarThickness(10);
    addAndMakeVisible(variableDetailsViewport);
    nodeGraphCanvas.setWantsKeyboardFocus(true);
    graphViewport.setViewedComponent(&nodeGraphCanvas, false);
    graphViewport.setScrollBarsShown(false, false);
    graphViewport.setScrollBarThickness(0);
    addAndMakeVisible(graphViewport);

    addAndMakeVisible(nodeEditorWindow);
    nodeEditorCloseButton.onClick = [this] { closeNodeEditor(); };
    addAndMakeVisible(nodeEditorCloseButton);

    addAndMakeVisible(controlPadWindow);
    addAndMakeVisible(controlPadCloseButton);
    addAndMakeVisible(addLocalControlButton);
    addLocalControlButton.onClick = [this]
    {
        captureUndoCheckpoint("Add graph variable");
        LocalControlVariable control;
        control.id = "localControl" + juce::String(localControls.size() + 1);
        control.name = "Value " + juce::String(localControls.size() + 1);
        control.valueType = "Float";
        control.targetParameter = "outputGain";
        control.value = 1.0f;
        control.exposedToAutomation = true;
        localControls.add(control);
        selectedLocalControlIndex = localControls.size() - 1;
        rebuildLocalControlChrome();
        refreshVariablePanel();
    };

    inspectorTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    inspectorTitleLabel.setFont(juce::Font(18.0f).boldened());
    addAndMakeVisible(inspectorTitleLabel);

    inspectorBodyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9aafc8));
    inspectorBodyLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(inspectorBodyLabel);

    nameLabel.setText("Sound Name", juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    nameEditor.setText(recipe.name, juce::dontSendNotification);
    nameEditor.onTextChange = [this]
    {
        if (suppressCallbacks)
            return;

        if (! nameEditUndoCaptured)
        {
            captureUndoCheckpoint("Rename sound");
            nameEditUndoCaptured = true;
        }
        recipe.name = nameEditor.getText().trim();
        updateStatusText();
    };
    nameEditor.onFocusLost = [this]
    {
        nameEditUndoCaptured = false;
    };
    addAndMakeVisible(nameEditor);

    descriptionLabel.setText("Description", juce::dontSendNotification);
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(descriptionLabel);

    descriptionEditor.setMultiLine(true, true);
    descriptionEditor.setReturnKeyStartsNewLine(true);
    descriptionEditor.setText(recipe.description, juce::dontSendNotification);
    descriptionEditor.onTextChange = [this]
    {
        if (suppressCallbacks)
            return;

        if (! descriptionEditUndoCaptured)
        {
            captureUndoCheckpoint("Edit signal description");
            descriptionEditUndoCaptured = true;
        }
        recipe.description = descriptionEditor.getText();
    };
    descriptionEditor.onFocusLost = [this]
    {
        descriptionEditUndoCaptured = false;
    };
    addAndMakeVisible(descriptionEditor);

    propertiesHeaderLabel.setText("Properties", juce::dontSendNotification);
    propertiesHeaderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    propertiesHeaderLabel.setFont(juce::Font(18.0f).boldened());
    addAndMakeVisible(propertiesHeaderLabel);

    signalSectionLabel.setText("Signal", juce::dontSendNotification);
    signalSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(signalSectionLabel);

    variablesSectionLabel.setText("Variables", juce::dontSendNotification);
    variablesSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(variablesSectionLabel);

    selectedVariableSectionLabel.setText("Selected Variable", juce::dontSendNotification);
    selectedVariableSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(selectedVariableSectionLabel);

    addAndMakeVisible(signalPropertiesPanel);
    signalPropertiesPanel.toBack();
    addAndMakeVisible(variablesPanel);
    variablesPanel.toBack();
    addAndMakeVisible(variableDetailsPanel);
    variableDetailsPanel.toBack();

    signalMetaLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9aafc8));
    signalMetaLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(signalMetaLabel);

    variableNameLabel.setText("Name", juce::dontSendNotification);
    variableNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    variableDetailsContent.addAndMakeVisible(variableNameLabel);
    variableDetailsContent.addAndMakeVisible(variableNameEditor);
    variableNameEditor.onTextChange = [this]
    {
        if (suppressCallbacks || selectedLocalControlIndex < 0 || selectedLocalControlIndex >= localControls.size())
            return;
        localControls.getReference(selectedLocalControlIndex).name = variableNameEditor.getText().trim();
        refreshVariablePanel();
        nodeGraphCanvas.repaint();
    };

    variableDescriptionLabel.setText("Description", juce::dontSendNotification);
    variableDescriptionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    variableDetailsContent.addAndMakeVisible(variableDescriptionLabel);
    variableDescriptionEditor.setMultiLine(true, true);
    variableDescriptionEditor.setReturnKeyStartsNewLine(true);
    variableDetailsContent.addAndMakeVisible(variableDescriptionEditor);
    variableDescriptionEditor.onTextChange = [this]
    {
        if (suppressCallbacks || selectedLocalControlIndex < 0 || selectedLocalControlIndex >= localControls.size())
            return;
        localControls.getReference(selectedLocalControlIndex).description = variableDescriptionEditor.getText();
    };

    variableTypeLabel.setText("Type", juce::dontSendNotification);
    variableTypeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    variableDetailsContent.addAndMakeVisible(variableTypeLabel);
    variableTypeSelector.addItem("Float", 1);
    variableTypeSelector.addItem("Int", 2);
    variableTypeSelector.addItem("Bool", 3);
    variableDetailsContent.addAndMakeVisible(variableTypeSelector);
    variableTypeSelector.onChange = [this]
    {
        if (suppressCallbacks || selectedLocalControlIndex < 0 || selectedLocalControlIndex >= localControls.size())
            return;
        localControls.getReference(selectedLocalControlIndex).valueType = variableTypeSelector.getText();
        refreshVariablePanel();
    };

    variableAccessLabel.setText("Access", juce::dontSendNotification);
    variableAccessLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    variableDetailsContent.addAndMakeVisible(variableAccessLabel);
    variableAccessSelector.addItem("Private", 1);
    variableAccessSelector.addItem("Public", 2);
    variableDetailsContent.addAndMakeVisible(variableAccessSelector);
    variableAccessSelector.onChange = [this]
    {
        if (suppressCallbacks || selectedLocalControlIndex < 0 || selectedLocalControlIndex >= localControls.size())
            return;
        localControls.getReference(selectedLocalControlIndex).accessScope = variableAccessSelector.getText();
    };

    variableValueLabel.setText("Value", juce::dontSendNotification);
    variableValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    variableDetailsContent.addAndMakeVisible(variableValueLabel);
    variableDetailsContent.addAndMakeVisible(variableValueEditor);
    variableValueEditor.onTextChange = [this]
    {
        if (suppressCallbacks || selectedLocalControlIndex < 0 || selectedLocalControlIndex >= localControls.size())
            return;
        auto text = variableValueEditor.getText().trim();
        auto& variable = localControls.getReference(selectedLocalControlIndex);
        if (variable.valueType == "Bool")
            variable.value = (text.equalsIgnoreCase("true") || text == "1") ? 1.0f : 0.0f;
        else if (variable.valueType == "Int")
            variable.value = (float) text.getIntValue();
        else
            variable.value = text.getFloatValue();
        applyLocalControlToRecipe(variable);
        regenerateSignal();
    };

    variableDetailsContent.addAndMakeVisible(variableAutomationToggle);
    variableAutomationToggle.onClick = [this]
    {
        if (selectedLocalControlIndex < 0 || selectedLocalControlIndex >= localControls.size())
            return;
        localControls.getReference(selectedLocalControlIndex).exposedToAutomation = variableAutomationToggle.getToggleState();
        variableValueEditor.setEnabled(! variableAutomationToggle.getToggleState());
    };

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

        captureUndoCheckpoint("Apply sound template");
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
    setupLabel(filterModeLabel, "Filter Type");
    setupLabel(filterCutoffLabel, "Filter Cutoff");
    setupLabel(filterResonanceLabel, "Filter Resonance");
    setupLabel(filterEnvelopeLabel, "Filter Envelope");
    setupLabel(envelopeCurveLabel, "Envelope Curve");
    setupLabel(automationCurveLabel, "Automation Curve");
    setupLabel(macroHardnessLabel, "Hardness");
    setupLabel(macroWeightLabel, "Weight");
    setupLabel(macroAirLabel, "Air");
    setupLabel(macroGritLabel, "Grit");
    setupLabel(macroSizeLabel, "Size");
    setupLabel(sineLabel, "Sine");
    setupLabel(sawLabel, "Saw");
    setupLabel(squareLabel, "Square");
    setupLabel(triangleLabel, "Triangle");
    setupLabel(noiseLabel, "Noise");
    setupLabel(probeControlALabel, "Probe A");
    setupLabel(probeControlBLabel, "Probe B");
    setupLabel(probeControlCLabel, "Probe C");
    setupLabel(probeControlDLabel, "Probe D");

    configureSlider(frequencySlider, 30.0, 2400.0, 1.0);
    configureSlider(durationSlider, 0.1, 3600.0, 0.01);
    durationSlider.setSkewFactorFromMidPoint(8.0);
    durationSlider.setNumDecimalPlacesToDisplay(2);
    durationSlider.setTextValueSuffix(" s");
    durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 96, 22);
    durationSlider.textFromValueFunction = [](double value)
    {
        return formatDurationText(value);
    };
    durationSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return parseDurationText(text);
    };
    configureSlider(pitchSlider, -24.0, 24.0, 0.1);
    filterModeSelector.addItem("Low-pass", 1);
    filterModeSelector.addItem("Band-pass", 2);
    filterModeSelector.addItem("High-pass", 3);
    addAndMakeVisible(filterModeSelector);
    envelopeCurveSelector.addItem("Linear", 1);
    envelopeCurveSelector.addItem("Smooth", 2);
    envelopeCurveSelector.addItem("Stepped", 3);
    addAndMakeVisible(envelopeCurveSelector);
    automationCurveSelector.addItem("Linear", 1);
    automationCurveSelector.addItem("Smooth", 2);
    automationCurveSelector.addItem("Stepped", 3);
    addAndMakeVisible(automationCurveSelector);
    configureSlider(filterCutoffSlider, kMinFilterCutoffHz, kMaxFilterCutoffHz, 1.0);
    configureSlider(filterResonanceSlider, 0.30, 8.0, 0.01);
    configureSlider(filterEnvelopeSlider, -1.0, 1.0, 0.01);
    configureSlider(macroHardnessSlider, 0.0, 1.0, 0.01);
    configureSlider(macroWeightSlider, 0.0, 1.0, 0.01);
    configureSlider(macroAirSlider, 0.0, 1.0, 0.01);
    configureSlider(macroGritSlider, 0.0, 1.0, 0.01);
    configureSlider(macroSizeSlider, 0.0, 1.0, 0.01);
    configureSlider(sineSlider, 0.0, 1.0, 0.001);
    configureSlider(sawSlider, 0.0, 1.0, 0.001);
    configureSlider(squareSlider, 0.0, 1.0, 0.001);
    configureSlider(triangleSlider, 0.0, 1.0, 0.001);
    configureSlider(noiseSlider, 0.0, 1.0, 0.001);
    configureSlider(probeControlASlider, 0.1, 200.0, 0.1);
    configureSlider(probeControlBSlider, 0.1, 4.0, 0.01);
    configureSlider(probeControlCSlider, 0.1, 4.0, 0.01);
    configureSlider(probeControlDSlider, 0.0, 1.0, 0.01);

    auto wireSliderUndo = [this](juce::Slider& slider, const juce::String& label)
    {
        slider.onDragStart = [this, label]
        {
            beginUndoGesture(label);
        };
        slider.onDragEnd = [this]
        {
            endUndoGesture();
        };
    };

    wireSliderUndo(frequencySlider, "Adjust base frequency");
    wireSliderUndo(durationSlider, "Adjust duration");
    wireSliderUndo(pitchSlider, "Adjust pitch sweep");
    wireSliderUndo(filterCutoffSlider, "Adjust filter cutoff");
    wireSliderUndo(filterResonanceSlider, "Adjust filter resonance");
    wireSliderUndo(filterEnvelopeSlider, "Adjust filter envelope");
    wireSliderUndo(macroHardnessSlider, "Adjust hardness");
    wireSliderUndo(macroWeightSlider, "Adjust weight");
    wireSliderUndo(macroAirSlider, "Adjust air");
    wireSliderUndo(macroGritSlider, "Adjust grit");
    wireSliderUndo(macroSizeSlider, "Adjust size");
    wireSliderUndo(sineSlider, "Adjust sine level");
    wireSliderUndo(sawSlider, "Adjust saw level");
    wireSliderUndo(squareSlider, "Adjust square level");
    wireSliderUndo(triangleSlider, "Adjust triangle level");
    wireSliderUndo(noiseSlider, "Adjust noise level");

    frequencySlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
            graphNodes.getReference(selectedGraphNodeIndex).oscillatorFrequencyHz = (float) frequencySlider.getValue();
        regenerateSignal();
    };
    durationSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.durationSeconds = durationSlider.getValue(); regenerateSignal(); } };
    pitchSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.pitchSweepSemitones = (float) pitchSlider.getValue(); regenerateSignal(); } };
    filterModeSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;

        captureUndoCheckpoint("Change filter type");
        if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
            graphNodes.getReference(selectedGraphNodeIndex).filterMode = filterModeSelector.getSelectedId() == 2 ? "bandpass"
                                                                        : filterModeSelector.getSelectedId() == 3 ? "highpass"
                                                                                                                   : "lowpass";
        regenerateSignal();
    };
    envelopeCurveSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;
        if (selectedGraphNodeIndex < 0 || selectedGraphNodeIndex >= graphNodes.size())
            return;

        captureUndoCheckpoint("Change envelope curve");
        auto& selectedNode = graphNodes.getReference(selectedGraphNodeIndex);
        selectedNode.envelopeCurveMode = envelopeCurveSelector.getSelectedId() == 3 ? "stepped"
                                        : envelopeCurveSelector.getSelectedId() == 1 ? "linear"
                                                                                     : "smooth";
        ensureEnvelopePoints(selectedNode.envelopePoints, selectedNode.envelopeCurveMode);
        for (auto& point : selectedNode.envelopePoints)
            point.curve = selectedNode.envelopeCurveMode;
        regenerateSignal();
    };
    automationCurveSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;

        captureUndoCheckpoint("Change motion curve");
        recipe.automationCurveMode = automationCurveSelector.getSelectedId() == 3 ? "stepped"
                                    : automationCurveSelector.getSelectedId() == 1 ? "linear"
                                                                                   : "smooth";
        for (auto& lane : recipe.automationLanes)
        {
            lane.interpolation = recipe.automationCurveMode;
            for (auto& point : lane.points)
                point.curve = recipe.automationCurveMode;
        }
        regenerateSignal();
    };
    filterCutoffSlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
            graphNodes.getReference(selectedGraphNodeIndex).filterCutoffHz = (float) filterCutoffSlider.getValue();
        regenerateSignal();
    };
    filterResonanceSlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
            graphNodes.getReference(selectedGraphNodeIndex).filterResonance = (float) filterResonanceSlider.getValue();
        regenerateSignal();
    };
    filterEnvelopeSlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
            graphNodes.getReference(selectedGraphNodeIndex).filterEnvelopeAmount = (float) filterEnvelopeSlider.getValue();
        regenerateSignal();
    };
    macroHardnessSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.macroHardness = (float) macroHardnessSlider.getValue(); regenerateSignal(); } };
    macroWeightSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.macroWeight = (float) macroWeightSlider.getValue(); regenerateSignal(); } };
    macroAirSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.macroAir = (float) macroAirSlider.getValue(); regenerateSignal(); } };
    macroGritSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.macroGrit = (float) macroGritSlider.getValue(); regenerateSignal(); } };
    macroSizeSlider.onValueChange = [this] { if (! suppressCallbacks) { noteInteraction(); recipe.macroSize = (float) macroSizeSlider.getValue(); regenerateSignal(); } };
    auto bindOscillatorLevelSlider = [this](juce::Slider& slider)
    {
        slider.onValueChange = [this, &slider]
        {
            if (suppressCallbacks) return;
            noteInteraction();
            if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
                graphNodes.getReference(selectedGraphNodeIndex).oscillatorLevel = (float) slider.getValue();
            regenerateSignal();
        };
    };
    bindOscillatorLevelSlider(sineSlider);
    bindOscillatorLevelSlider(sawSlider);
    bindOscillatorLevelSlider(squareSlider);
    bindOscillatorLevelSlider(triangleSlider);
    bindOscillatorLevelSlider(noiseSlider);
    probeControlASlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && graphNodes.getReference(selectedGraphNodeIndex).type == "scope")
            probeSettings.scopeTimebaseMs = probeControlASlider.getValue();
        else
            probeSettings.analyzerMinHz = probeControlASlider.getValue();
        nodeGraphCanvas.repaint();
    };
    probeControlBSlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && graphNodes.getReference(selectedGraphNodeIndex).type == "scope")
            probeSettings.scopeGainA = probeControlBSlider.getValue();
        else
            probeSettings.analyzerMaxHz = probeControlBSlider.getValue();
        nodeGraphCanvas.repaint();
    };
    probeControlCSlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && graphNodes.getReference(selectedGraphNodeIndex).type == "scope")
            probeSettings.scopeGainB = probeControlCSlider.getValue();
        else
            probeSettings.analyzerDbFloor = probeControlCSlider.getValue();
        nodeGraphCanvas.repaint();
    };
    probeControlDSlider.onValueChange = [this]
    {
        if (suppressCallbacks) return;
        noteInteraction();
        probeSettings.analyzerSmoothing = probeControlDSlider.getValue();
        nodeGraphCanvas.repaint();
    };

    envelopeEditor.onGestureBegin = [this] { beginUndoGesture("Move envelope point"); };
    envelopeEditor.onGestureEnd = [this] { endUndoGesture(); };
    envelopeEditor.onDiscreteEditRequested = [this](const juce::String& label) { captureUndoCheckpoint(label); };

    envelopeEditor.onEnvelopeChanged = [this](const juce::Array<cw::PatchAutomationPoint>& points)
    {
        noteInteraction();
        if (selectedGraphNodeIndex >= 0 && selectedGraphNodeIndex < graphNodes.size())
        {
            auto& selectedNode = graphNodes.getReference(selectedGraphNodeIndex);
            if (selectedNode.type == "envelope")
            {
                selectedNode.envelopePoints = points;
                ensureEnvelopePoints(selectedNode.envelopePoints, selectedNode.envelopeCurveMode);
            }
        }
        regenerateSignal();
    };

    previewButton.onClick = [this]
    {
        ensureAudioRendered();
        if (onPreviewRequested && generatedBuffer.getNumSamples() > 0)
            onPreviewRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    };
    previewButton.setTooltip("Preview the generated signal");
    addAndMakeVisible(previewButton);

    renderButton.onClick = [this]
    {
        ensureAudioRendered();
        if (onRenderRequested && generatedBuffer.getNumSamples() > 0)
            onRenderRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    };
    renderButton.setTooltip("Render the generated signal into the project as an asset");
    addAndMakeVisible(renderButton);

    exportPatchButton.onClick = [this]
    {
        if (onPatchExportRequested == nullptr)
            return;

        auto patchDocument = buildPatchDocument(recipe);
        onPatchExportRequested(cw::serialisePatchDocumentJson(patchDocument), recipe.name);
    };
    exportPatchButton.setTooltip("Export this patch as a file");
    addAndMakeVisible(exportPatchButton);

    savePatchButton.onClick = [this]
    {
        if (onPatchSaveToLibraryRequested == nullptr)
            return;

        auto patchDocument = buildPatchDocument(recipe);
        onPatchSaveToLibraryRequested(cw::serialisePatchDocumentJson(patchDocument), recipe.name);
    };
    savePatchButton.setTooltip("Save this patch to your sound library");
    addAndMakeVisible(savePatchButton);

    loadPatchButton.onClick = [this]
    {
        if (onPatchLoadRequested)
            onPatchLoadRequested();
    };
    loadPatchButton.setTooltip("Load a saved patch from your sound library");
    addAndMakeVisible(loadPatchButton);

    addAutomationLaneButton.onClick = [this]
    {
        captureUndoCheckpoint("Add motion lane");
        auto spec = getAutomationTargetSpecs()[(size_t) (recipe.automationLanes.size() % (int) getAutomationTargetSpecs().size())];
        recipe.automationLanes.add(makeLaneForSpec(spec, recipe.automationCurveMode));
        rebuildAutomationChrome();
        regenerateSignal();
    };
    addAutomationLaneButton.setTooltip("Add another automation lane");
    addAndMakeVisible(addAutomationLaneButton);

    addAndMakeVisible(envelopeEditor);
    automationViewport.setViewedComponent(&automationHost, false);
    automationViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(automationViewport);
    addAndMakeVisible(scopePanel);
    addAndMakeVisible(spectrumPanel);

    previewButton.setVisible(false);
    renderButton.setVisible(false);
    exportPatchButton.setVisible(false);
    savePatchButton.setVisible(false);
    loadPatchButton.setVisible(false);
    templateLabel.setVisible(false);
    templateSelector.setVisible(false);
    addAutomationLaneButton.setVisible(false);
    automationViewport.setVisible(false);

    rebuildLocalControlChrome();
    refreshVariablePanel();
    rebuildAutomationChrome();
    rebuildNodeGraphFromRecipe();
    refreshControlsFromRecipe();
    regenerateSignal();
    graphNodes.clear();
    nodeGraphCanvas.repaint();
    updateCanvasWorkspace();
}

void SignalLabPanel::rebuildNodeGraphFromRecipe()
{
    auto addNode = [this](const juce::String& type, juce::Point<int> position, bool locked, bool required, const juce::String& nodeId = {}, const juce::String& title = {}, const juce::String& payload = {})
    {
        GraphNodeModel node;
        node.id = nodeId.isNotEmpty() ? nodeId : type + juce::String(graphNodes.size() + 1);
        node.type = type;
        node.title = title.isNotEmpty() ? title : graphNodeTitle(type);
        node.targetParameter = payload;
        node.position = position;
        node.accent = graphNodeAccent(type);
        node.locked = locked;
        node.required = required;
        graphNodes.add(node);
    };

    juce::HashMap<juce::String, juce::Point<int>> priorPositions;
    for (auto& node : graphNodes)
        priorPositions.set(node.id, node.position);

    // Get/Set variable nodes are explicitly placed by the user (drag from
    // the Variables panel, or the right-click menu) -- not auto-generated
    // per variable the way the old "value" node used to be. This rebuild
    // still clears and re-derives everything else from the recipe/flags,
    // so preserve any already-placed getter/setter nodes verbatim across
    // it, dropping only ones whose variable was deleted. Oscillator/noise
    // source nodes are preserved the same way -- each instance owns its own
    // level/frequency now (GraphNodeModel::oscillatorLevel/oscillatorFrequencyHz),
    // so there's nothing to re-derive them from; the old single-node-per-type
    // recipe.XLevel scalars are only used as a one-shot seed by
    // seedOscillatorNodesFromRecipeLevels() (templates, patch load), never
    // read here.
    juce::Array<GraphNodeModel> preservedValueNodes;
    juce::Array<GraphNodeModel> preservedOscillatorNodes;
    juce::Array<GraphNodeModel> preservedFilterEnvelopeNodes;
    juce::Array<GraphNodeModel> preservedMidiControlNodes;
    juce::Array<float> preservedMixerInputVolumes { 1.0f, 1.0f };
    for (auto& node : graphNodes)
    {
        if (node.type == "valueGet" || node.type == "valueSet")
            preservedValueNodes.add(node);
        else if (node.type == "mix")
            preservedMixerInputVolumes = node.mixerInputVolumes;
        else if (node.type == "sine" || node.type == "saw" || node.type == "square"
                 || node.type == "triangle" || node.type == "noise")
            preservedOscillatorNodes.add(node);
        else if (node.type == "filter" || node.type == "envelope")
            preservedFilterEnvelopeNodes.add(node);
        else if (node.type == "midiFader" || node.type == "midiButton")
            preservedMidiControlNodes.add(node);
    }

    graphNodes.clear();

    for (auto& preserved : preservedOscillatorNodes)
        graphNodes.add(preserved);

    // Filter/Envelope are real independent instances now too (multiple can
    // exist, each with its own settings) -- preserved verbatim like
    // oscillators, not re-derived from the old filterNodeEnabled/
    // envelopeNodeEnabled singleton flags.
    for (auto& preserved : preservedFilterEnvelopeNodes)
        graphNodes.add(preserved);

    // MIDI Control nodes carry a learned hardware binding that took real
    // user effort to set up -- preserved verbatim like everything else
    // above, never re-derived from anything.
    for (auto& preserved : preservedMidiControlNodes)
        graphNodes.add(preserved);

    for (auto& preserved : preservedValueNodes)
    {
        bool variableStillExists = false;
        for (auto& variable : localControls)
            if (variable.id == preserved.targetParameter)
                variableStillExists = true;
        if (variableStillExists)
            graphNodes.add(preserved);
    }

    if (mixNodeEnabled)
    {
        addNode("mix", priorPositions.contains("mix") ? priorPositions["mix"] : juce::Point<int>(470, 180), false, false, "mix");
        graphNodes.getReference(graphNodes.size() - 1).mixerInputVolumes = preservedMixerInputVolumes;
    }
    addNode("output", priorPositions.contains("output") ? priorPositions["output"] : juce::Point<int>(1100, 180), false, true, "output");
    if (probeSettings.scopeEnabled)
        addNode("scope", priorPositions.contains("scope") ? priorPositions["scope"] : juce::Point<int>(890, 36), false, false, "scope");
    if (probeSettings.analyzerEnabled)
        addNode("analyzer", priorPositions.contains("analyzer") ? priorPositions["analyzer"] : juce::Point<int>(890, 320), false, false, "analyzer");

    if (selectedGraphNodeIndex >= graphNodes.size())
        selectedGraphNodeIndex = graphNodes.isEmpty() ? -1 : 0;

    auto nodeStillExists = [this](const juce::String& nodeId)
    {
        for (auto& node : graphNodes)
            if (node.id == nodeId)
                return true;
        return false;
    };
    for (int index = graphConnections.size() - 1; index >= 0; --index)
    {
        auto& connection = graphConnections.getReference(index);
        if (! nodeStillExists(connection.fromNodeId) || ! nodeStillExists(connection.toNodeId))
            graphConnections.remove(index);
    }
    if (selectedConnectionIndex >= graphConnections.size())
        selectedConnectionIndex = -1;
}

// One-shot conversion from the legacy single-instance recipe.XLevel scalars
// into a fresh set of oscillator graph nodes -- used only by operations that
// replace the whole sound wholesale (applying an AI template, loading a
// saved patch), where those scalars are the only source of truth available.
// Regular editing never calls this; rebuildNodeGraphFromRecipe() preserves
// whatever oscillator nodes already exist instead.
void SignalLabPanel::seedOscillatorNodesFromRecipeLevels()
{
    for (int index = graphNodes.size() - 1; index >= 0; --index)
    {
        auto& node = graphNodes.getReference(index);
        if (node.type == "sine" || node.type == "saw" || node.type == "square"
            || node.type == "triangle" || node.type == "noise")
            graphNodes.remove(index);
    }

    int sourceY = 36;
    auto addSourceIfActive = [&](const juce::String& type, float level)
    {
        if (level <= 0.0f)
            return;
        GraphNodeModel node;
        node.id = type + ":" + juce::String(juce::Random::getSystemRandom().nextInt64());
        node.type = type;
        node.title = graphNodeTitle(type);
        node.position = { 36, sourceY };
        node.accent = graphNodeAccent(type);
        node.oscillatorLevel = level;
        node.oscillatorFrequencyHz = recipe.baseFrequencyHz;
        graphNodes.add(node);
        sourceY += 110;
    };

    addSourceIfActive("sine", recipe.sineLevel);
    addSourceIfActive("saw", recipe.sawLevel);
    addSourceIfActive("square", recipe.squareLevel);
    addSourceIfActive("triangle", recipe.triangleLevel);
    addSourceIfActive("noise", recipe.noiseLevel);
}

void SignalLabPanel::updateInspectorForSelection()
{
    auto hideAll = [this]()
    {
        frequencyLabel.setVisible(false); frequencySlider.setVisible(false);
        durationLabel.setVisible(false); durationSlider.setVisible(false);
        pitchLabel.setVisible(false); pitchSlider.setVisible(false);
        filterModeLabel.setVisible(false); filterModeSelector.setVisible(false);
        filterCutoffLabel.setVisible(false); filterCutoffSlider.setVisible(false);
        filterResonanceLabel.setVisible(false); filterResonanceSlider.setVisible(false);
        filterEnvelopeLabel.setVisible(false); filterEnvelopeSlider.setVisible(false);
        envelopeCurveLabel.setVisible(false); envelopeCurveSelector.setVisible(false);
        automationCurveLabel.setVisible(false); automationCurveSelector.setVisible(false);
        macroHardnessLabel.setVisible(false); macroHardnessSlider.setVisible(false);
        macroWeightLabel.setVisible(false); macroWeightSlider.setVisible(false);
        macroAirLabel.setVisible(false); macroAirSlider.setVisible(false);
        macroGritLabel.setVisible(false); macroGritSlider.setVisible(false);
        macroSizeLabel.setVisible(false); macroSizeSlider.setVisible(false);
        sineLabel.setVisible(false); sineSlider.setVisible(false);
        sawLabel.setVisible(false); sawSlider.setVisible(false);
        squareLabel.setVisible(false); squareSlider.setVisible(false);
        triangleLabel.setVisible(false); triangleSlider.setVisible(false);
        noiseLabel.setVisible(false); noiseSlider.setVisible(false);
        probeControlALabel.setVisible(false); probeControlASlider.setVisible(false);
        probeControlBLabel.setVisible(false); probeControlBSlider.setVisible(false);
        probeControlCLabel.setVisible(false); probeControlCSlider.setVisible(false);
        probeControlDLabel.setVisible(false); probeControlDSlider.setVisible(false);
        envelopeEditor.setVisible(false);
        scopePanel.setVisible(false);
        spectrumPanel.setVisible(false);
    };

    hideAll();

    if (selectedGraphNodeIndex < 0 || selectedGraphNodeIndex >= graphNodes.size())
    {
        inspectorTitleLabel.setText("No node selected", juce::dontSendNotification);
        inspectorBodyLabel.setText("Right-click the canvas to add source nodes, then click a node to shape the sound.", juce::dontSendNotification);
        resized();
        repaint();
        return;
    }

    auto type = graphNodes.getReference(selectedGraphNodeIndex).type;
    inspectorTitleLabel.setText(graphNodes.getReference(selectedGraphNodeIndex).title, juce::dontSendNotification);

    if (type == "sine" || type == "saw" || type == "square" || type == "triangle" || type == "noise")
    {
        inspectorBodyLabel.setText("Source node. Each instance has its own frequency and level.", juce::dontSendNotification);
        auto& selectedNode = graphNodes.getReference(selectedGraphNodeIndex);

        suppressCallbacks = true;
        frequencyLabel.setVisible(true); frequencySlider.setVisible(true);
        frequencySlider.setValue(selectedNode.oscillatorFrequencyHz, juce::dontSendNotification);
        frequencySlider.setEnabled(true); // no port for frequency yet -- always manual

        double wiredLevel = 0.0;
        auto isLevelWired = findWiredParameterValue(selectedNode.id, "param:" + type + "Level", wiredLevel);
        auto levelToShow = isLevelWired ? (float) wiredLevel : selectedNode.oscillatorLevel;
        if (type == "sine") { sineLabel.setVisible(true); sineSlider.setVisible(true); sineSlider.setValue(levelToShow, juce::dontSendNotification); sineSlider.setEnabled(! isLevelWired); }
        else if (type == "saw") { sawLabel.setVisible(true); sawSlider.setVisible(true); sawSlider.setValue(levelToShow, juce::dontSendNotification); sawSlider.setEnabled(! isLevelWired); }
        else if (type == "square") { squareLabel.setVisible(true); squareSlider.setVisible(true); squareSlider.setValue(levelToShow, juce::dontSendNotification); squareSlider.setEnabled(! isLevelWired); }
        else if (type == "triangle") { triangleLabel.setVisible(true); triangleSlider.setVisible(true); triangleSlider.setValue(levelToShow, juce::dontSendNotification); triangleSlider.setEnabled(! isLevelWired); }
        else if (type == "noise") { noiseLabel.setVisible(true); noiseSlider.setVisible(true); noiseSlider.setValue(levelToShow, juce::dontSendNotification); noiseSlider.setEnabled(! isLevelWired); }
        suppressCallbacks = false;
    }
    else if (type == "filter")
    {
        inspectorBodyLabel.setText("Tone-shaping stage. Position in the chain now comes from how it's wired, not a fixed slot.", juce::dontSendNotification);
        auto& filterNode = graphNodes.getReference(selectedGraphNodeIndex);
        double wiredValue = 0.0;

        suppressCallbacks = true;
        filterModeLabel.setVisible(true); filterModeSelector.setVisible(true);
        filterModeSelector.setSelectedId(filterNode.filterMode == "bandpass" ? 2 : filterNode.filterMode == "highpass" ? 3 : 1, juce::dontSendNotification);

        filterCutoffLabel.setVisible(true); filterCutoffSlider.setVisible(true);
        auto cutoffWired = findWiredParameterValue(filterNode.id, "param:filterCutoff", wiredValue);
        filterCutoffSlider.setValue(cutoffWired ? wiredValue : (double) filterNode.filterCutoffHz, juce::dontSendNotification);
        filterCutoffSlider.setEnabled(! cutoffWired);

        filterResonanceLabel.setVisible(true); filterResonanceSlider.setVisible(true);
        auto resonanceWired = findWiredParameterValue(filterNode.id, "param:filterResonance", wiredValue);
        filterResonanceSlider.setValue(resonanceWired ? wiredValue : (double) filterNode.filterResonance, juce::dontSendNotification);
        filterResonanceSlider.setEnabled(! resonanceWired);

        filterEnvelopeLabel.setVisible(true); filterEnvelopeSlider.setVisible(true);
        auto envAmountWired = findWiredParameterValue(filterNode.id, "param:filterEnvelopeAmount", wiredValue);
        filterEnvelopeSlider.setValue(envAmountWired ? wiredValue : (double) filterNode.filterEnvelopeAmount, juce::dontSendNotification);
        filterEnvelopeSlider.setEnabled(! envAmountWired);
        suppressCallbacks = false;
    }
    else if (type == "envelope")
    {
        inspectorBodyLabel.setText("Amplitude contour for the rendered sound. Drag points directly in the envelope view.", juce::dontSendNotification);
        auto& envelopeNode = graphNodes.getReference(selectedGraphNodeIndex);

        suppressCallbacks = true;
        envelopeCurveLabel.setVisible(true); envelopeCurveSelector.setVisible(true);
        envelopeCurveSelector.setSelectedId(envelopeNode.envelopeCurveMode == "stepped" ? 3 : envelopeNode.envelopeCurveMode == "linear" ? 1 : 2, juce::dontSendNotification);
        suppressCallbacks = false;

        envelopeEditor.setVisible(true);
        SignalRecipe envelopeView;
        envelopeView.envelopeCurveMode = envelopeNode.envelopeCurveMode;
        envelopeView.envelopePoints = envelopeNode.envelopePoints;
        envelopeEditor.setRecipe(envelopeView);
    }
    else if (type == "output")
    {
        inspectorBodyLabel.setText("Final sound settings and character macros before preview or render.", juce::dontSendNotification);
        durationLabel.setVisible(true); durationSlider.setVisible(true);
        pitchLabel.setVisible(true); pitchSlider.setVisible(true);
        macroHardnessLabel.setVisible(true); macroHardnessSlider.setVisible(true);
        macroWeightLabel.setVisible(true); macroWeightSlider.setVisible(true);
        macroAirLabel.setVisible(true); macroAirSlider.setVisible(true);
        macroGritLabel.setVisible(true); macroGritSlider.setVisible(true);
        macroSizeLabel.setVisible(true); macroSizeSlider.setVisible(true);
    }
    else if (type == "scope")
    {
        inspectorBodyLabel.setText("Dual-trace oscilloscope probe node. For now it previews the current rendered stereo signal inline and here; next pass will add attachable probe targets and detachable full instrument windows.", juce::dontSendNotification);
        probeControlALabel.setText("Timebase (ms)", juce::dontSendNotification);
        probeControlBLabel.setText("Trace A Gain", juce::dontSendNotification);
        probeControlCLabel.setText("Trace B Gain", juce::dontSendNotification);
        probeControlDLabel.setText("Analyzer Smooth", juce::dontSendNotification);
        probeControlASlider.setRange(0.5, 250.0, 0.1);
        probeControlBSlider.setRange(0.1, 4.0, 0.01);
        probeControlCSlider.setRange(0.1, 4.0, 0.01);
        probeControlDSlider.setRange(0.0, 1.0, 0.01);
        probeControlASlider.setValue(probeSettings.scopeTimebaseMs, juce::dontSendNotification);
        probeControlBSlider.setValue(probeSettings.scopeGainA, juce::dontSendNotification);
        probeControlCSlider.setValue(probeSettings.scopeGainB, juce::dontSendNotification);
        probeControlDSlider.setValue(probeSettings.analyzerSmoothing, juce::dontSendNotification);
        probeControlALabel.setVisible(true); probeControlASlider.setVisible(true);
        probeControlBLabel.setVisible(true); probeControlBSlider.setVisible(true);
        probeControlCLabel.setVisible(true); probeControlCSlider.setVisible(true);
        scopePanel.setVisible(true);
    }
    else if (type == "analyzer")
    {
        inspectorBodyLabel.setText("Frequency analyzer probe node with adjustable range and floor. This is the first node-based analyzer pass; detachable pro window comes next.", juce::dontSendNotification);
        probeControlALabel.setText("Min Hz", juce::dontSendNotification);
        probeControlBLabel.setText("Max Hz", juce::dontSendNotification);
        probeControlCLabel.setText("dB Floor", juce::dontSendNotification);
        probeControlDLabel.setText("Smoothing", juce::dontSendNotification);
        probeControlASlider.setRange(10.0, 2000.0, 1.0);
        probeControlBSlider.setRange(500.0, 24000.0, 1.0);
        probeControlCSlider.setRange(-120.0, -12.0, 1.0);
        probeControlDSlider.setRange(0.0, 1.0, 0.01);
        probeControlASlider.setValue(probeSettings.analyzerMinHz, juce::dontSendNotification);
        probeControlBSlider.setValue(probeSettings.analyzerMaxHz, juce::dontSendNotification);
        probeControlCSlider.setValue(probeSettings.analyzerDbFloor, juce::dontSendNotification);
        probeControlDSlider.setValue(probeSettings.analyzerSmoothing, juce::dontSendNotification);
        probeControlALabel.setVisible(true); probeControlASlider.setVisible(true);
        probeControlBLabel.setVisible(true); probeControlBSlider.setVisible(true);
        probeControlCLabel.setVisible(true); probeControlCSlider.setVisible(true);
        probeControlDLabel.setVisible(true); probeControlDSlider.setVisible(true);
        spectrumPanel.setVisible(true);
    }
    else
    {
        inspectorBodyLabel.setText("Mixer node. Active source nodes feed here automatically.", juce::dontSendNotification);
    }

    resized();
    repaint();
}

void SignalLabPanel::showCanvasActionMenu(juce::Point<int> canvasPosition, bool anchorToButton)
{
    juce::Array<NodeSearchPanel::Entry> entries;
    entries.add({ "Sine Oscillator", "sine", {} });
    entries.add({ "Saw Oscillator", "saw", {} });
    entries.add({ "Square Oscillator", "square", {} });
    entries.add({ "Triangle Oscillator", "triangle", {} });
    entries.add({ "Noise", "noise", {} });
    entries.add({ "Mixer", "mix", {} });
    entries.add({ "Filter", "filter", {} });
    entries.add({ "Envelope", "envelope", {} });
    entries.add({ "Sink", "output", {} });
    entries.add({ "Timeline", "timeline", {} });
    entries.add({ "Oscilloscope", "scope", {} });
    entries.add({ "Frequency Analyzer", "analyzer", {} });
    entries.add({ "Fader Control", "midiFader", {} });
    entries.add({ "Button Control", "midiButton", {} });
    for (auto& variable : localControls)
    {
        entries.add({ "Get " + variable.name, "valueGet", variable.id });
        if (! hasSetterNodeForVariable(variable.id))
            entries.add({ "Set " + variable.name, "valueSet", variable.id });
    }

    auto panel = std::make_unique<NodeSearchPanel>();
    panel->setEntries(entries);
    panel->setSize(180, juce::jmin(400, 44 + entries.size() * 22));
    panel->onEntryChosen = [this, canvasPosition](const juce::String& type, const juce::String& payload)
    {
        addGraphNode(type, canvasPosition, payload);
    };

    auto* rawPanel = panel.get();
    auto area = nodeGraphCanvas.localAreaToGlobal(juce::Rectangle<int>(canvasPosition.x, canvasPosition.y, 1, 1));
    auto& callout = juce::CallOutBox::launchAsynchronously(std::move(panel), area, nullptr);
    auto* calloutPtr = &callout;
    rawPanel->onDismissRequested = [calloutPtr]
    {
        calloutPtr->setVisible(false);
        calloutPtr->dismiss();
    };
}

void SignalLabPanel::showNodeContextMenu(int nodeIndex, juce::Point<int> canvasPosition)
{
    if (nodeIndex < 0 || nodeIndex >= graphNodes.size())
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Open");
    menu.addItem(2, "Delete", ! graphNodes.getReference(nodeIndex).required);

    auto area = nodeGraphCanvas.localAreaToGlobal(juce::Rectangle<int>(canvasPosition.x, canvasPosition.y, 1, 1));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
                       [this, nodeIndex](int result)
                       {
                           if (nodeIndex < 0 || nodeIndex >= graphNodes.size())
                               return;

                           setSelectedGraphNodeIndex(nodeIndex);

                           if (result == 1)
                               openNodeEditorForSelection();
                           else if (result == 2)
                               removeSelectedGraphNode();
                       });
}

void SignalLabPanel::showSignalMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "New");
    menu.addItem(2, "Open");
    menu.addItem(3, "Save");
    menu.addItem(4, "Save As");
    menu.addItem(5, "Render to Project");

    auto area = signalMenuButton.getScreenBounds();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
                       [this](int result)
                       {
                           if (result == 1)
                               createNewSignal();
                           else if (result == 2)
                           {
                               if (onPatchLoadRequested)
                                   onPatchLoadRequested();
                           }
                           else if (result == 3 || result == 4)
                           {
                               if (onPatchSaveToLibraryRequested)
                                   onPatchSaveToLibraryRequested(cw::serialisePatchDocumentJson(buildPatchDocument(recipe)), recipe.name);
                           }
                           else if (result == 5)
                           {
                               ensureAudioRendered();
                               if (onRenderRequested)
                                   onRenderRequested(generatedBuffer, recipe.sampleRate, recipe.name);
                           }
                       });
}

void SignalLabPanel::createNewSignal()
{
    stopTransport();
    recipe = {};
    mixNodeEnabled = false;
    filterNodeEnabled = false;
    envelopeNodeEnabled = false;
    probeSettings = {};
    localControls.clear();
    selectedLocalControlIndex = -1;
    rebuildLocalControlChrome();
    refreshVariablePanel();
    refreshControlsFromRecipe();
    regenerateSignal();
    graphNodes.clear();
    nodeGraphCanvas.repaint();
}

void SignalLabPanel::addGraphNode(const juce::String& type, juce::Point<int> canvasPosition, const juce::String& payload)
{
    // Get nodes are explicitly-placed, and multiple instances of the same
    // variable are allowed (UE4-style: drop a fresh "Get" wherever you need
    // the value instead of routing one long wire) -- each placement always
    // creates a new node, never finds-and-repositions an existing one.
    if (type == "valueGet")
    {
        captureUndoCheckpoint("Add Get " + payload);
        GraphNodeModel node;
        node.id = "valueGet:" + payload + ":" + juce::String(juce::Random::getSystemRandom().nextInt64());
        node.type = "valueGet";
        for (auto& variable : localControls)
            if (variable.id == payload)
                node.title = "Get " + variable.name;
        node.targetParameter = payload;
        node.position = canvasToGraph(canvasPosition);
        node.accent = graphNodeAccent(type);
        graphNodes.add(node);
        setSelectedGraphNodeIndex(graphNodes.size() - 1);
        return;
    }

    // Set nodes: only one allowed per variable (the single driven instance).
    // If one already exists, just reposition/select it rather than adding
    // a second -- the right-click menu already omits "Set" once one
    // exists, this is the defensive fallback for any other call path.
    if (type == "valueSet")
    {
        for (int index = 0; index < graphNodes.size(); ++index)
        {
            if (graphNodes.getReference(index).type == "valueSet" && graphNodes.getReference(index).targetParameter == payload)
            {
                if (canvasPosition != juce::Point<int>())
                    graphNodes.getReference(index).position = canvasToGraph(canvasPosition);
                setSelectedGraphNodeIndex(index);
                return;
            }
        }

        captureUndoCheckpoint("Add Set " + payload);
        GraphNodeModel node;
        node.id = "valueSet:" + payload;
        node.type = "valueSet";
        for (auto& variable : localControls)
            if (variable.id == payload)
                node.title = "Set " + variable.name;
        node.targetParameter = payload;
        node.position = canvasToGraph(canvasPosition);
        node.accent = graphNodeAccent(type);
        graphNodes.add(node);
        setSelectedGraphNodeIndex(graphNodes.size() - 1);
        return;
    }

    // Oscillator/noise sources: each placement always creates a new,
    // independent instance (same reasoning as Get nodes above) -- each one
    // owns its own level and frequency, so there's no shared slot to guard
    // against a duplicate of.
    if (type == "sine" || type == "saw" || type == "square" || type == "triangle" || type == "noise")
    {
        captureUndoCheckpoint("Add " + graphNodeTitle(type));
        GraphNodeModel node;
        node.id = type + ":" + juce::String(juce::Random::getSystemRandom().nextInt64());
        node.type = type;
        node.title = graphNodeTitle(type);
        node.position = canvasToGraph(canvasPosition);
        node.accent = graphNodeAccent(type);
        node.oscillatorFrequencyHz = recipe.baseFrequencyHz;
        node.oscillatorLevel = type == "sine" ? 0.65f
                              : type == "saw" ? 0.20f
                              : type == "square" ? 0.15f
                              : type == "triangle" ? 0.20f
                                                    : 0.10f;
        graphNodes.add(node);
        setSelectedGraphNodeIndex(graphNodes.size() - 1);
        regenerateSignal();
        return;
    }

    // Filter/Envelope: same reasoning as oscillators -- each placement
    // creates a new, independent instance with its own settings, so
    // multiple Filters (or Envelopes) in different chain positions don't
    // fight over one shared slot.
    if (type == "filter" || type == "envelope")
    {
        captureUndoCheckpoint("Add " + graphNodeTitle(type));
        GraphNodeModel node;
        node.id = type + ":" + juce::String(juce::Random::getSystemRandom().nextInt64());
        node.type = type;
        node.title = graphNodeTitle(type);
        node.position = canvasToGraph(canvasPosition);
        node.accent = graphNodeAccent(type);
        if (type == "envelope")
            node.envelopePoints = makeDefaultEnvelopePoints(node.envelopeCurveMode);
        graphNodes.add(node);
        setSelectedGraphNodeIndex(graphNodes.size() - 1);
        regenerateSignal();
        return;
    }

    // MIDI Control nodes: same reasoning as oscillators/filter/envelope --
    // you may want several physical controls bound to different parameters,
    // so each placement is its own independent instance, unlearned until
    // the user opens it and hits Learn.
    if (type == "midiFader" || type == "midiButton")
    {
        captureUndoCheckpoint("Add " + graphNodeTitle(type));
        GraphNodeModel node;
        node.id = type + ":" + juce::String(juce::Random::getSystemRandom().nextInt64());
        node.type = type;
        node.title = graphNodeTitle(type);
        node.position = canvasToGraph(canvasPosition);
        node.accent = graphNodeAccent(type);
        graphNodes.add(node);
        setSelectedGraphNodeIndex(graphNodes.size() - 1);
        regenerateSignal();
        return;
    }

    if (hasGraphNodeType(type) && type != "mix")
        return;

    captureUndoCheckpoint("Add " + graphNodeTitle(type));

    if (type == "mix") mixNodeEnabled = true;
    else if (type == "scope") probeSettings.scopeEnabled = true;
    else if (type == "analyzer") probeSettings.analyzerEnabled = true;

    regenerateSignal();

    for (int index = 0; index < graphNodes.size(); ++index)
    {
        if (graphNodes.getReference(index).type == type)
        {
            if (canvasPosition != juce::Point<int>())
                graphNodes.getReference(index).position = canvasToGraph(canvasPosition);
            setSelectedGraphNodeIndex(index);
            break;
        }
    }
}

void SignalLabPanel::removeSelectedGraphNode()
{
    if (selectedGraphNodeIndex < 0 || selectedGraphNodeIndex >= graphNodes.size())
        return;

    auto type = graphNodes.getReference(selectedGraphNodeIndex).type;
    if (graphNodes.getReference(selectedGraphNodeIndex).required)
        return;

    captureUndoCheckpoint("Remove " + graphNodeTitle(type));

    // Oscillator/noise nodes are independent instances now (no shared
    // recipe.XLevel slot to clear) -- rebuildNodeGraphFromRecipe() preserves
    // whatever's already in graphNodes for these types, so this one has to
    // be removed from the array directly rather than gated off by a flag.
    if (type == "sine" || type == "saw" || type == "square" || type == "triangle" || type == "noise"
        || type == "filter" || type == "envelope" || type == "midiFader" || type == "midiButton")
        graphNodes.remove(selectedGraphNodeIndex);
    else if (type == "mix") mixNodeEnabled = false;
    else if (type == "scope") probeSettings.scopeEnabled = false;
    else if (type == "analyzer") probeSettings.analyzerEnabled = false;

    selectedGraphNodeIndex = -1;
    regenerateSignal();
}

int SignalLabPanel::findGraphNodeAt(juce::Point<int> position) const
{
    for (int index = graphNodes.size() - 1; index >= 0; --index)
        if (graphToCanvas(getGraphNodeBounds(index)).contains(position))
            return index;
    return -1;
}

juce::Rectangle<int> SignalLabPanel::getGraphNodeBounds(int index) const
{
    if (index < 0 || index >= graphNodes.size())
        return {};
    auto& node = graphNodes.getReference(index);
    return { node.position.x, node.position.y, 180, getGraphNodeHeight(index) };
}

int SignalLabPanel::getGraphNodeHeight(int index) const
{
    if (index < 0 || index >= graphNodes.size())
        return 96;

    auto& node = graphNodes.getReference(index);

    // Node body grows to fit however many stacked left-edge parameter ports
    // it has, plus room for a type-specific inline preview (e.g. the scope
    // node's live mini-trace, the analyzer's mini spectrum bars). Full
    // instrument detail still lives in the detachable floating window
    // opened via double-click -- this inline area is a compact preview only.
    int leftPortRows = nodeParameterIds(node.type).size();
    if (node.type == "mix")
        leftPortRows += node.mixerInputVolumes.size() * 2 + 2; // signal+weight pair per input, plus room for the "+ Input" button

    bool hasSignalIn = node.type != "sine" && node.type != "saw" && node.type != "square"
                     && node.type != "triangle" && node.type != "noise" && node.type != "timeline"
                     && node.type != "value" && node.type != "valueGet" && node.type != "valueSet"
                     && node.type != "mix" && node.type != "midiFader" && node.type != "midiButton";
    bool hasSignalOut = node.type != "output" && node.type != "valueGet" && node.type != "valueSet"
                      && node.type != "midiFader" && node.type != "midiButton";
    if (hasSignalIn || hasSignalOut)
        leftPortRows += 1; // signal ports now get their own row under the header

    int height = 96;
    if (leftPortRows > 2)
        height += (leftPortRows - 2) * 22;
    if (node.type == "scope" || node.type == "analyzer")
        height = juce::jmax(height, 118);

    return height;
}

juce::Rectangle<int> SignalLabPanel::getMixerAddInputButtonBounds(int index) const
{
    if (index < 0 || index >= graphNodes.size())
        return {};
    auto bounds = getGraphNodeBounds(index);
    auto& node = graphNodes.getReference(index);
    auto y = bounds.getY() + 30 + node.mixerInputVolumes.size() * 44;
    return { bounds.getX() + 10, y, bounds.getWidth() - 20, 20 };
}

void SignalLabPanel::setSelectedGraphNodeIndex(int index)
{
    selectedGraphNodeIndex = index;
    if (nodeEditorVisible)
        editingNodeIndex = index;
    updateInspectorForSelection();
    layoutFloatingWindows();
    nodeGraphCanvas.repaint();
}

bool SignalLabPanel::hasGraphNodeType(const juce::String& type) const
{
    for (auto& node : graphNodes)
        if (node.type == type)
            return true;
    return false;
}

bool SignalLabPanel::hasSetterNodeForVariable(const juce::String& variableId) const
{
    for (auto& node : graphNodes)
        if (node.type == "valueSet" && node.targetParameter == variableId)
            return true;
    return false;
}

// Value-port wiring (a Get-variable node's "valueOut" wired into a
// parameter port -- Mixer Weight, Filter Cutoff/Resonance/EnvAmt,
// oscillator Level) was previously decorative: the wire could be drawn but
// nothing downstream ever read it, so the manual knob value always won.
// This resolves whatever's actually wired in, if anything, so callers can
// use it in place of the manual value and disable the manual control.
bool SignalLabPanel::findWiredParameterValue(const juce::String& nodeId, const juce::String& portId, double& outValue) const
{
    for (auto& connection : graphConnections)
    {
        if (connection.isExec || connection.toNodeId != nodeId || connection.toPortId != portId)
            continue;

        for (auto& sourceNode : graphNodes)
        {
            if (sourceNode.id != connection.fromNodeId)
                continue;

            if (sourceNode.type == "valueGet")
            {
                for (auto& variable : localControls)
                {
                    if (variable.id != sourceNode.targetParameter)
                        continue;
                    outValue = variable.value;
                    return true;
                }
            }
            else if (sourceNode.type == "midiFader" || sourceNode.type == "midiButton")
            {
                // A MIDI Control node IS a value source, same role as a
                // Get-variable node above -- just driven by hardware instead
                // of a manually-set variable. This one function change is
                // what makes MIDI Control nodes work everywhere a parameter
                // port already resolves wired values (oscillator level,
                // filter cutoff/resonance, mixer weight) with no other
                // changes needed at any of those call sites.
                outValue = sourceNode.midiLiveValue;
                return true;
            }
        }
    }
    return false;
}

// Sibling to findWiredParameterValue above, for the live engine: it needs
// to know *which* MIDI Control node id is driving a port (to register a
// live atomic slot for it in PatchLiveVoice), not just its current value.
// Empty string if the port isn't wired to a MIDI Control node (unwired, or
// wired to a Get-variable instead -- variables aren't live yet, see the
// CEL/control-graph roadmap note).
juce::String SignalLabPanel::findWiredMidiSourceNodeId(const juce::String& nodeId, const juce::String& portId) const
{
    for (auto& connection : graphConnections)
    {
        if (connection.isExec || connection.toNodeId != nodeId || connection.toPortId != portId)
            continue;

        for (auto& sourceNode : graphNodes)
            if (sourceNode.id == connection.fromNodeId && (sourceNode.type == "midiFader" || sourceNode.type == "midiButton"))
                return sourceNode.id;
    }
    return {};
}

void SignalLabPanel::addMixerInput(int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= graphNodes.size())
        return;
    auto& node = graphNodes.getReference(nodeIndex);
    if (node.type != "mix")
        return;

    captureUndoCheckpoint("Add mixer input");
    node.mixerInputVolumes.add(1.0f);
    updateCanvasWorkspace();
}

void SignalLabPanel::openNodeEditorForSelection()
{
    if (selectedGraphNodeIndex < 0 || selectedGraphNodeIndex >= graphNodes.size())
        return;

    auto node = graphNodes.getReference(selectedGraphNodeIndex);

    for (int index = 0; index < openNodeWindows.size(); ++index)
    {
        auto* entry = openNodeWindows[index];
        if (entry != nullptr && entry->nodeId == node.id && entry->window != nullptr)
        {
            entry->window->setVisible(true);
            entry->window->toFront(true);
            return;
        }
    }

    auto content = std::make_unique<SimpleNodeEditorContent>();
    content->addTextBlock(node.title, "Node tool window");

    if (node.type == "sine" || node.type == "saw" || node.type == "square" || node.type == "triangle" || node.type == "noise")
    {
        // Each oscillator instance owns its own frequency/level now (that's
        // the whole point -- two Sine nodes must not edit the same value),
        // so look the node up by id at callback time rather than binding to
        // the shared recipe or a captured index that could go stale if
        // nodes are added/removed while this window stays open.
        auto nodeId = node.id;
        auto findNode = [this, nodeId]() -> GraphNodeModel*
        {
            for (auto& candidate : graphNodes)
                if (candidate.id == nodeId)
                    return &candidate;
            return nullptr;
        };

        auto& freq = content->addSliderRow("Frequency", 30.0, 2400.0, 1.0);
        freq.setValue(node.oscillatorFrequencyHz, juce::dontSendNotification);
        freq.onValueChange = [this, &freq, findNode]
        {
            if (auto* target = findNode())
                target->oscillatorFrequencyHz = (float) freq.getValue();
            regenerateSignal();
        };

        juce::Slider* level = nullptr;
        if (node.type == "sine") level = &content->addSliderRow("Sine Level", 0.0, 1.0, 0.001);
        else if (node.type == "saw") level = &content->addSliderRow("Saw Level", 0.0, 1.0, 0.001);
        else if (node.type == "square") level = &content->addSliderRow("Square Level", 0.0, 1.0, 0.001);
        else if (node.type == "triangle") level = &content->addSliderRow("Triangle Level", 0.0, 1.0, 0.001);
        else if (node.type == "noise") level = &content->addSliderRow("Noise Level", 0.0, 1.0, 0.001);

        if (level != nullptr)
        {
            double wiredLevel = 0.0;
            if (findWiredParameterValue(node.id, "param:" + node.type + "Level", wiredLevel))
            {
                level->setValue(wiredLevel, juce::dontSendNotification);
                level->setEnabled(false);
            }
            else
            {
                level->setValue(node.oscillatorLevel, juce::dontSendNotification);
                level->onValueChange = [this, level, findNode]
                {
                    if (auto* target = findNode())
                        target->oscillatorLevel = (float) level->getValue();
                    regenerateSignal();
                };
            }
        }
    }
    else if (node.type == "filter")
    {
        // Each Filter instance owns its own mode/cutoff/resonance now --
        // same reasoning and pattern as oscillators above.
        auto nodeId = node.id;
        auto findFilterNode = [this, nodeId]() -> GraphNodeModel*
        {
            for (auto& candidate : graphNodes)
                if (candidate.id == nodeId)
                    return &candidate;
            return nullptr;
        };

        auto& mode = content->addComboRow("Filter Type", { "Low-pass", "Band-pass", "High-pass" });
        mode.setSelectedId(node.filterMode == "bandpass" ? 2 : node.filterMode == "highpass" ? 3 : 1, juce::dontSendNotification);
        mode.onChange = [this, &mode, findFilterNode]
        {
            if (auto* target = findFilterNode())
                target->filterMode = mode.getSelectedId() == 2 ? "bandpass"
                                    : mode.getSelectedId() == 3 ? "highpass"
                                                                : "lowpass";
            regenerateSignal();
        };

        auto& cutoff = content->addSliderRow("Cutoff", kMinFilterCutoffHz, kMaxFilterCutoffHz, 1.0);
        double wiredCutoff = 0.0;
        if (findWiredParameterValue(node.id, "param:filterCutoff", wiredCutoff))
        {
            cutoff.setValue(wiredCutoff, juce::dontSendNotification);
            cutoff.setEnabled(false);
        }
        else
        {
            cutoff.setValue(node.filterCutoffHz, juce::dontSendNotification);
            cutoff.onValueChange = [this, &cutoff, findFilterNode]
            {
                if (auto* target = findFilterNode())
                    target->filterCutoffHz = (float) cutoff.getValue();
                regenerateSignal();
            };
        }

        auto& resonance = content->addSliderRow("Resonance", 0.30, 8.0, 0.01);
        double wiredResonance = 0.0;
        if (findWiredParameterValue(node.id, "param:filterResonance", wiredResonance))
        {
            resonance.setValue(wiredResonance, juce::dontSendNotification);
            resonance.setEnabled(false);
        }
        else
        {
            resonance.setValue(node.filterResonance, juce::dontSendNotification);
            resonance.onValueChange = [this, &resonance, findFilterNode]
            {
                if (auto* target = findFilterNode())
                    target->filterResonance = (float) resonance.getValue();
                regenerateSignal();
            };
        }
    }
    else if (node.type == "mix")
    {
        // Real mixing-board faders, one per channel, instead of the "+
        // Input" button being the only way to touch a channel's weight.
        // Wiring a Get-variable into a channel's Weight port still works
        // (buildPatchDocument already resolves it via
        // findWiredParameterValue) -- that channel's fader just shows the
        // wired value and disables, same rule as every other parameter.
        auto nodeId = node.id;
        auto findMixNode = [this, nodeId]() -> GraphNodeModel*
        {
            for (auto& candidate : graphNodes)
                if (candidate.id == nodeId)
                    return &candidate;
            return nullptr;
        };

        juce::Array<MixerFaderBank::ChannelState> channelStates;
        for (int channelIndex = 0; channelIndex < node.mixerInputVolumes.size(); ++channelIndex)
        {
            MixerFaderBank::ChannelState state;
            state.label = "Ch " + juce::String(channelIndex + 1);
            state.value = node.mixerInputVolumes[channelIndex];

            double wiredWeight = 0.0;
            if (findWiredParameterValue(node.id, "mixWeight:" + juce::String(channelIndex), wiredWeight))
            {
                state.wired = true;
                state.wiredValue = wiredWeight;
            }
            channelStates.add(state);
        }

        auto* faderBank = new MixerFaderBank();
        faderBank->setChannels(channelStates, [this, findMixNode](int channelIndex, float newValue)
        {
            if (auto* target = findMixNode())
                if (channelIndex >= 0 && channelIndex < target->mixerInputVolumes.size())
                    target->mixerInputVolumes.getReference(channelIndex) = newValue;
            regenerateSignal();
        });
        content->addCustomComponent(faderBank, 170);
    }
    else if (node.type == "midiFader" || node.type == "midiButton")
    {
        auto nodeId = node.id;
        auto findMidiNode = [this, nodeId]() -> GraphNodeModel*
        {
            for (auto& candidate : graphNodes)
                if (candidate.id == nodeId)
                    return &candidate;
            return nullptr;
        };

        auto describeBinding = [](const GraphNodeModel& n) -> juce::String
        {
            if (! n.midiLearned)
                return "Not learned yet -- click Learn, then move or press the control.";
            return "Bound to " + (n.midiDeviceLabel.isNotEmpty() ? n.midiDeviceLabel : "any device")
                 + ", channel " + juce::String(n.midiChannel) + ", "
                 + (n.midiIsController ? "CC " : "Note ") + juce::String(n.midiNumber);
        };

        auto* statusLabelForNode = new juce::Label();
        statusLabelForNode->setJustificationType(juce::Justification::topLeft);
        statusLabelForNode->setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        statusLabelForNode->setText(describeBinding(node), juce::dontSendNotification);
        content->addCustomComponent(statusLabelForNode, 40);

        // SafePointer, not a raw pointer: the Learn dialog spawned below is a
        // separate top-level window that can outlive this tool window (the
        // user can close this window while a learn capture is still
        // pending) -- a raw pointer to statusLabelForNode would dangle in
        // that case, and a plain null-check wouldn't catch it since nothing
        // sets it to null on delete.
        juce::Component::SafePointer<juce::Label> statusLabelSafe(statusLabelForNode);

        auto& learnButton = content->addButtonRow(node.midiLearned ? "Re-learn..." : "Learn...");
        learnButton.onClick = [this, findMidiNode, statusLabelSafe, describeBinding, nodeTitle = node.title]
        {
            if (! onMidiLearnRequested)
                return;

            onMidiLearnRequested(nodeTitle, [this, findMidiNode, statusLabelSafe, describeBinding]
                                 (juce::String deviceId, int channel, int number, bool isController)
            {
                auto* target = findMidiNode();
                if (target == nullptr)
                    return;

                juce::String deviceLabel = "Any device";
                if (deviceId.isNotEmpty())
                {
                    for (const auto& device : juce::MidiInput::getAvailableDevices())
                    {
                        if (device.identifier == deviceId)
                        {
                            deviceLabel = device.name;
                            break;
                        }
                    }
                }

                target->midiLearned = true;
                target->midiDeviceId = deviceId;
                target->midiDeviceLabel = deviceLabel;
                target->midiChannel = channel;
                target->midiNumber = number;
                target->midiIsController = isController;

                if (auto* label = statusLabelSafe.getComponent())
                    label->setText(describeBinding(*target), juce::dontSendNotification);
                nodeGraphCanvas.repaint();
            });
        };

        if (node.type == "midiButton")
        {
            auto& modeCombo = content->addComboRow("Button Mode", { "Momentary", "Toggle (On/Off)" });
            modeCombo.setSelectedId(node.midiButtonMode == "toggle" ? 2 : 1, juce::dontSendNotification);
            modeCombo.onChange = [this, &modeCombo, findMidiNode]
            {
                if (auto* target = findMidiNode())
                    target->midiButtonMode = modeCombo.getSelectedId() == 2 ? "toggle" : "momentary";
            };
        }

        auto& clearButton = content->addButtonRow("Clear Binding");
        clearButton.onClick = [this, findMidiNode, statusLabelSafe, describeBinding]
        {
            auto* target = findMidiNode();
            if (target == nullptr)
                return;

            target->midiLearned = false;
            target->midiDeviceId.clear();
            target->midiDeviceLabel.clear();
            target->midiChannel = 1;
            target->midiNumber = 0;
            target->midiLiveValue = 0.0f;

            if (auto* label = statusLabelSafe.getComponent())
                label->setText(describeBinding(*target), juce::dontSendNotification);
            regenerateSignal();
        };
    }
    else if (node.type == "output")
    {
        auto& duration = content->addSliderRow("Duration", 0.1, 3600.0, 0.01);
        duration.setValue(recipe.durationSeconds, juce::dontSendNotification);
        duration.onValueChange = [this, &duration] { recipe.durationSeconds = duration.getValue(); regenerateSignal(); };

        auto& mode = content->addComboRow("Output Mode", { "Audio (Device)", "Wave (File)" });
        mode.setSelectedId(recipe.sinkMode == "wave" ? 2 : 1, juce::dontSendNotification);
        mode.onChange = [this, &mode] { recipe.sinkMode = mode.getSelectedId() == 2 ? "wave" : "audio"; regenerateSignal(); };

        auto& deviceButton = content->addButtonRow("Change Output Device...");
        deviceButton.onClick = [this] { if (onAudioSettingsRequested) onAudioSettingsRequested(); };

        auto& renderButton2 = content->addButtonRow("Render \"" + recipe.name + "\" to Project");
        renderButton2.onClick = [this]
        {
            ensureAudioRendered();
            if (onRenderRequested && generatedBuffer.getNumSamples() > 0)
                onRenderRequested(generatedBuffer, recipe.sampleRate, recipe.name);
        };
    }
    else if (node.type == "scope")
    {
        ensureAudioRendered();
        auto* scope = new ScopePanel();
        scope->setBuffer(getDisplayBufferForNode(node.id));
        content->addCustomComponent(scope, 220);
    }
    else if (node.type == "analyzer")
    {
        ensureAudioRendered();
        auto* analyzer = new SpectrumPanel();
        analyzer->setBuffer(getDisplayBufferForNode(node.id), recipe.sampleRate);
        content->addCustomComponent(analyzer, 220);
    }
    else if (node.type == "envelope")
    {
        // Each Envelope instance owns its own curve now. EnvelopeEditor only
        // reads envelopePoints/envelopeCurveMode off whatever SignalRecipe
        // it's given, so a throwaway recipe carrying just this node's curve
        // is enough to point it at the right instance without changing the
        // editor component itself.
        auto nodeId = node.id;
        auto* editor = new EnvelopeEditor();
        SignalRecipe envelopeView;
        envelopeView.envelopeCurveMode = node.envelopeCurveMode;
        envelopeView.envelopePoints = node.envelopePoints;
        editor->setRecipe(envelopeView);
        editor->onEnvelopeChanged = [this, nodeId](const juce::Array<cw::PatchAutomationPoint>& points)
        {
            for (auto& candidate : graphNodes)
                if (candidate.id == nodeId)
                {
                    candidate.envelopePoints = points;
                    break;
                }
            regenerateSignal();
        };
        content->addCustomComponent(editor, 220);
    }
    else if (node.type == "timeline")
    {
        auto* laneEditor = new AutomationLaneEditor();
        for (const auto& lane : recipe.automationLanes)
        {
            if (lane.id == node.id)
            {
                laneEditor->setLane(lane, node.accent);
                break;
            }
        }
        laneEditor->onLaneChanged = [this, nodeId = node.id](const cw::PatchAutomationLane& updatedLane)
        {
            for (auto& lane : recipe.automationLanes)
            {
                if (lane.id == nodeId)
                {
                    lane = updatedLane;
                    regenerateSignal();
                    break;
                }
            }
        };
        content->addCustomComponent(laneEditor, 220);
    }
    else if (node.type == "value")
    {
        for (auto& variable : localControls)
        {
            if (variable.id != node.targetParameter)
                continue;

            auto& valueSlider = content->addSliderRow(variable.name, 0.0, 1.0, 0.001);
            valueSlider.setValue(variable.value, juce::dontSendNotification);
            valueSlider.onValueChange = [this, variableId = variable.id, &valueSlider]
            {
                for (auto& control : localControls)
                {
                    if (control.id == variableId)
                    {
                        control.value = (float) valueSlider.getValue();
                        applyLocalControlToRecipe(control);
                        refreshVariablePanel();
                        regenerateSignal();
                        break;
                    }
                }
            };
            break;
        }
    }

    nodeEditorVisible = false;
    editingNodeIndex = selectedGraphNodeIndex;
    layoutFloatingWindows();

    auto* entry = openNodeWindows.add(new OpenNodeWindow());
    entry->nodeId = node.id;

    auto window = std::make_unique<SignalLabNodeWindow>(node.title, [this, nodeId = node.id]
    {
        for (int index = openNodeWindows.size(); --index >= 0;)
        {
            auto* candidate = openNodeWindows[index];
            if (candidate != nullptr && candidate->nodeId == nodeId)
            {
                openNodeWindows.remove(index);
                break;
            }
        }
    });
    window->setContentOwned(content.release(), true);
    auto windowWidth = node.type == "scope" || node.type == "analyzer" || node.type == "timeline" || node.type == "envelope" ? 520
                      : node.type == "mix" ? juce::jmax(420, 70 * node.mixerInputVolumes.size() + 80)
                      : 420;
    auto windowHeight = node.type == "scope" || node.type == "analyzer" || node.type == "timeline" || node.type == "envelope" ? 360
                       : node.type == "mix" ? 340
                       : 280;
    window->centreWithSize(windowWidth, windowHeight);
    window->setVisible(true);
    window->toFront(true);
    entry->window = std::move(window);
}

void SignalLabPanel::closeNodeEditor()
{
    nodeEditorVisible = false;
    editingNodeIndex = -1;
    layoutFloatingWindows();
}

void SignalLabPanel::toggleControlPad()
{
    controlPadVisible = ! controlPadVisible;
    layoutFloatingWindows();
}

void SignalLabPanel::ensureDefaultLocalControls()
{
}

void SignalLabPanel::rebuildLocalControlChrome()
{
    while (localControlNameEditors.size() < localControls.size())
    {
        auto* nameEditor = localControlNameEditors.add(new juce::TextEditor());
        auto* targetSelector = localControlTargetSelectors.add(new juce::ComboBox());
        auto* valueSlider = localControlValueSliders.add(new juce::Slider());
        auto* removeButton = removeLocalControlButtons.add(new juce::TextButton("×"));

        for (const auto& spec : getAutomationTargetSpecs())
            targetSelector->addItem(spec.title, targetSelector->getNumItems() + 1);

        configureSlider(*valueSlider, 0.0, 1.0, 0.001);
        addAndMakeVisible(nameEditor);
        addAndMakeVisible(targetSelector);
        addAndMakeVisible(removeButton);

        nameEditor->onTextChange = [this, nameEditor]
        {
            if (suppressCallbacks)
                return;
            auto index = localControlNameEditors.indexOf(nameEditor);
            if (index >= 0 && index < localControls.size())
            {
                localControls.getReference(index).name = nameEditor->getText().trim();
                refreshVariablePanel();
                nodeGraphCanvas.repaint();
            }
        };

        targetSelector->onChange = [this, targetSelector]
        {
            if (suppressCallbacks)
                return;
            auto index = localControlTargetSelectors.indexOf(targetSelector);
            if (index >= 0 && index < localControls.size())
            {
                auto selected = targetSelector->getSelectedItemIndex();
                if (selected >= 0 && selected < (int) getAutomationTargetSpecs().size())
                {
                    localControls.getReference(index).targetParameter = getAutomationTargetSpecs()[(size_t) selected].parameterId;
                    applyLocalControlToRecipe(localControls.getReference(index));
                    refreshVariablePanel();
                    regenerateSignal();
                }
            }
        };

        valueSlider->onValueChange = [this, valueSlider]
        {
            if (suppressCallbacks)
                return;
            auto index = localControlValueSliders.indexOf(valueSlider);
            if (index >= 0 && index < localControls.size())
            {
                localControls.getReference(index).value = (float) valueSlider->getValue();
                applyLocalControlToRecipe(localControls.getReference(index));
                refreshVariablePanel();
                regenerateSignal();
            }
        };

        removeButton->onClick = [this, removeButton]
        {
            auto index = removeLocalControlButtons.indexOf(removeButton);
            if (index >= 0 && index < localControls.size())
            {
                captureUndoCheckpoint("Remove local control variable");
                localControls.remove(index);
                rebuildLocalControlChrome();
                refreshVariablePanel();
                layoutFloatingWindows();
                nodeGraphCanvas.repaint();
            }
        };
    }

    suppressCallbacks = true;
    for (int index = 0; index < localControls.size(); ++index)
    {
        auto& control = localControls.getReference(index);
        localControlNameEditors[index]->setText(control.name, juce::dontSendNotification);
        localControlValueSliders[index]->setValue(control.value, juce::dontSendNotification);
        for (int itemIndex = 0; itemIndex < (int) getAutomationTargetSpecs().size(); ++itemIndex)
        {
            if (control.targetParameter == juce::String(getAutomationTargetSpecs()[(size_t) itemIndex].parameterId))
            {
                localControlTargetSelectors[index]->setSelectedItemIndex(itemIndex, juce::dontSendNotification);
                break;
            }
        }
    }
    suppressCallbacks = false;

    for (int index = 0; index < localControlNameEditors.size(); ++index)
    {
        auto visible = index < localControls.size();
        localControlNameEditors[index]->setVisible(visible && controlPadVisible);
        localControlTargetSelectors[index]->setVisible(visible && controlPadVisible);
        localControlValueSliders[index]->setVisible(visible && controlPadVisible);
        removeLocalControlButtons[index]->setVisible(visible && controlPadVisible);
    }
}

void SignalLabPanel::applyLocalControlToRecipe(const LocalControlVariable& control)
{
    auto value = juce::jlimit(0.0f, 1.0f, control.value);
    auto assignLevel = [&](float& target) { target = value; };

    if (control.targetParameter == "outputGain")
    {
        setLaneValues(recipe, "outputGain", { value, value, value, value });
        return;
    }
    if (control.targetParameter == "filterCutoff")
    {
        recipe.filterCutoffHz = normalizedToCutoff(value);
        return;
    }
    if (control.targetParameter == "filterResonance")
    {
        recipe.filterResonance = (float) juce::jmap((double) value, getTargetSpec("filterResonance").rangeMin, getTargetSpec("filterResonance").rangeMax);
        return;
    }
    if (control.targetParameter == "filterEnvelopeAmount")
    {
        recipe.filterEnvelopeAmount = (float) juce::jmap((double) value, -1.0, 1.0);
        return;
    }
    if (control.targetParameter == "baseFrequency")
    {
        recipe.baseFrequencyHz = (float) juce::jmap((double) value, getTargetSpec("baseFrequency").rangeMin, getTargetSpec("baseFrequency").rangeMax);
        return;
    }
    if (control.targetParameter == "pitchOffsetSemitones")
    {
        recipe.pitchSweepSemitones = (float) juce::jmap((double) value, -12.0, 12.0);
        return;
    }
    if (control.targetParameter == "sineLevel") { assignLevel(recipe.sineLevel); return; }
    if (control.targetParameter == "sawLevel") { assignLevel(recipe.sawLevel); return; }
    if (control.targetParameter == "squareLevel") { assignLevel(recipe.squareLevel); return; }
    if (control.targetParameter == "triangleLevel") { assignLevel(recipe.triangleLevel); return; }
    if (control.targetParameter == "noiseLevel") { assignLevel(recipe.noiseLevel); return; }
    if (control.targetParameter == "macroHardness") { recipe.macroHardness = value; return; }
    if (control.targetParameter == "macroWeight") { recipe.macroWeight = value; return; }
    if (control.targetParameter == "macroAir") { recipe.macroAir = value; return; }
    if (control.targetParameter == "macroGrit") { recipe.macroGrit = value; return; }
    if (control.targetParameter == "macroSize") { recipe.macroSize = value; return; }
}

void SignalLabPanel::refreshVariablePanel()
{
    toolboxPane.setVariables(localControls);
    if (selectedLocalControlIndex >= localControls.size())
        selectedLocalControlIndex = localControls.isEmpty() ? -1 : 0;
    refreshSelectedVariableEditor();
}

void SignalLabPanel::refreshSelectedVariableEditor()
{
    auto hasSelection = selectedLocalControlIndex >= 0 && selectedLocalControlIndex < localControls.size();

    variableNameLabel.setVisible(hasSelection);
    variableNameEditor.setVisible(hasSelection);
    variableDescriptionLabel.setVisible(hasSelection);
    variableDescriptionEditor.setVisible(hasSelection);
    variableTypeLabel.setVisible(hasSelection);
    variableTypeSelector.setVisible(hasSelection);
    variableAccessLabel.setVisible(hasSelection);
    variableAccessSelector.setVisible(hasSelection);
    variableValueLabel.setVisible(hasSelection);
    variableValueEditor.setVisible(hasSelection);
    variableAutomationToggle.setVisible(hasSelection);

    if (! hasSelection)
    {
        signalMetaLabel.setText("Blank signal graph\nOnly Output is required.", juce::dontSendNotification);
        return;
    }

    suppressCallbacks = true;
    auto& variable = localControls.getReference(selectedLocalControlIndex);
    variableNameEditor.setText(variable.name, juce::dontSendNotification);
    variableDescriptionEditor.setText(variable.description, juce::dontSendNotification);
    variableTypeSelector.setText(variable.valueType, juce::dontSendNotification);
    variableAccessSelector.setText(variable.accessScope, juce::dontSendNotification);
    if (variable.valueType == "Bool")
        variableValueEditor.setText(variable.value >= 0.5f ? "true" : "false", juce::dontSendNotification);
    else if (variable.valueType == "Int")
        variableValueEditor.setText(juce::String((int) std::round(variable.value)), juce::dontSendNotification);
    else
        variableValueEditor.setText(juce::String(variable.value, 3), juce::dontSendNotification);
    variableAutomationToggle.setToggleState(variable.exposedToAutomation, juce::dontSendNotification);
    variableValueEditor.setEnabled(! variable.exposedToAutomation);
    suppressCallbacks = false;

    signalMetaLabel.setText("Samples: " + juce::String((int) recipe.sampleRate)
                            + "\nDuration: " + formatDurationText(recipe.durationSeconds)
                            + "\nNodes: " + juce::String(graphNodes.size()),
                            juce::dontSendNotification);
}

juce::Point<float> SignalLabPanel::getControlPadOutputPort(int index) const
{
    auto top = controlPadBounds.getY() + 82 + index * 48;
    return { (float) controlPadBounds.getRight(), (float) top };
}

juce::Colour SignalLabPanel::portValueColour(PortValueType type)
{
    switch (type)
    {
        case PortValueType::Float: return juce::Colour(0xffe5484d);
        case PortValueType::Int:   return juce::Colour(0xff4d8fe5);
        case PortValueType::Bool:  return juce::Colour(0xff4de58f);
    }
    return juce::Colours::white;
}

juce::Array<SignalLabPanel::GraphPort> SignalLabPanel::getNodePorts(int index) const
{
    juce::Array<GraphPort> ports;
    if (index < 0 || index >= graphNodes.size())
        return ports;

    const auto& node = graphNodes.getReference(index);
    auto bounds = getGraphNodeBounds(index).toFloat();

    // The white ports carry the actual audio signal being processed through
    // the chain (source -> mix -> filter -> envelope -> output) -- there is
    // no separate abstract "signal" port alongside them. Sources only emit
    // (no signal-in); the final Output node only receives (no signal-out).
    bool hasSignalIn = node.type != "sine" && node.type != "saw" && node.type != "square"
                     && node.type != "triangle" && node.type != "noise" && node.type != "timeline"
                     && node.type != "value" && node.type != "valueGet" && node.type != "valueSet"
                     && node.type != "mix" && node.type != "midiFader" && node.type != "midiButton";
    bool hasSignalOut = node.type != "output" && node.type != "valueGet" && node.type != "valueSet"
                      && node.type != "midiFader" && node.type != "midiButton";

    float leftY = bounds.getY() + 30.0f;

    // Signal ports get their own row just under the header, clear of the
    // type badge, instead of being crammed into the corners of the title
    // bar. Both sides share the row since one node rarely has both.
    if (hasSignalIn)
    {
        GraphPort p;
        p.portId = "signalIn";
        p.label = "Signal";
        p.isOutput = false;
        p.isExec = true;
        p.position = { bounds.getX() + 20.0f, leftY };
        ports.add(p);
    }
    if (hasSignalOut)
    {
        GraphPort p;
        p.portId = "signalOut";
        p.label = "Signal";
        p.isOutput = true;
        p.isExec = true;
        p.position = { bounds.getRight() - 20.0f, leftY };
        ports.add(p);
    }
    if (hasSignalIn || hasSignalOut)
        leftY += 22.0f;

    // Mixer output is a weighted sum: out = sum(input_i * weight_i). Each
    // input is a white Signal port (the stream) paired with a real-valued
    // weight/multiplier port (Float, automatable) -- variable arity,
    // default 2 inputs, grown via the node's own "+ Input" button. Unlike
    // every other node type, this isn't a fixed registry shape from
    // nodeParameterIds.
    if (node.type == "mix")
    {
        for (int inputIndex = 0; inputIndex < node.mixerInputVolumes.size(); ++inputIndex)
        {
            GraphPort signalPort;
            signalPort.portId = "signalIn:" + juce::String(inputIndex);
            signalPort.label = "Channel " + juce::String(inputIndex + 1);
            signalPort.isOutput = false;
            signalPort.isExec = true;
            signalPort.position = { bounds.getX(), leftY };
            ports.add(signalPort);
            leftY += 22.0f;

            GraphPort weightPort;
            weightPort.portId = "mixWeight:" + juce::String(inputIndex);
            weightPort.label = "Weight " + juce::String(inputIndex + 1);
            weightPort.isOutput = false;
            weightPort.isExec = false;
            weightPort.valueType = PortValueType::Float;
            weightPort.position = { bounds.getX(), leftY };
            ports.add(weightPort);
            leftY += 22.0f;
        }
        leftY += 26.0f; // room for the "+ Input" button drawn below the last pair
    }

    for (auto& parameterId : nodeParameterIds(node.type))
    {
        GraphPort p;
        p.portId = "param:" + parameterId;
        p.label = shortParamLabel(parameterId);
        p.isOutput = false;
        p.isExec = false;
        p.valueType = PortValueType::Float;
        p.position = { bounds.getX(), leftY };
        ports.add(p);
        leftY += 22.0f;
    }

    if (node.type == "valueGet" || node.type == "valueSet")
    {
        PortValueType variableType = PortValueType::Float;
        for (auto& variable : localControls)
        {
            if (variable.id != node.targetParameter)
                continue;
            if (variable.valueType == "Int") variableType = PortValueType::Int;
            else if (variable.valueType == "Bool") variableType = PortValueType::Bool;
            break;
        }

        if (node.type == "valueSet")
        {
            GraphPort p;
            p.portId = "valueIn";
            p.label = "In";
            p.isOutput = false;
            p.isExec = false;
            p.valueType = variableType;
            p.position = { bounds.getX(), leftY };
            ports.add(p);
        }

        GraphPort p;
        p.portId = "valueOut";
        p.label = "Value";
        p.isOutput = true;
        p.isExec = false;
        p.valueType = variableType;
        p.position = { bounds.getRight(), bounds.getY() + 30.0f };
        ports.add(p);
    }

    if (node.type == "midiFader" || node.type == "midiButton")
    {
        // Single typed output, same shape as a Get-variable node -- a MIDI
        // Control node IS a value source, just one driven by hardware
        // instead of a manually-set variable. Type follows the control kind
        // so it can only wire into ports of the matching colour, same rule
        // as everything else.
        GraphPort p;
        p.portId = "valueOut";
        p.label = "Value";
        p.isOutput = true;
        p.isExec = false;
        p.valueType = node.type == "midiFader" ? PortValueType::Float : PortValueType::Bool;
        p.position = { bounds.getRight(), bounds.getY() + 30.0f };
        ports.add(p);
    }

    return ports;
}

SignalLabPanel::PortHit SignalLabPanel::findPortAt(juce::Point<int> canvasPosition) const
{
    constexpr float hitRadius = 9.0f;
    for (int index = graphNodes.size() - 1; index >= 0; --index)
    {
        for (auto& port : getNodePorts(index))
        {
            auto screenPos = graphToCanvas(port.position);
            if (screenPos.getDistanceFrom(canvasPosition.toFloat()) <= hitRadius)
                return { true, index, port };
        }
    }
    return {};
}

juce::Point<int> SignalLabPanel::resolvePortPosition(const juce::String& nodeId, const juce::String& portId, bool wantOutput) const
{
    juce::ignoreUnused(wantOutput);
    for (int index = 0; index < graphNodes.size(); ++index)
    {
        if (graphNodes.getReference(index).id != nodeId)
            continue;
        for (auto& port : getNodePorts(index))
            if (port.portId == portId)
                return port.position.roundToInt();
    }
    return {};
}

juce::Path SignalLabPanel::buildConnectionPath(int connectionIndex) const
{
    juce::Path path;
    if (connectionIndex < 0 || connectionIndex >= graphConnections.size())
        return path;

    auto& connection = graphConnections.getReference(connectionIndex);
    auto fromPoint = graphToCanvas(resolvePortPosition(connection.fromNodeId, connection.fromPortId, true));
    auto toPoint = graphToCanvas(resolvePortPosition(connection.toNodeId, connection.toPortId, false));

    juce::Array<juce::Point<int>> canvasPoints;
    canvasPoints.add(fromPoint);
    for (auto& waypoint : connection.waypoints)
        canvasPoints.add(graphToCanvas(waypoint));
    canvasPoints.add(toPoint);

    path.startNewSubPath(canvasPoints.getReference(0).toFloat());
    for (int pointIndex = 1; pointIndex < canvasPoints.size(); ++pointIndex)
    {
        auto a = canvasPoints.getReference(pointIndex - 1).toFloat();
        auto b = canvasPoints.getReference(pointIndex).toFloat();
        auto handle = juce::jmax(24.0f, std::abs(b.x - a.x) * 0.4f);
        path.cubicTo(a.translated(handle, 0.0f), b.translated(-handle, 0.0f), b);
    }
    return path;
}

int SignalLabPanel::findConnectionAt(juce::Point<int> canvasPosition, int* outWaypointIndex) const
{
    if (outWaypointIndex != nullptr)
        *outWaypointIndex = -1;

    // Generous on purpose -- these wires are drawn as bowed bezier curves
    // (see buildConnectionPath/paint), not straight lines between their
    // endpoints, and a tight pixel tolerance meant a click that visually
    // looked like it was right on the wire would miss the hit-test and fall
    // through to the generic canvas menu instead of the wire's own menu.
    constexpr float waypointHitRadius = 10.0f;
    constexpr float lineHitDistance = 9.0f;

    for (int connectionIndex = 0; connectionIndex < graphConnections.size(); ++connectionIndex)
    {
        auto& connection = graphConnections.getReference(connectionIndex);

        for (int waypointIndex = 0; waypointIndex < connection.waypoints.size(); ++waypointIndex)
        {
            auto centre = graphToCanvas(connection.waypoints.getReference(waypointIndex));
            if (centre.toFloat().getDistanceFrom(canvasPosition.toFloat()) <= waypointHitRadius)
            {
                if (outWaypointIndex != nullptr)
                    *outWaypointIndex = waypointIndex;
                return connectionIndex;
            }
        }

        // Hit-test against the actual curve, not the straight line between
        // its endpoints -- stroke the real path out to the hit tolerance and
        // do a containment test, which follows the bow correctly no matter
        // how far the curve departs from a straight line.
        auto path = buildConnectionPath(connectionIndex);
        juce::Path strokeOutline;
        juce::PathStrokeType(lineHitDistance * 2.0f).createStrokedPath(strokeOutline, path);
        if (strokeOutline.contains(canvasPosition.toFloat()))
            return connectionIndex;
    }

    return -1;
}

void SignalLabPanel::tryCompleteConnection(int fromNodeIndex, const GraphPort& fromPort, juce::Point<int> releaseCanvasPosition)
{
    auto hit = findPortAt(releaseCanvasPosition);
    if (! hit.found || hit.nodeIndex == fromNodeIndex)
        return;

    if (hit.port.isExec != fromPort.isExec)
        return;
    if (! hit.port.isExec && hit.port.valueType != fromPort.valueType)
        return;
    if (hit.port.isOutput == fromPort.isOutput)
        return;

    const GraphPort& outputPort = fromPort.isOutput ? fromPort : hit.port;
    const GraphPort& inputPort = fromPort.isOutput ? hit.port : fromPort;
    int outputNodeIndex = fromPort.isOutput ? fromNodeIndex : hit.nodeIndex;
    int inputNodeIndex = fromPort.isOutput ? hit.nodeIndex : fromNodeIndex;

    captureUndoCheckpoint("Connect nodes");

    for (int index = graphConnections.size() - 1; index >= 0; --index)
    {
        auto& existing = graphConnections.getReference(index);
        if (existing.toNodeId == graphNodes.getReference(inputNodeIndex).id && existing.toPortId == inputPort.portId)
            graphConnections.remove(index);
    }

    GraphConnection connection;
    connection.id = "connection" + juce::String(juce::Random::getSystemRandom().nextInt64());
    connection.fromNodeId = graphNodes.getReference(outputNodeIndex).id;
    connection.fromPortId = outputPort.portId;
    connection.toNodeId = graphNodes.getReference(inputNodeIndex).id;
    connection.toPortId = inputPort.portId;
    connection.isExec = outputPort.isExec;
    connection.valueType = outputPort.valueType;
    graphConnections.add(connection);

    // Pre-existing gap, fixed here because it directly matters for MIDI
    // Control wiring: neither audioDirty nor liveGraphDirty was ever marked
    // by completing a wire (only by the handful of edits that already
    // called regenerateSignal() elsewhere) -- wiring a MIDI Control node
    // into a parameter port after an earlier Play, then pressing Play
    // again, would silently keep using the old topology.
    regenerateSignal();
    nodeGraphCanvas.repaint();
}

void SignalLabPanel::removeConnection(int index)
{
    if (index < 0 || index >= graphConnections.size())
        return;
    captureUndoCheckpoint("Delete connection");
    graphConnections.remove(index);
    if (selectedConnectionIndex == index)
        selectedConnectionIndex = -1;
    regenerateSignal(); // see the matching comment in tryCompleteConnection
    nodeGraphCanvas.repaint();
}

void SignalLabPanel::showConnectionContextMenu(int connectionIndex, int waypointIndex, juce::Point<int> canvasPosition)
{
    if (connectionIndex < 0 || connectionIndex >= graphConnections.size())
        return;

    juce::PopupMenu menu;
    if (waypointIndex >= 0)
        menu.addItem(1, "Remove Reroute Point");
    else
        menu.addItem(2, "Add Reroute Point Here");
    menu.addItem(3, "Delete Connection");

    auto area = nodeGraphCanvas.localAreaToGlobal(juce::Rectangle<int>(canvasPosition.x, canvasPosition.y, 1, 1));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
                       [this, connectionIndex, waypointIndex, canvasPosition](int result)
                       {
                           if (connectionIndex < 0 || connectionIndex >= graphConnections.size())
                               return;

                           if (result == 1 && waypointIndex >= 0)
                           {
                               captureUndoCheckpoint("Remove reroute point");
                               graphConnections.getReference(connectionIndex).waypoints.remove(waypointIndex);
                               nodeGraphCanvas.repaint();
                           }
                           else if (result == 2)
                           {
                               captureUndoCheckpoint("Add reroute point");
                               graphConnections.getReference(connectionIndex).waypoints.add(canvasToGraph(canvasPosition));
                               nodeGraphCanvas.repaint();
                           }
                           else if (result == 3)
                           {
                               removeConnection(connectionIndex);
                           }
                       });
}

juce::ValueTree SignalLabPanel::createState() const
{
    juce::ValueTree state("SignalLab");
    state.setProperty("name", recipe.name, nullptr);
    state.setProperty("sampleRate", recipe.sampleRate, nullptr);
    state.setProperty("durationSeconds", recipe.durationSeconds, nullptr);
    state.setProperty("sinkMode", recipe.sinkMode, nullptr);
    state.setProperty("baseFrequencyHz", recipe.baseFrequencyHz, nullptr);
    state.setProperty("filterMode", recipe.filterMode, nullptr);
    state.setProperty("filterCutoffHz", recipe.filterCutoffHz, nullptr);
    state.setProperty("filterResonance", recipe.filterResonance, nullptr);
    state.setProperty("filterEnvelopeAmount", recipe.filterEnvelopeAmount, nullptr);
    state.setProperty("envelopeCurveMode", recipe.envelopeCurveMode, nullptr);
    state.setProperty("automationCurveMode", recipe.automationCurveMode, nullptr);
    state.setProperty("macroHardness", recipe.macroHardness, nullptr);
    state.setProperty("macroWeight", recipe.macroWeight, nullptr);
    state.setProperty("macroAir", recipe.macroAir, nullptr);
    state.setProperty("macroGrit", recipe.macroGrit, nullptr);
    state.setProperty("macroSize", recipe.macroSize, nullptr);
    state.setProperty("mixNodeEnabled", mixNodeEnabled, nullptr);
    state.setProperty("filterNodeEnabled", filterNodeEnabled, nullptr);
    state.setProperty("envelopeNodeEnabled", envelopeNodeEnabled, nullptr);
    state.setProperty("sineLevel", recipe.sineLevel, nullptr);
    state.setProperty("sawLevel", recipe.sawLevel, nullptr);
    state.setProperty("squareLevel", recipe.squareLevel, nullptr);
    state.setProperty("triangleLevel", recipe.triangleLevel, nullptr);
    state.setProperty("noiseLevel", recipe.noiseLevel, nullptr);
    state.setProperty("pitchSweepSemitones", recipe.pitchSweepSemitones, nullptr);

    for (const auto& point : recipe.envelopePoints)
    {
        juce::ValueTree pointState("EnvelopePoint");
        pointState.setProperty("time", point.time, nullptr);
        pointState.setProperty("value", point.value, nullptr);
        pointState.setProperty("curve", point.curve, nullptr);
        state.addChild(pointState, -1, nullptr);
    }

    for (const auto& lane : recipe.automationLanes)
    {
        juce::ValueTree laneState("AutomationLane");
        laneState.setProperty("id", lane.id, nullptr);
        laneState.setProperty("name", lane.name, nullptr);
        laneState.setProperty("targetParameter", lane.targetParameter, nullptr);
        laneState.setProperty("interpolation", lane.interpolation, nullptr);
        laneState.setProperty("rangeMin", lane.rangeMin, nullptr);
        laneState.setProperty("rangeMax", lane.rangeMax, nullptr);
        for (const auto& point : lane.points)
        {
            juce::ValueTree pointState("Point");
            pointState.setProperty("time", point.time, nullptr);
            pointState.setProperty("value", point.value, nullptr);
            pointState.setProperty("curve", point.curve, nullptr);
            laneState.addChild(pointState, -1, nullptr);
        }
        state.addChild(laneState, -1, nullptr);
    }

    return state;
}

void SignalLabPanel::restoreState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    recipe = {};
    mixNodeEnabled = false;
    filterNodeEnabled = false;
    envelopeNodeEnabled = false;
    recipe.name = state.getProperty("name", recipe.name).toString();
    recipe.sampleRate = (double) state.getProperty("sampleRate", recipe.sampleRate);
    recipe.durationSeconds = (double) state.getProperty("durationSeconds", recipe.durationSeconds);
    recipe.sinkMode = state.getProperty("sinkMode", recipe.sinkMode).toString();
    recipe.baseFrequencyHz = (float) state.getProperty("baseFrequencyHz", recipe.baseFrequencyHz);
    recipe.filterMode = state.getProperty("filterMode", recipe.filterMode).toString();
    recipe.filterCutoffHz = (float) state.getProperty("filterCutoffHz", recipe.filterCutoffHz);
    recipe.filterResonance = (float) state.getProperty("filterResonance", recipe.filterResonance);
    recipe.filterEnvelopeAmount = (float) state.getProperty("filterEnvelopeAmount", recipe.filterEnvelopeAmount);
    recipe.envelopeCurveMode = state.getProperty("envelopeCurveMode", recipe.envelopeCurveMode).toString();
    recipe.automationCurveMode = state.getProperty("automationCurveMode", recipe.automationCurveMode).toString();
    recipe.macroHardness = (float) state.getProperty("macroHardness", recipe.macroHardness);
    recipe.macroWeight = (float) state.getProperty("macroWeight", recipe.macroWeight);
    recipe.macroAir = (float) state.getProperty("macroAir", recipe.macroAir);
    recipe.macroGrit = (float) state.getProperty("macroGrit", recipe.macroGrit);
    recipe.macroSize = (float) state.getProperty("macroSize", recipe.macroSize);
    mixNodeEnabled = (bool) state.getProperty("mixNodeEnabled", mixNodeEnabled);
    filterNodeEnabled = (bool) state.getProperty("filterNodeEnabled", filterNodeEnabled);
    envelopeNodeEnabled = (bool) state.getProperty("envelopeNodeEnabled", envelopeNodeEnabled);
    recipe.sineLevel = (float) state.getProperty("sineLevel", recipe.sineLevel);
    recipe.sawLevel = (float) state.getProperty("sawLevel", recipe.sawLevel);
    recipe.squareLevel = (float) state.getProperty("squareLevel", recipe.squareLevel);
    recipe.triangleLevel = (float) state.getProperty("triangleLevel", recipe.triangleLevel);
    recipe.noiseLevel = (float) state.getProperty("noiseLevel", recipe.noiseLevel);
    recipe.pitchSweepSemitones = (float) state.getProperty("pitchSweepSemitones", recipe.pitchSweepSemitones);

    recipe.envelopePoints.clear();
    recipe.automationLanes.clear();

    for (int index = 0; index < state.getNumChildren(); ++index)
    {
        auto child = state.getChild(index);
        if (child.hasType("EnvelopePoint"))
        {
            recipe.envelopePoints.add(makePoint((double) child.getProperty("time"),
                                                (double) child.getProperty("value"),
                                                child.getProperty("curve", recipe.envelopeCurveMode).toString()));
        }
        else if (child.hasType("AutomationLane"))
        {
            cw::PatchAutomationLane lane;
            lane.id = child.getProperty("id").toString();
            lane.name = child.getProperty("name").toString();
            lane.targetParameter = child.getProperty("targetParameter").toString();
            lane.interpolation = child.getProperty("interpolation", recipe.automationCurveMode).toString();
            lane.rangeMin = (double) child.getProperty("rangeMin", getTargetSpec(lane.targetParameter).rangeMin);
            lane.rangeMax = (double) child.getProperty("rangeMax", getTargetSpec(lane.targetParameter).rangeMax);
            for (int pointIndex = 0; pointIndex < child.getNumChildren(); ++pointIndex)
            {
                auto pointChild = child.getChild(pointIndex);
                lane.points.add(makePoint((double) pointChild.getProperty("time"),
                                          (double) pointChild.getProperty("value"),
                                          pointChild.getProperty("curve", lane.interpolation).toString()));
            }
            recipe.automationLanes.add(lane);
        }
    }

    ensureRecipe(recipe);
    rebuildAutomationChrome();
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
    mixNodeEnabled = false;
    filterNodeEnabled = false;
    envelopeNodeEnabled = false;
    recipe.name = document.name.isNotEmpty() ? document.name : recipe.name;
    recipe.description = document.description;

    for (const auto& parameter : document.parameters)
    {
        if (parameter.id == "baseFrequency")
            recipe.baseFrequencyHz = (float) parameter.defaultValue;
        else if (parameter.id == "filterCutoff")
            recipe.filterCutoffHz = (float) parameter.defaultValue;
        else if (parameter.id == "filterResonance")
            recipe.filterResonance = (float) parameter.defaultValue;
        else if (parameter.id == "filterEnvelopeAmount")
            recipe.filterEnvelopeAmount = (float) parameter.defaultValue;
        else if (parameter.id == "envelopeCurveMode")
            recipe.envelopeCurveMode = parameter.defaultValue >= 2.5 ? "stepped"
                                     : parameter.defaultValue <= 1.5 ? "linear"
                                                                     : "smooth";
        else if (parameter.id == "macroHardness")
            recipe.macroHardness = (float) parameter.defaultValue;
        else if (parameter.id == "macroWeight")
            recipe.macroWeight = (float) parameter.defaultValue;
        else if (parameter.id == "macroAir")
            recipe.macroAir = (float) parameter.defaultValue;
        else if (parameter.id == "macroGrit")
            recipe.macroGrit = (float) parameter.defaultValue;
        else if (parameter.id == "macroSize")
            recipe.macroSize = (float) parameter.defaultValue;
        else if (parameter.id == "noiseLevel")
            recipe.noiseLevel = (float) parameter.defaultValue;
    }

    recipe.envelopePoints.clear();
    probeSettings = {};

    // Filter/Envelope are real independent instances now (Phase 3) -- their
    // config is read per-instance below, in the node-reconstruction loop,
    // straight from each patchNode's own properties. This pass only handles
    // the stages that are still singleton (Mix, Scope, Analyzer): only
    // enable one if a node of that kind is actually present in the file --
    // buildPatchDocument only ever writes nodes that were really on the
    // canvas, so "present in the file" correctly means "the user placed
    // one", unlike the old always-emit-everything format.
    for (const auto& node : document.nodes)
    {
        if (node.kind == "mix") mixNodeEnabled = true;
        else if (node.kind == "scope") probeSettings.scopeEnabled = true;
        else if (node.kind == "analyzer") probeSettings.analyzerEnabled = true;
    }

    recipe.automationLanes = document.automationLanes;
    for (auto& lane : recipe.automationLanes)
    {
        if (lane.interpolation.isNotEmpty())
            recipe.automationCurveMode = lane.interpolation;
    }

    // Reconstruct the real graph -- same ids and positions as what was
    // saved, not fresh default-positioned nodes. rebuildNodeGraphFromRecipe()
    // (called via regenerateSignal() below) preserves oscillator/noise nodes
    // verbatim, and for the singleton mix/filter/envelope/output nodes it
    // looks up prior positions/mixer weights by their fixed id ("mix",
    // "filter", "envelope", "output") -- so seeding graphNodes here with
    // those same ids is enough for it to pick everything back up correctly,
    // without needing to duplicate that logic here.
    graphNodes.clear();
    graphConnections.clear();

    for (const auto& source : document.sources)
    {
        if (source.kind != "oscillator" && source.kind != "noise")
            continue;

        GraphNodeModel node;
        node.id = source.id;
        node.type = source.kind == "noise" ? "noise" : source.waveform;
        node.title = graphNodeTitle(node.type);
        node.position = { source.canvasX, source.canvasY };
        node.accent = graphNodeAccent(node.type);
        node.oscillatorLevel = (float) source.level;
        node.oscillatorFrequencyHz = recipe.baseFrequencyHz;
        if (source.frequencyParameter.isNotEmpty())
            for (const auto& parameter : document.parameters)
                if (parameter.id == source.frequencyParameter)
                {
                    node.oscillatorFrequencyHz = (float) parameter.defaultValue;
                    break;
                }
        graphNodes.add(node);
    }

    for (const auto& patchNode : document.nodes)
    {
        if (patchNode.kind != "mix" && patchNode.kind != "filter" && patchNode.kind != "envelope"
            && patchNode.kind != "output" && patchNode.kind != "scope" && patchNode.kind != "analyzer")
            continue;

        GraphNodeModel node;
        // Mix/Output/Scope/Analyzer are still singleton, so their fixed
        // kind-as-id keeps rebuildNodeGraphFromRecipe()'s prior-position
        // lookup working. Filter/Envelope are real independent instances
        // now (Phase 3) -- use the actual saved id so multiple instances
        // round-trip correctly instead of collapsing onto one shared id.
        node.id = (patchNode.kind == "filter" || patchNode.kind == "envelope") ? patchNode.id : patchNode.kind;
        node.type = patchNode.kind;
        node.title = graphNodeTitle(node.type);
        node.position = { patchNode.canvasX, patchNode.canvasY };
        node.accent = graphNodeAccent(node.type);
        node.required = patchNode.kind == "output";

        if (patchNode.kind == "mix")
        {
            auto weightsText = patchNode.properties.getWithDefault("channelWeights", {}).toString();
            if (weightsText.isNotEmpty())
            {
                juce::Array<float> volumes;
                for (const auto& token : juce::StringArray::fromTokens(weightsText, ",", {}))
                    volumes.add(token.getFloatValue());
                if (! volumes.isEmpty())
                    node.mixerInputVolumes = volumes;
            }
        }
        else if (patchNode.kind == "filter")
        {
            node.filterMode = patchNode.properties.getWithDefault("mode", node.filterMode).toString();
            node.filterCutoffHz = (float) patchNode.properties.getWithDefault("cutoffHz", node.filterCutoffHz);
            node.filterResonance = (float) patchNode.properties.getWithDefault("resonance", node.filterResonance);
            node.filterEnvelopeAmount = (float) patchNode.properties.getWithDefault("envelopeAmount", node.filterEnvelopeAmount);
        }
        else if (patchNode.kind == "envelope")
        {
            node.envelopeCurveMode = patchNode.properties.getWithDefault("curveMode", node.envelopeCurveMode).toString();
            auto pointsJson = patchNode.properties.getWithDefault("pointsJson", {}).toString();
            if (pointsJson.isNotEmpty())
                node.envelopePoints = parsePointsJson(pointsJson, node.envelopeCurveMode);
            if (node.envelopePoints.isEmpty())
                node.envelopePoints = makeDefaultEnvelopePoints(node.envelopeCurveMode);
        }

        graphNodes.add(node);
    }

    for (const auto& connection : document.connections)
    {
        GraphConnection graphConnection;
        graphConnection.id = "conn:" + juce::String(juce::Random::getSystemRandom().nextInt64());
        graphConnection.fromNodeId = connection.from;
        graphConnection.toNodeId = connection.to;
        graphConnection.fromPortId = connection.fromPort.isNotEmpty() ? connection.fromPort : "signalOut";
        graphConnection.toPortId = connection.toPort.isNotEmpty() ? connection.toPort : "signalIn";
        graphConnection.isExec = true;
        graphConnection.waypoints = connection.waypoints;
        graphConnections.add(graphConnection);
    }

    ensureRecipe(recipe);
    rebuildAutomationChrome();
    refreshControlsFromRecipe();
    regenerateSignal();
    return true;
}

void SignalLabPanel::applyAiTemplate(const juce::String& templateName)
{
    auto selected = templateName.trim();
    if (selected.isEmpty() || selected == "Custom")
        return;

    applyTemplate(selected);
}

bool SignalLabPanel::previewCurrentSignal()
{
    ensureAudioRendered();
    if (generatedBuffer.getNumSamples() <= 0 || onPreviewRequested == nullptr)
        return false;

    onPreviewRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    return true;
}

void SignalLabPanel::timerCallback()
{
    if (! repeatEnabled)
    {
        stopTimer();
        return;
    }

    triggerTransportPlay();
}

juce::Array<SignalLabPanel::GraphValidationError> SignalLabPanel::validateGraph() const
{
    juce::Array<GraphValidationError> errors;

    auto isSourceType = [](const juce::String& type)
    {
        return type == "sine" || type == "saw" || type == "square" || type == "triangle" || type == "noise";
    };

    // Cycle detection over the signal-carrying (isExec) edges only -- value
    // wires (a Get-variable feeding a parameter port) legitimately fan out
    // to many nodes and aren't part of the audio path, so they can't form a
    // signal cycle.
    enum class VisitState { Unvisited, Visiting, Done };
    juce::HashMap<juce::String, VisitState> visitState;
    for (auto& node : graphNodes)
        visitState.set(node.id, VisitState::Unvisited);

    std::function<bool(const juce::String&)> visit = [&](const juce::String& nodeId) -> bool
    {
        visitState.set(nodeId, VisitState::Visiting);
        for (auto& connection : graphConnections)
        {
            if (! connection.isExec || connection.fromNodeId != nodeId)
                continue;

            auto nextState = visitState[connection.toNodeId];
            if (nextState == VisitState::Visiting)
            {
                for (auto& node : graphNodes)
                {
                    if (node.id == connection.toNodeId)
                    {
                        errors.add({ node.id, "Cycle detected in the signal path through \"" + node.title + "\"." });
                        break;
                    }
                }
                return true;
            }
            if (nextState == VisitState::Unvisited && visit(connection.toNodeId))
                return true;
        }
        visitState.set(nodeId, VisitState::Done);
        return false;
    };

    for (auto& node : graphNodes)
        if (visitState[node.id] == VisitState::Unvisited)
            visit(node.id);

    // Every required node (today, just the Sink) must be reachable from at
    // least one oscillator/noise source by walking signal connections
    // backwards -- otherwise it renders silence no matter what else is wired.
    for (auto& sinkNode : graphNodes)
    {
        if (! sinkNode.required)
            continue;

        juce::Array<juce::String> visited;
        juce::Array<juce::String> frontier;
        frontier.add(sinkNode.id);
        bool sourceFound = false;

        while (! frontier.isEmpty() && ! sourceFound)
        {
            auto currentId = frontier.removeAndReturn(frontier.size() - 1);
            if (visited.contains(currentId))
                continue;
            visited.add(currentId);

            for (auto& connection : graphConnections)
            {
                if (! connection.isExec || connection.toNodeId != currentId)
                    continue;

                for (auto& candidate : graphNodes)
                {
                    if (candidate.id != connection.fromNodeId)
                        continue;
                    if (isSourceType(candidate.type))
                        sourceFound = true;
                    break;
                }

                if (sourceFound)
                    break;

                frontier.add(connection.fromNodeId);
            }
        }

        if (! sourceFound)
            errors.add({ sinkNode.id, "\"" + sinkNode.title + "\" has no oscillator or noise source feeding it -- it will render silence." });
    }

    // Placed processing nodes with nothing wired into their signal input are
    // dead weight in the graph -- flag them so they're easy to find.
    for (auto& node : graphNodes)
    {
        bool needsSignalIn = node.type == "mix" || node.type == "filter" || node.type == "envelope"
                           || node.type == "scope" || node.type == "analyzer";
        if (! needsSignalIn)
            continue;

        bool hasIncomingSignal = false;
        for (auto& connection : graphConnections)
        {
            if (connection.isExec && connection.toNodeId == node.id)
            {
                hasIncomingSignal = true;
                break;
            }
        }

        if (! hasIncomingSignal)
            errors.add({ node.id, "\"" + node.title + "\" has no signal wired into it." });
    }

    return errors;
}

void SignalLabPanel::compileGraph()
{
    validationErrors = validateGraph();
    showCompileErrorWindow();
}

void SignalLabPanel::showCompileErrorWindow()
{
    // Non-intrusive by default: a clean graph doesn't pop a window open on
    // every Play. But if the user already has the log open (inspecting it
    // while they keep editing), it stays open and refreshes in place rather
    // than disappearing out from under them.
    if (validationErrors.isEmpty() && compileErrorWindow == nullptr)
        return;

    if (compileErrorWindow == nullptr)
    {
        auto window = std::make_unique<SignalLabNodeWindow>("Signal Lab -- Compile Results", [this]
        {
            compileErrorWindow.reset();
        });
        window->setContentOwned(new CompileErrorListContent(), true);
        window->centreWithSize(440, 320);
        compileErrorWindow = std::move(window);
    }

    juce::Array<CompileErrorRow> rows;
    for (auto& error : validationErrors)
        rows.add({ error.nodeId, error.message });

    if (auto* content = dynamic_cast<CompileErrorListContent*>(compileErrorWindow->getContentComponent()))
    {
        content->setErrors(rows, [this](const juce::String& nodeId)
        {
            centerCanvasOnGraphNode(nodeId);
        });
    }

    compileErrorWindow->setVisible(true);
    compileErrorWindow->toFront(true);
}

void SignalLabPanel::centerCanvasOnGraphNode(const juce::String& nodeId)
{
    for (int index = 0; index < graphNodes.size(); ++index)
    {
        if (graphNodes.getReference(index).id != nodeId)
            continue;

        setSelectedGraphNodeIndex(index);

        auto centreGraphPoint = getGraphNodeBounds(index).getCentre();
        auto centreCanvasPoint = graphToCanvas(centreGraphPoint);
        graphViewport.setViewPosition(centreCanvasPoint.x - graphViewport.getWidth() / 2,
                                      centreCanvasPoint.y - graphViewport.getHeight() / 2);
        break;
    }
}

void SignalLabPanel::triggerTransportPlay()
{
    // Play drives the live engine (PatchLiveVoice), not the offline
    // renderer -- audioDirty stays untouched here on purpose (it exclusively
    // governs the separate offline Preview/Render-to-Project path via
    // ensureAudioRendered(); see the comment on liveGraphDirty in the
    // header). Compiling (topology rebuild) still only happens when the
    // graph actually changed structurally, same "compile if dirty, then
    // run" rule as before -- a MIDI Control node's value moving never sets
    // liveGraphDirty, so it never causes a rebuild here, live or offline.
    auto showBusyStatus = [this](const juce::String& text)
    {
        statusLabel.setText(text, juce::dontSendNotification);
        statusLabel.repaint();
        if (auto* peer = getPeer())
            peer->performAnyPendingRepaintsNow();
    };

    const bool wasDirty = liveGraphDirty;
    if (wasDirty)
    {
        juce::MouseCursor::showWaitCursor();
        showBusyStatus("Compiling...");
        compileGraph();
        if (onLiveGraphRebuildRequested)
            onLiveGraphRebuildRequested(buildPatchDocument(recipe), buildLiveBindingMap());
        liveGraphDirty = false;
        showBusyStatus({});
        juce::MouseCursor::hideWaitCursor();
    }

    if (onLiveStartRequested)
        onLiveStartRequested(recipe.durationSeconds);

    updateStatusText();

    if (repeatEnabled)
    {
        auto totalSeconds = juce::jmax(0.001, recipe.durationSeconds + repeatDelaySeconds);
        startTimer(juce::jmax(1, juce::roundToInt(totalSeconds * 1000.0)));
    }
}

void SignalLabPanel::stopTransport()
{
    stopTimer();
    repeatEnabled = false;
    repeatButton.setToggleState(false, juce::dontSendNotification);
    if (onLiveStopRequested)
        onLiveStopRequested();
    if (onStopRequested)
        onStopRequested();
}

void SignalLabPanel::updateCanvasWorkspace()
{
    constexpr int nodeWidth = 180;
    constexpr int nodeHeight = 96;
    constexpr int margin = 320;

    juce::ignoreUnused(margin);

    constexpr int planeHalfWidth = 30000;
    constexpr int planeHalfHeight = 22000;

    canvasWorkspaceSize = { planeHalfWidth * 2, planeHalfHeight * 2 };
    graphOrigin = { planeHalfWidth, planeHalfHeight };
    canvasPixelOffset = {};

    nodeGraphCanvas.setSize(canvasWorkspaceSize.x, canvasWorkspaceSize.y);

    if (! graphViewportInitialized && graphViewport.getWidth() > 0 && graphViewport.getHeight() > 0)
    {
        auto centrePoint = graphToCanvas(juce::Point<int> { 0, 0 });
        graphViewport.setViewPosition(centrePoint.x - graphViewport.getWidth() / 2,
                                      centrePoint.y - graphViewport.getHeight() / 2);
        graphViewportInitialized = true;
    }

    nodeGraphCanvas.repaint();
}

juce::Point<int> SignalLabPanel::graphToCanvas(juce::Point<int> position) const
{
    return { graphOrigin.x + (int) std::round(position.x * canvasZoom),
             graphOrigin.y + (int) std::round(position.y * canvasZoom) };
}

juce::Point<float> SignalLabPanel::graphToCanvas(juce::Point<float> position) const
{
    return { (float) graphOrigin.x + position.x * canvasZoom,
             (float) graphOrigin.y + position.y * canvasZoom };
}

juce::Rectangle<int> SignalLabPanel::graphToCanvas(juce::Rectangle<int> bounds) const
{
    auto topLeft = graphToCanvas(bounds.getPosition());
    return { topLeft.x,
             topLeft.y,
             (int) std::round(bounds.getWidth() * canvasZoom),
             (int) std::round(bounds.getHeight() * canvasZoom) };
}

juce::Point<int> SignalLabPanel::canvasToGraph(juce::Point<int> position) const
{
    return { (int) std::round((position.x - graphOrigin.x) / juce::jmax(0.001f, canvasZoom)),
             (int) std::round((position.y - graphOrigin.y) / juce::jmax(0.001f, canvasZoom)) };
}

juce::Point<float> SignalLabPanel::canvasToGraph(juce::Point<float> position) const
{
    return { (position.x - (float) graphOrigin.x) / juce::jmax(0.001f, canvasZoom),
             (position.y - (float) graphOrigin.y) / juce::jmax(0.001f, canvasZoom) };
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
    signalMenuButton.setBounds(topBar.removeFromLeft(110));
    topBar.removeFromLeft(10);
    auto transportArea = topBar.removeFromLeft(546);
    compileButton.setBounds(transportArea.removeFromLeft(80));
    transportArea.removeFromLeft(8);
    playButton.setBounds(transportArea.removeFromLeft(80));
    transportArea.removeFromLeft(8);
    stopButton.setBounds(transportArea.removeFromLeft(80));
    transportArea.removeFromLeft(8);
    repeatButton.setBounds(transportArea.removeFromLeft(80));
    transportArea.removeFromLeft(8);
    repeatDelaySlider.setBounds(transportArea.removeFromLeft(180));

    area.removeFromTop(10);
    auto propertiesArea = area.removeFromLeft(280);
    propertiesHeaderLabel.setBounds(propertiesArea.removeFromTop(28));
    propertiesArea.removeFromTop(8);

    auto signalArea = propertiesArea.removeFromTop(210);
    signalPropertiesPanel.setBounds(signalArea);
    auto signalContent = signalArea.reduced(12);
    signalSectionLabel.setBounds(signalContent.removeFromTop(22));
    signalContent.removeFromTop(6);
    nameLabel.setVisible(true);
    nameEditor.setVisible(true);
    nameLabel.setBounds(signalContent.removeFromTop(20));
    signalContent.removeFromTop(4);
    nameEditor.setBounds(signalContent.removeFromTop(26));
    signalContent.removeFromTop(8);
    descriptionLabel.setBounds(signalContent.removeFromTop(20));
    signalContent.removeFromTop(4);
    descriptionEditor.setBounds(signalContent.removeFromTop(50));
    signalContent.removeFromTop(8);
    signalMetaLabel.setBounds(signalContent);

    propertiesArea.removeFromTop(10);
    auto variableSectionHeight = juce::jmax(160, propertiesArea.getHeight() / 2);
    auto variableArea = propertiesArea.removeFromTop(variableSectionHeight);
    variablesPanel.setBounds(variableArea);
    auto variableContent = variableArea.reduced(12);
    variablesSectionLabel.setBounds(variableContent.removeFromTop(22));
    variableContent.removeFromTop(6);
    addLocalControlButton.setVisible(true);
    addLocalControlButton.setBounds(variableContent.removeFromTop(26));
    variableContent.removeFromTop(6);
    variablesViewport.setBounds(variableContent);
    toolboxPane.setSize(variableContent.getWidth(), juce::jmax(variableContent.getHeight(), toolboxPane.getRequiredHeight()));

    propertiesArea.removeFromTop(10);
    variableDetailsPanel.setBounds(propertiesArea);
    auto detailContent = propertiesArea.reduced(12);
    selectedVariableSectionLabel.setBounds(detailContent.removeFromTop(22));
    detailContent.removeFromTop(6);
    variableDetailsViewport.setBounds(detailContent);

    auto layoutDetailField = [](juce::Rectangle<int>& content, juce::Component& label, juce::Component& field, int fieldHeight)
    {
        label.setBounds(content.removeFromTop(20));
        content.removeFromTop(4);
        field.setBounds(content.removeFromTop(fieldHeight));
        content.removeFromTop(6);
    };

    auto scratchArea = juce::Rectangle<int>(0, 0, juce::jmax(1, detailContent.getWidth()), 4000);
    layoutDetailField(scratchArea, variableNameLabel, variableNameEditor, 24);
    layoutDetailField(scratchArea, variableDescriptionLabel, variableDescriptionEditor, 44);
    layoutDetailField(scratchArea, variableTypeLabel, variableTypeSelector, 24);
    layoutDetailField(scratchArea, variableAccessLabel, variableAccessSelector, 24);
    layoutDetailField(scratchArea, variableValueLabel, variableValueEditor, 24);
    variableAutomationToggle.setBounds(scratchArea.removeFromTop(24));

    variableDetailsContent.setSize(detailContent.getWidth(), 4000 - scratchArea.getHeight());

    area.removeFromLeft(10);
    graphViewport.setBounds(area);

    if (! graphViewportInitialized && graphViewport.getWidth() > 0 && graphViewport.getHeight() > 0)
    {
        auto centrePoint = graphToCanvas(juce::Point<int> { 0, 0 });
        graphViewport.setViewPosition(centrePoint.x - graphViewport.getWidth() / 2,
                                      centrePoint.y - graphViewport.getHeight() / 2);
        graphViewportInitialized = true;
    }

    layoutFloatingWindows();
}

void SignalLabPanel::configureSlider(juce::Slider& slider, double min, double max, double step)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 22);
    slider.setRange(min, max, step);
    addAndMakeVisible(slider);
}

void SignalLabPanel::layoutFloatingWindows()
{
    auto hideAllNodeControls = [this]()
    {
        inspectorTitleLabel.setVisible(false);
        inspectorBodyLabel.setVisible(false);
        frequencyLabel.setVisible(false); frequencySlider.setVisible(false);
        durationLabel.setVisible(false); durationSlider.setVisible(false);
        pitchLabel.setVisible(false); pitchSlider.setVisible(false);
        filterModeLabel.setVisible(false); filterModeSelector.setVisible(false);
        filterCutoffLabel.setVisible(false); filterCutoffSlider.setVisible(false);
        filterResonanceLabel.setVisible(false); filterResonanceSlider.setVisible(false);
        filterEnvelopeLabel.setVisible(false); filterEnvelopeSlider.setVisible(false);
        envelopeCurveLabel.setVisible(false); envelopeCurveSelector.setVisible(false);
        automationCurveLabel.setVisible(false); automationCurveSelector.setVisible(false);
        macroHardnessLabel.setVisible(false); macroHardnessSlider.setVisible(false);
        macroWeightLabel.setVisible(false); macroWeightSlider.setVisible(false);
        macroAirLabel.setVisible(false); macroAirSlider.setVisible(false);
        macroGritLabel.setVisible(false); macroGritSlider.setVisible(false);
        macroSizeLabel.setVisible(false); macroSizeSlider.setVisible(false);
        sineLabel.setVisible(false); sineSlider.setVisible(false);
        sawLabel.setVisible(false); sawSlider.setVisible(false);
        squareLabel.setVisible(false); squareSlider.setVisible(false);
        triangleLabel.setVisible(false); triangleSlider.setVisible(false);
        noiseLabel.setVisible(false); noiseSlider.setVisible(false);
        probeControlALabel.setVisible(false); probeControlASlider.setVisible(false);
        probeControlBLabel.setVisible(false); probeControlBSlider.setVisible(false);
        probeControlCLabel.setVisible(false); probeControlCSlider.setVisible(false);
        probeControlDLabel.setVisible(false); probeControlDSlider.setVisible(false);
        envelopeEditor.setVisible(false);
        automationViewport.setVisible(false);
        addAutomationLaneButton.setVisible(false);
        scopePanel.setVisible(false);
        spectrumPanel.setVisible(false);
    };

    hideAllNodeControls();

    nodeEditorWindow.setVisible(false);
    nodeEditorCloseButton.setVisible(false);
    controlPadWindow.setVisible(false);
    controlPadCloseButton.setVisible(false);
}

void SignalLabPanel::regenerateSignal()
{
    // Cheap path only -- graph/UI bookkeeping so the node cards, status
    // text, and inspector always stay in sync live as you edit. The actual
    // DSP render is expensive (multi-second) and is deliberately NOT done
    // here: doing it on every edit is what made dragging an envelope point
    // or adding a node feel like it hung. Rendering happens on demand, via
    // ensureAudioRendered(), only at the handful of places that actually
    // need audio (Play, Preview, Render, opening a Scope/Analyzer window).
    ensureRecipe(recipe);
    audioDirty = true;
    liveGraphDirty = true;
    updateStatusText();
    rebuildNodeGraphFromRecipe();
    updateInspectorForSelection();
}

void SignalLabPanel::applyLiveMidiControlChanges(const juce::Array<MidiControlChange>& changes)
{
    if (changes.isEmpty())
        return;

    bool anyApplied = false;
    for (auto& node : graphNodes)
    {
        if ((node.type != "midiFader" && node.type != "midiButton") || ! node.midiLearned)
            continue;

        for (auto& change : changes)
        {
            if (change.channel != node.midiChannel || change.number != node.midiNumber
                || change.isController != node.midiIsController)
                continue;
            // Empty midiDeviceId means "any device", matching how the learn
            // dialog's device combo works -- otherwise it must be this exact
            // device (the same physical control from two devices shouldn't
            // both drive a binding that was learned from one specific one).
            if (node.midiDeviceId.isNotEmpty() && change.deviceId != node.midiDeviceId)
                continue;

            if (node.type == "midiFader")
            {
                node.midiLiveValue = change.value;
            }
            else if (node.midiButtonMode == "toggle")
            {
                // Edge-triggered: flip only on the press, ignore the
                // matching release -- otherwise every press+release pair
                // would flip twice and the toggle would never visibly latch.
                if (change.value > 0.5f)
                    node.midiLiveValue = node.midiLiveValue > 0.5f ? 0.0f : 1.0f;
            }
            else
            {
                node.midiLiveValue = change.value > 0.5f ? 1.0f : 0.0f;
            }

            // Straight through to the currently-playing live voice, if any
            // -- this is the actual point of this whole feature: a value
            // change reaches already-playing sound immediately, with no
            // rebuild, no Play press, no compile step. If nothing is
            // playing right now this is a harmless no-op (PatchLiveVoice
            // ignores an unregistered/inactive node id); the value still
            // reaches the next Play through the normal rebuild path since
            // node.midiLiveValue seeds a fresh slot at rebuild() time.
            if (onLiveMidiValueChanged)
                onLiveMidiValueChanged(node.id, node.midiLiveValue);

            anyApplied = true;
        }
    }

    if (! anyApplied)
        return;

    // Deliberately NOT liveGraphDirty -- a value-only change never needs a
    // topology rebuild, live or offline; that's the entire point of this
    // method. audioDirty is still set so the *offline* Preview/
    // Render-to-Project path (a completely separate use case, unrelated to
    // whether anything is live-playing right now) reflects the new value
    // whenever it's next used.
    audioDirty = true;
    nodeGraphCanvas.repaint();
}

void SignalLabPanel::ensureAudioRendered()
{
    if (! audioDirty)
        return;

    auto patchDocument = buildPatchDocument(recipe);
    juce::String errorMessage;
    nodeTapBuffers.clear();
    if (! runtimePlayer.renderPatchToBuffer(patchDocument, recipe.durationSeconds, generatedBuffer, errorMessage, &nodeTapBuffers))
        generatedBuffer = buildSignalBuffer(recipe);

    envelopeEditor.setRecipe(recipe);
    scopePanel.setBuffer(generatedBuffer);
    spectrumPanel.setBuffer(generatedBuffer, recipe.sampleRate);
    audioDirty = false;

    // The Scope/Analyzer nodes' inline mini-trace on the canvas reads
    // generatedBuffer directly during paint() -- nothing else repaints that
    // canvas after a render happens now that rendering is deferred out of
    // every edit, so without this the trace stays blank even though audio
    // played correctly.
    nodeGraphCanvas.repaint();
}

const juce::AudioBuffer<float>& SignalLabPanel::getDisplayBufferForNode(const juce::String& nodeId) const
{
    for (auto& tap : nodeTapBuffers)
        if (tap.nodeId == nodeId)
            return tap.buffer;

    // Not tapped to anything specific yet (e.g. before the first render) --
    // fall back to the final output rather than showing nothing.
    return generatedBuffer;
}

juce::AudioBuffer<float> SignalLabPanel::buildSignalBuffer(const SignalRecipe& activeRecipe) const
{
    auto recipeCopy = activeRecipe;
    ensureRecipe(recipeCopy);

    auto numSamples = juce::jmax(1, juce::roundToInt(recipeCopy.durationSeconds * recipeCopy.sampleRate));
    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.clear();

    juce::Random random(0x5349474E);
    auto totalLevel = recipeCopy.sineLevel + recipeCopy.sawLevel + recipeCopy.squareLevel + recipeCopy.triangleLevel + recipeCopy.noiseLevel;
    auto normalizer = totalLevel > 0.0f ? (0.9f / totalLevel) : 0.0f;

    auto envelopePoints = recipeCopy.envelopePoints;
    ensureEnvelopePoints(envelopePoints, recipeCopy.envelopeCurveMode);
    for (int index = 1; index < envelopePoints.size() - 1; ++index)
    {
        double timeShift = index == 1
                         ? juce::jmap((double) recipeCopy.macroHardness, 0.0, 1.0, 0.05, -0.05)
                         : juce::jmap((double) recipeCopy.macroSize, 0.0, 1.0, -0.04, 0.08);
        envelopePoints.getReference(index).time += timeShift;
        envelopePoints.getReference(index).value = juce::jlimit(0.0,
                                                                1.0,
                                                                envelopePoints.getReference(index).value
                                                                    + juce::jmap((double) recipeCopy.macroWeight, 0.0, 1.0, -0.08, 0.12));
    }
    ensureEnvelopePoints(envelopePoints, recipeCopy.envelopeCurveMode);

    float filterState = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto t = (double) sample / (double) juce::jmax(1, numSamples - 1);
        auto pitchMotion = sampleTargetLanes(recipeCopy.automationLanes, "pitchOffsetSemitones", t, 0.0);
        auto gainMotion = sampleTargetLanes(recipeCopy.automationLanes, "outputGain", t, 1.0);
        auto filterMotion = sampleTargetLanes(recipeCopy.automationLanes, "filterCutoff", t, 0.5);
        auto resonanceMotion = sampleTargetLanes(recipeCopy.automationLanes, "filterResonance", t, recipeCopy.filterResonance);
        auto filterEnvelopeMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "filterEnvelopeAmount", t, recipeCopy.filterEnvelopeAmount);
        auto noiseMotion = sampleTargetLanes(recipeCopy.automationLanes, "noiseLevel", t, recipeCopy.noiseLevel);
        auto baseFrequencyMotion = sampleTargetLanes(recipeCopy.automationLanes, "baseFrequency", t, recipeCopy.baseFrequencyHz);
        auto sineLevelMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "sineLevel", t, recipeCopy.sineLevel);
        auto sawLevelMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "sawLevel", t, recipeCopy.sawLevel);
        auto squareLevelMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "squareLevel", t, recipeCopy.squareLevel);
        auto triangleLevelMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "triangleLevel", t, recipeCopy.triangleLevel);
        auto hardnessMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "macroHardness", t, recipeCopy.macroHardness);
        auto weightMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "macroWeight", t, recipeCopy.macroWeight);
        auto airMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "macroAir", t, recipeCopy.macroAir);
        auto gritMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "macroGrit", t, recipeCopy.macroGrit);
        auto sizeMotion = (float) sampleTargetLanes(recipeCopy.automationLanes, "macroSize", t, recipeCopy.macroSize);
        auto pitchSemitones = recipeCopy.pitchSweepSemitones
                            + (float) pitchMotion
                            + juce::jmap(weightMotion, 0.0f, 1.0f, 2.0f, -2.0f) * ((float) t - 0.5f) * 2.0f;
        auto baseFrequency = (float) baseFrequencyMotion
                           * juce::jmap(weightMotion, 0.0f, 1.0f, 1.16f, 0.86f)
                           * juce::jmap(sizeMotion, 0.0f, 1.0f, 1.04f, 0.94f);
        auto frequency = baseFrequency * std::pow(2.0f, pitchSemitones / 12.0f);
        auto phase = juce::MathConstants<float>::twoPi * frequency * ((float) sample / (float) recipeCopy.sampleRate);

        auto sine = std::sin(phase);
        auto saw = 2.0f * (phase / juce::MathConstants<float>::twoPi - std::floor(0.5f + phase / juce::MathConstants<float>::twoPi));
        auto square = std::sin(phase) >= 0.0f ? 1.0f : -1.0f;
        auto triangle = std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
        auto noise = random.nextFloat() * 2.0f - 1.0f;
        auto macroNoise = juce::jlimit(0.0f, 1.0f, (float) noiseMotion + airMotion * 0.18f + gritMotion * 0.12f);
        auto gritDrive = 1.0f + gritMotion * 5.5f;
        auto envelope = (float) sampleLane(envelopePoints, recipeCopy.envelopeCurveMode, t, 0.0);

        auto sampleValue = normalizer * envelope * (float) gainMotion
                         * ((sineLevelMotion + weightMotion * 0.10f) * sine
                            + (sawLevelMotion + gritMotion * 0.14f) * saw
                            + (squareLevelMotion + hardnessMotion * 0.12f) * square
                            + (triangleLevelMotion + weightMotion * 0.08f) * triangle
                            + macroNoise * noise);

        sampleValue = std::tanh(sampleValue * gritDrive) / std::tanh(gritDrive);

        auto filterNormalized = clamp01(cutoffToNormalized(recipeCopy.filterCutoffHz)
                                        + ((float) filterMotion - 0.5f) * 0.75f
                                        + (envelope - 0.5f) * (filterEnvelopeMotion + hardnessMotion * 0.30f)
                                        + airMotion * 0.18f
                                        - weightMotion * 0.10f
                                        - sizeMotion * 0.06f);
        auto brightnessEquivalent = juce::jmap(filterNormalized, 0.0f, 1.0f, 0.02f, 1.0f)
                                  * juce::jmap(sizeMotion, 0.0f, 1.0f, 1.02f, 0.90f);
        sampleValue = applyBrightnessFilter(sampleValue, filterState, brightnessEquivalent, recipeCopy.sampleRate);
        sampleValue *= juce::jlimit(0.6f, 1.4f, (float) resonanceMotion / juce::jmax(0.30f, recipeCopy.filterResonance));

        buffer.setSample(0, sample, sampleValue);
        buffer.setSample(1, sample, sampleValue);
    }

    return buffer;
}

cw::PatchDocument SignalLabPanel::buildPatchDocument(const SignalRecipe& activeRecipe) const
{
    auto recipeCopy = activeRecipe;
    ensureRecipe(recipeCopy);

    cw::PatchDocument document;
    document.patchId = cw::makePatchId(recipeCopy.name);
    document.name = recipeCopy.name;
    document.type = "instrument";
    document.description = recipeCopy.description;
    document.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    document.updatedAt = document.createdAt;

    document.parameters.add({ "baseFrequency", "Base Frequency", "float", recipeCopy.baseFrequencyHz, 30.0, 2400.0, "hz" });
    document.parameters.add({ "envelopeCurveMode", "Envelope Curve", "float", recipeCopy.envelopeCurveMode == "stepped" ? 3.0 : recipeCopy.envelopeCurveMode == "linear" ? 1.0 : 2.0, 1.0, 3.0, "mode" });
    document.parameters.add({ "filterCutoff", "Filter Cutoff", "float", recipeCopy.filterCutoffHz, kMinFilterCutoffHz, kMaxFilterCutoffHz, "hz" });
    document.parameters.add({ "filterResonance", "Filter Resonance", "float", recipeCopy.filterResonance, 0.30, 8.0, "q" });
    document.parameters.add({ "filterEnvelopeAmount", "Filter Envelope", "float", recipeCopy.filterEnvelopeAmount, -1.0, 1.0, "normalized" });
    document.parameters.add({ "macroHardness", "Hardness", "float", recipeCopy.macroHardness, 0.0, 1.0, "normalized" });
    document.parameters.add({ "macroWeight", "Weight", "float", recipeCopy.macroWeight, 0.0, 1.0, "normalized" });
    document.parameters.add({ "macroAir", "Air", "float", recipeCopy.macroAir, 0.0, 1.0, "normalized" });
    document.parameters.add({ "macroGrit", "Grit", "float", recipeCopy.macroGrit, 0.0, 1.0, "normalized" });
    document.parameters.add({ "macroSize", "Size", "float", recipeCopy.macroSize, 0.0, 1.0, "normalized" });
    document.parameters.add({ "pitchOffsetSemitones", "Pitch Offset", "float", 0.0, -12.0, 12.0, "semitones" });
    document.parameters.add({ "outputGain", "Output Gain", "float", 1.0, 0.0, 1.0, "normalized" });
    document.parameters.add({ "noiseLevel", "Noise Level", "float", recipeCopy.noiseLevel, 0.0, 1.0, "normalized" });

    document.automationLanes = recipeCopy.automationLanes;
    for (auto& lane : document.automationLanes)
        ensureLane(lane, recipeCopy.automationCurveMode);

    // One entry per node actually on the graph, using its real id and
    // canvas position -- not a fixed osc/mix1/filter1/env1 skeleton. If you
    // didn't place a Filter, no filter node goes into this document, and
    // the renderer (which now walks this same structure) won't apply one.
    for (auto& node : graphNodes)
    {
        if (node.type == "sine" || node.type == "saw" || node.type == "square" || node.type == "triangle")
        {
            double wiredLevel = 0.0;
            auto effectiveLevel = findWiredParameterValue(node.id, "param:" + node.type + "Level", wiredLevel)
                                 ? (float) wiredLevel : node.oscillatorLevel;
            if (effectiveLevel <= 0.0f)
                continue;
            auto frequencyParamId = "frequency:" + node.id;
            document.parameters.add({ frequencyParamId, node.title + " Frequency", "float", node.oscillatorFrequencyHz, 30.0, 2400.0, "hz" });
            cw::PatchSource source { node.id, "oscillator", node.type, {}, effectiveLevel, frequencyParamId };
            source.canvasX = node.position.x;
            source.canvasY = node.position.y;
            document.sources.add(source);
        }
        else if (node.type == "noise")
        {
            double wiredLevel = 0.0;
            auto effectiveLevel = findWiredParameterValue(node.id, "param:noiseLevel", wiredLevel)
                                 ? (float) wiredLevel : node.oscillatorLevel;
            if (effectiveLevel <= 0.0f)
                continue;
            cw::PatchSource source { node.id, "noise", {}, "white", effectiveLevel, {} };
            source.canvasX = node.position.x;
            source.canvasY = node.position.y;
            document.sources.add(source);
        }
        else if (node.type == "mix" || node.type == "filter" || node.type == "envelope"
                 || node.type == "output" || node.type == "scope" || node.type == "analyzer")
        {
            cw::PatchNode patchNode;
            patchNode.id = node.id;
            patchNode.kind = node.type;
            patchNode.canvasX = node.position.x;
            patchNode.canvasY = node.position.y;

            if (node.type == "mix")
            {
                juce::StringArray weights;
                for (auto volume : node.mixerInputVolumes)
                    weights.add(juce::String(volume, 4));
                patchNode.properties.set("channelWeights", weights.joinIntoString(","));
            }
            else if (node.type == "filter")
            {
                double wiredValue = 0.0;
                patchNode.properties.set("mode", node.filterMode);
                patchNode.properties.set("cutoffHz", findWiredParameterValue(node.id, "param:filterCutoff", wiredValue)
                                                    ? wiredValue : (double) node.filterCutoffHz);
                patchNode.properties.set("resonance", findWiredParameterValue(node.id, "param:filterResonance", wiredValue)
                                                     ? wiredValue : (double) node.filterResonance);
                patchNode.properties.set("envelopeAmount", findWiredParameterValue(node.id, "param:filterEnvelopeAmount", wiredValue)
                                                          ? wiredValue : (double) node.filterEnvelopeAmount);
            }
            else if (node.type == "envelope")
            {
                patchNode.properties.set("curveMode", node.envelopeCurveMode);
                patchNode.properties.set("pointsJson", serialisePointsJson(node.envelopePoints));
            }

            document.nodes.add(patchNode);
        }
    }

    // Mirror the real wires from the canvas -- only signal (white/exec)
    // connections matter to the audio graph; value-port wiring (Get/Set,
    // filter cutoff knobs, etc.) isn't part of this signal-path document.
    for (auto& connection : graphConnections)
    {
        if (! connection.isExec)
            continue;

        cw::PatchConnection patchConnection;
        patchConnection.from = connection.fromNodeId;
        patchConnection.to = connection.toNodeId;
        patchConnection.fromPort = connection.fromPortId;
        patchConnection.toPort = connection.toPortId;
        patchConnection.weight = 1.0;
        patchConnection.waypoints = connection.waypoints;

        if (connection.toPortId.startsWith("signalIn:"))
        {
            for (auto& targetNode : graphNodes)
            {
                if (targetNode.id != connection.toNodeId || targetNode.type != "mix")
                    continue;
                auto channelIndex = connection.toPortId.fromFirstOccurrenceOf(":", false, false).getIntValue();
                double wiredWeight = 0.0;
                if (findWiredParameterValue(targetNode.id, "mixWeight:" + juce::String(channelIndex), wiredWeight))
                    patchConnection.weight = wiredWeight;
                else if (channelIndex >= 0 && channelIndex < targetNode.mixerInputVolumes.size())
                    patchConnection.weight = targetNode.mixerInputVolumes[channelIndex];
                break;
            }
        }

        document.connections.add(patchConnection);
    }

    document.output.channelMode = "stereo";
    document.output.gain = 0.9;
    document.output.pan = 0.0;

    return document;
}

// Sibling to buildPatchDocument(), built from the same graph at the same
// moment (call both together) -- buildPatchDocument() already resolves a
// wired MIDI value into a plain baked scalar (findWiredParameterValue), but
// deliberately doesn't record *which* node it came from, since the
// document is meant to be a portable, resolved format. PatchLiveVoice needs
// that extra "which node" information to keep reading a parameter live
// after the graph is compiled -- this is where it comes from.
PatchLiveBindingMap SignalLabPanel::buildLiveBindingMap() const
{
    PatchLiveBindingMap map;

    for (auto& node : graphNodes)
    {
        if (node.type == "midiFader" || node.type == "midiButton")
            map.midiNodeValues.add({ node.id, node.midiLiveValue });

        if (node.type == "sine" || node.type == "saw" || node.type == "square" || node.type == "triangle")
        {
            auto midiNodeId = findWiredMidiSourceNodeId(node.id, "param:" + node.type + "Level");
            if (midiNodeId.isNotEmpty())
                map.entries.add({ node.id, "level", midiNodeId });
        }
        else if (node.type == "noise")
        {
            auto midiNodeId = findWiredMidiSourceNodeId(node.id, "param:noiseLevel");
            if (midiNodeId.isNotEmpty())
                map.entries.add({ node.id, "level", midiNodeId });
        }
        else if (node.type == "filter")
        {
            auto cutoffMidiId = findWiredMidiSourceNodeId(node.id, "param:filterCutoff");
            if (cutoffMidiId.isNotEmpty())
                map.entries.add({ node.id, "cutoff", cutoffMidiId });
            auto resonanceMidiId = findWiredMidiSourceNodeId(node.id, "param:filterResonance");
            if (resonanceMidiId.isNotEmpty())
                map.entries.add({ node.id, "resonance", resonanceMidiId });
            auto envAmountMidiId = findWiredMidiSourceNodeId(node.id, "param:filterEnvelopeAmount");
            if (envAmountMidiId.isNotEmpty())
                map.entries.add({ node.id, "envelopeAmount", envAmountMidiId });
        }
        else if (node.type == "mix")
        {
            for (int channelIndex = 0; channelIndex < node.mixerInputVolumes.size(); ++channelIndex)
            {
                auto weightMidiId = findWiredMidiSourceNodeId(node.id, "mixWeight:" + juce::String(channelIndex));
                if (weightMidiId.isNotEmpty())
                    map.entries.add({ node.id, "mixWeight:" + juce::String(channelIndex), weightMidiId });
            }
        }
    }

    return map;
}

void SignalLabPanel::applyTemplate(const juce::String& templateName)
{
    recipe = {};

    if (templateName == "Soft Keys")
    {
        recipe.name = "Soft Keys";
        recipe.durationSeconds = 1.8;
        recipe.baseFrequencyHz = 220.0f;
        recipe.filterMode = "lowpass";
        recipe.filterCutoffHz = 980.0f;
        recipe.filterResonance = 0.55f;
        recipe.filterEnvelopeAmount = 0.18f;
        recipe.macroHardness = 0.24f;
        recipe.macroWeight = 0.56f;
        recipe.macroAir = 0.18f;
        recipe.macroGrit = 0.08f;
        recipe.macroSize = 0.46f;
        recipe.sineLevel = 0.72f;
        recipe.sawLevel = 0.0f;
        recipe.squareLevel = 0.0f;
        recipe.triangleLevel = 0.28f;
        recipe.noiseLevel = 0.02f;
        recipe.pitchSweepSemitones = 0.0f;
        setEnvelopeFromLegacy(recipe, 0.10f, 0.38f, 0.86f, 0.70f);
        setLaneValues(recipe, "pitchOffsetSemitones", { 0.0f, 0.0f, 0.0f, 0.0f });
        setLaneValues(recipe, "outputGain", { 1.0f, 0.88f, 0.72f, 0.0f });
        setLaneValues(recipe, "filterCutoff", { 0.44f, 0.46f, 0.42f, 0.36f });
    }
    else if (templateName == "Triangle Lead")
    {
        recipe.name = "Triangle Lead";
        recipe.durationSeconds = 1.2;
        recipe.baseFrequencyHz = 330.0f;
        recipe.filterMode = "bandpass";
        recipe.filterCutoffHz = 2600.0f;
        recipe.filterResonance = 1.85f;
        recipe.filterEnvelopeAmount = 0.42f;
        recipe.macroHardness = 0.62f;
        recipe.macroWeight = 0.42f;
        recipe.macroAir = 0.36f;
        recipe.macroGrit = 0.18f;
        recipe.macroSize = 0.30f;
        recipe.sineLevel = 0.20f;
        recipe.sawLevel = 0.12f;
        recipe.squareLevel = 0.0f;
        recipe.triangleLevel = 0.68f;
        recipe.noiseLevel = 0.0f;
        recipe.pitchSweepSemitones = 1.5f;
        setEnvelopeFromLegacy(recipe, 0.04f, 0.22f, 0.78f, 0.62f);
        setLaneValues(recipe, "pitchOffsetSemitones", { 0.55f, 0.50f, 0.48f, 0.50f });
        setLaneValues(recipe, "outputGain", { 1.0f, 0.92f, 0.76f, 0.0f });
        setLaneValues(recipe, "filterCutoff", { 0.60f, 0.68f, 0.54f, 0.48f });
    }
    else if (templateName == "Noisy Pluck")
    {
        recipe.name = "Noisy Pluck";
        recipe.durationSeconds = 0.9;
        recipe.baseFrequencyHz = 196.0f;
        recipe.filterMode = "highpass";
        recipe.filterCutoffHz = 3100.0f;
        recipe.filterResonance = 1.20f;
        recipe.filterEnvelopeAmount = 0.66f;
        recipe.macroHardness = 0.78f;
        recipe.macroWeight = 0.34f;
        recipe.macroAir = 0.44f;
        recipe.macroGrit = 0.42f;
        recipe.macroSize = 0.22f;
        recipe.sineLevel = 0.25f;
        recipe.sawLevel = 0.25f;
        recipe.squareLevel = 0.10f;
        recipe.triangleLevel = 0.20f;
        recipe.noiseLevel = 0.20f;
        recipe.pitchSweepSemitones = -4.0f;
        setEnvelopeFromLegacy(recipe, 0.02f, 0.12f, 0.52f, 0.20f);
        setLaneValues(recipe, "pitchOffsetSemitones", { 0.78f, 0.58f, 0.46f, 0.50f });
        setLaneValues(recipe, "outputGain", { 1.0f, 0.58f, 0.22f, 0.0f });
        setLaneValues(recipe, "filterCutoff", { 0.82f, 0.58f, 0.38f, 0.30f });
    }
    else if (templateName == "Drone Pad")
    {
        recipe.name = "Drone Pad";
        recipe.durationSeconds = 3.2;
        recipe.baseFrequencyHz = 110.0f;
        recipe.filterMode = "lowpass";
        recipe.filterCutoffHz = 740.0f;
        recipe.filterResonance = 0.72f;
        recipe.filterEnvelopeAmount = 0.24f;
        recipe.macroHardness = 0.20f;
        recipe.macroWeight = 0.68f;
        recipe.macroAir = 0.28f;
        recipe.macroGrit = 0.10f;
        recipe.macroSize = 0.86f;
        recipe.sineLevel = 0.44f;
        recipe.sawLevel = 0.18f;
        recipe.squareLevel = 0.06f;
        recipe.triangleLevel = 0.26f;
        recipe.noiseLevel = 0.06f;
        recipe.pitchSweepSemitones = 2.0f;
        setEnvelopeFromLegacy(recipe, 0.18f, 0.46f, 0.92f, 0.78f);
        setLaneValues(recipe, "pitchOffsetSemitones", { 0.48f, 0.52f, 0.46f, 0.50f });
        setLaneValues(recipe, "outputGain", { 0.84f, 1.0f, 0.92f, 0.0f });
        setLaneValues(recipe, "filterCutoff", { 0.34f, 0.42f, 0.40f, 0.36f });
    }
    else if (templateName == "Impact Tone")
    {
        recipe.name = "Impact Tone";
        recipe.durationSeconds = 1.1;
        recipe.baseFrequencyHz = 90.0f;
        recipe.filterMode = "bandpass";
        recipe.filterCutoffHz = 1800.0f;
        recipe.filterResonance = 2.80f;
        recipe.filterEnvelopeAmount = 0.82f;
        recipe.macroHardness = 0.92f;
        recipe.macroWeight = 0.72f;
        recipe.macroAir = 0.30f;
        recipe.macroGrit = 0.48f;
        recipe.macroSize = 0.58f;
        recipe.sineLevel = 0.36f;
        recipe.sawLevel = 0.18f;
        recipe.squareLevel = 0.14f;
        recipe.triangleLevel = 0.12f;
        recipe.noiseLevel = 0.20f;
        recipe.pitchSweepSemitones = -8.0f;
        setEnvelopeFromLegacy(recipe, 0.01f, 0.10f, 0.48f, 0.18f);
        setLaneValues(recipe, "pitchOffsetSemitones", { 0.92f, 0.64f, 0.42f, 0.50f });
        setLaneValues(recipe, "outputGain", { 1.0f, 0.52f, 0.18f, 0.0f });
        setLaneValues(recipe, "filterCutoff", { 0.88f, 0.62f, 0.34f, 0.22f });
    }

    ensureRecipe(recipe);
    rebuildAutomationChrome();
    refreshControlsFromRecipe();
    seedOscillatorNodesFromRecipeLevels();

    // Filter/Envelope are independent per-instance nodes now (Phase 3) --
    // a template only sets the legacy recipe scalars above, so if any
    // Filter/Envelope nodes are already on the canvas, push the template's
    // values into them directly. This deliberately doesn't create new
    // Filter/Envelope nodes -- templates tune character for whatever's
    // already placed, they don't add structure to the graph.
    for (auto& node : graphNodes)
    {
        if (node.type == "filter")
        {
            node.filterMode = recipe.filterMode;
            node.filterCutoffHz = recipe.filterCutoffHz;
            node.filterResonance = recipe.filterResonance;
            node.filterEnvelopeAmount = recipe.filterEnvelopeAmount;
        }
        else if (node.type == "envelope")
        {
            node.envelopeCurveMode = recipe.envelopeCurveMode;
            node.envelopePoints = recipe.envelopePoints;
        }
    }

    regenerateSignal();
}

void SignalLabPanel::refreshControlsFromRecipe()
{
    ensureRecipe(recipe);
    suppressCallbacks = true;
    nameEditor.setText(recipe.name, juce::dontSendNotification);
    descriptionEditor.setText(recipe.description, juce::dontSendNotification);
    templateSelector.setSelectedId(1, juce::dontSendNotification);
    frequencySlider.setValue(recipe.baseFrequencyHz, juce::dontSendNotification);
    durationSlider.setValue(recipe.durationSeconds, juce::dontSendNotification);
    pitchSlider.setValue(recipe.pitchSweepSemitones, juce::dontSendNotification);
    filterModeSelector.setSelectedId(recipe.filterMode == "bandpass" ? 2 : recipe.filterMode == "highpass" ? 3 : 1,
                                     juce::dontSendNotification);
    envelopeCurveSelector.setSelectedId(recipe.envelopeCurveMode == "stepped" ? 3 : recipe.envelopeCurveMode == "linear" ? 1 : 2,
                                        juce::dontSendNotification);
    automationCurveSelector.setSelectedId(recipe.automationCurveMode == "stepped" ? 3 : recipe.automationCurveMode == "linear" ? 1 : 2,
                                          juce::dontSendNotification);
    filterCutoffSlider.setValue(recipe.filterCutoffHz, juce::dontSendNotification);
    filterResonanceSlider.setValue(recipe.filterResonance, juce::dontSendNotification);
    filterEnvelopeSlider.setValue(recipe.filterEnvelopeAmount, juce::dontSendNotification);
    macroHardnessSlider.setValue(recipe.macroHardness, juce::dontSendNotification);
    macroWeightSlider.setValue(recipe.macroWeight, juce::dontSendNotification);
    macroAirSlider.setValue(recipe.macroAir, juce::dontSendNotification);
    macroGritSlider.setValue(recipe.macroGrit, juce::dontSendNotification);
    macroSizeSlider.setValue(recipe.macroSize, juce::dontSendNotification);
    sineSlider.setValue(recipe.sineLevel, juce::dontSendNotification);
    sawSlider.setValue(recipe.sawLevel, juce::dontSendNotification);
    squareSlider.setValue(recipe.squareLevel, juce::dontSendNotification);
    triangleSlider.setValue(recipe.triangleLevel, juce::dontSendNotification);
    noiseSlider.setValue(recipe.noiseLevel, juce::dontSendNotification);
    suppressCallbacks = false;

    envelopeEditor.setRecipe(recipe);
    syncAutomationEditors();
}

void SignalLabPanel::updateStatusText()
{
    // Derived from the recipe, not generatedBuffer -- the buffer is only
    // rendered on demand now (Play/Preview/Render), so it can be stale or
    // empty while this status text must stay accurate as you edit.
    auto sampleCount = juce::jmax(0, juce::roundToInt(recipe.durationSeconds * recipe.sampleRate));
    auto text = "Ready: " + recipe.name
              + "  |  " + juce::String(recipe.durationSeconds, 2) + " s"
              + "  |  " + juce::String((int) recipe.baseFrequencyHz) + " Hz"
              + "  |  " + recipe.filterMode + " " + juce::String((int) recipe.filterCutoffHz) + " Hz"
              + "  |  envelope points: " + juce::String(recipe.envelopePoints.size())
              + "  |  motion lanes: " + juce::String(recipe.automationLanes.size())
              + "  |  curves E:" + recipe.envelopeCurveMode + " A:" + recipe.automationCurveMode
              + "  |  " + juce::String(sampleCount) + " samples";
    statusLabel.setText(text, juce::dontSendNotification);
}

void SignalLabPanel::syncAutomationEditors()
{
    ensureRecipe(recipe);
    for (int index = 0; index < automationLaneEditors.size() && index < recipe.automationLanes.size(); ++index)
    {
        auto& lane = recipe.automationLanes.getReference(index);
        auto spec = getTargetSpec(lane.targetParameter);
        lane.name = spec.title;
        automationLaneEditors[index]->setLane(lane, spec.accent);
        automationLaneEditors[index]->onGestureBegin = [this] { beginUndoGesture("Move motion point"); };
        automationLaneEditors[index]->onGestureEnd = [this] { endUndoGesture(); };
        automationLaneEditors[index]->onDiscreteEditRequested = [this](const juce::String& label) { captureUndoCheckpoint(label); };
        for (int itemIndex = 0; itemIndex < (int) getAutomationTargetSpecs().size(); ++itemIndex)
        {
            if (lane.targetParameter == juce::String(getAutomationTargetSpecs()[(size_t) itemIndex].parameterId))
            {
                automationTargetSelectors[index]->setSelectedItemIndex(itemIndex, juce::dontSendNotification);
                break;
            }
        }
    }
}

void SignalLabPanel::rebuildAutomationChrome()
{
    while (automationLaneEditors.size() < recipe.automationLanes.size())
    {
        auto* editor = automationLaneEditors.add(new AutomationLaneEditor());
        editor->onLaneChanged = [this, editor](const cw::PatchAutomationLane& updatedLane)
        {
            auto index = automationLaneEditors.indexOf(editor);
            if (index >= 0 && index < recipe.automationLanes.size())
            {
                noteInteraction();
                recipe.automationLanes.getReference(index) = updatedLane;
                ensureLane(recipe.automationLanes.getReference(index), recipe.automationCurveMode);
                regenerateSignal();
            }
        };
        automationHost.addAndMakeVisible(editor);

        auto* selector = automationTargetSelectors.add(new juce::ComboBox());
        for (const auto& spec : getAutomationTargetSpecs())
            selector->addItem(spec.title, selector->getNumItems() + 1);
        selector->onChange = [this, selector]
        {
            if (suppressCallbacks)
                return;

            auto index = automationTargetSelectors.indexOf(selector);
            if (index >= 0 && index < recipe.automationLanes.size())
            {
                captureUndoCheckpoint("Retarget motion lane");
                auto selectedSpec = getAutomationTargetSpecs()[(size_t) juce::jmax(0, selector->getSelectedItemIndex())];
                auto existingPoints = recipe.automationLanes.getReference(index).points;
                recipe.automationLanes.getReference(index) = makeLaneForSpec(selectedSpec, recipe.automationCurveMode);
                if (existingPoints.size() >= 2)
                {
                    recipe.automationLanes.getReference(index).points = existingPoints;
                    ensureLane(recipe.automationLanes.getReference(index), recipe.automationCurveMode);
                }
                regenerateSignal();
            }
        };
        automationHost.addAndMakeVisible(selector);

        auto* curveSelector = automationCurveSelectors.add(new juce::ComboBox());
        curveSelector->addItem("Linear", 1);
        curveSelector->addItem("Smooth", 2);
        curveSelector->addItem("Stepped", 3);
        curveSelector->onChange = [this, curveSelector]
        {
            if (suppressCallbacks)
                return;

            auto index = automationCurveSelectors.indexOf(curveSelector);
            if (index >= 0 && index < recipe.automationLanes.size())
            {
                captureUndoCheckpoint("Change lane curve");
                auto& lane = recipe.automationLanes.getReference(index);
                lane.interpolation = curveSelector->getSelectedId() == 3 ? "stepped"
                                   : curveSelector->getSelectedId() == 2 ? "smooth"
                                                                         : "linear";
                for (auto& point : lane.points)
                    point.curve = lane.interpolation;
                regenerateSignal();
            }
        };
        automationHost.addAndMakeVisible(curveSelector);

        auto* removeButton = removeAutomationLaneButtons.add(new juce::TextButton("Remove"));
        removeButton->onClick = [this, removeButton]
        {
            auto index = removeAutomationLaneButtons.indexOf(removeButton);
            if (index >= 0 && index < recipe.automationLanes.size() && recipe.automationLanes.size() > 1)
            {
                captureUndoCheckpoint("Remove motion lane");
                recipe.automationLanes.remove(index);
                rebuildAutomationChrome();
                regenerateSignal();
            }
        };
        automationHost.addAndMakeVisible(removeButton);
    }

    while (automationLaneEditors.size() > recipe.automationLanes.size())
    {
        automationHost.removeChildComponent(automationLaneEditors.getLast());
        automationLaneEditors.removeLast();
        automationHost.removeChildComponent(automationTargetSelectors.getLast());
        automationTargetSelectors.removeLast();
        automationHost.removeChildComponent(automationCurveSelectors.getLast());
        automationCurveSelectors.removeLast();
        automationHost.removeChildComponent(removeAutomationLaneButtons.getLast());
        removeAutomationLaneButtons.removeLast();
    }

    suppressCallbacks = true;
    for (int index = 0; index < recipe.automationLanes.size(); ++index)
    {
        auto& lane = recipe.automationLanes.getReference(index);
        auto spec = getTargetSpec(lane.targetParameter);
        lane.name = spec.title;
        lane.rangeMin = spec.rangeMin;
        lane.rangeMax = spec.rangeMax;
        automationLaneEditors[index]->setLane(lane, spec.accent);

        auto* selector = automationTargetSelectors[index];
        auto* curveSelector = automationCurveSelectors[index];
        for (int itemIndex = 0; itemIndex < (int) getAutomationTargetSpecs().size(); ++itemIndex)
        {
            if (lane.targetParameter == juce::String(getAutomationTargetSpecs()[(size_t) itemIndex].parameterId))
            {
                selector->setSelectedItemIndex(itemIndex, juce::dontSendNotification);
                break;
            }
        }
        curveSelector->setSelectedId(lane.interpolation == "stepped" ? 3 : lane.interpolation == "smooth" ? 2 : 1,
                                     juce::dontSendNotification);
    }
    suppressCallbacks = false;

    resized();
}

void SignalLabPanel::captureUndoCheckpoint(const juce::String& label)
{
    noteInteraction();
    if (undoGestureActive)
        return;

    if (onUndoCheckpointRequested)
        onUndoCheckpointRequested(createState(), label);
}

void SignalLabPanel::beginUndoGesture(const juce::String& label)
{
    noteInteraction();
    if (undoGestureActive)
        return;

    undoGestureActive = true;
    if (onUndoCheckpointRequested)
        onUndoCheckpointRequested(createState(), label);
}

void SignalLabPanel::endUndoGesture()
{
    undoGestureActive = false;
}

void SignalLabPanel::noteInteraction()
{
    if (onInteractionStarted)
        onInteractionStarted();
}
