#include "SignalLabPanel.h"

namespace
{
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
constexpr float kMinFilterCutoffHz = 40.0f;
constexpr float kMaxFilterCutoffHz = 16000.0f;

const std::array<AutomationTargetSpec, 6>& getAutomationTargetSpecs()
{
    static const std::array<AutomationTargetSpec, 6> specs
    {{
        { "pitchOffsetSemitones", "Pitch Motion", -12.0, 12.0, 0.0, juce::Colour(0xffb37df0) },
        { "outputGain", "Gain Motion", 0.0, 1.0, 1.0, juce::Colour(0xff7dd36f) },
        { "filterCutoff", "Filter Motion", 0.0, 1.0, 0.5, juce::Colour(0xffffad5a) },
        { "filterResonance", "Resonance Motion", 0.30, 8.0, 0.90, juce::Colour(0xff5ad1ff) },
        { "noiseLevel", "Noise Motion", 0.0, 1.0, 0.10, juce::Colour(0xffff7aa2) },
        { "baseFrequency", "Base Frequency", 30.0, 2400.0, 180.0, juce::Colour(0xff8ee58f) }
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
            sum += sampleLane(lane.points, lane.interpolation, t, fallbackValue);
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

    if (event.mods.isPopupMenu() && dragIndex > 0 && dragIndex < recipe.envelopePoints.size() - 1)
    {
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

void SignalLabPanel::EnvelopeEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    auto normalizedY = clamp01((plot.getBottom() - event.position.y) / plot.getHeight());

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

    if (event.mods.isPopupMenu() && dragIndex > 0 && dragIndex < lane.points.size() - 1)
    {
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

void SignalLabPanel::AutomationLaneEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        return;

    auto plot = getPlotArea();
    auto normalizedX = clamp01((event.position.x - plot.getX()) / plot.getWidth());
    lane.points.add(makePoint(normalizedX, pointValueFromY(event.position.y), lane.interpolation));
    ensureLane(lane, lane.interpolation);
    if (onLaneChanged)
        onLaneChanged(lane);
    repaint();
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

    configureSlider(frequencySlider, 30.0, 2400.0, 1.0);
    configureSlider(durationSlider, 0.1, 6.0, 0.01);
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

    frequencySlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.baseFrequencyHz = (float) frequencySlider.getValue(); regenerateSignal(); } };
    durationSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.durationSeconds = durationSlider.getValue(); regenerateSignal(); } };
    pitchSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.pitchSweepSemitones = (float) pitchSlider.getValue(); regenerateSignal(); } };
    filterModeSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;

        recipe.filterMode = filterModeSelector.getSelectedId() == 2 ? "bandpass"
                          : filterModeSelector.getSelectedId() == 3 ? "highpass"
                                                                    : "lowpass";
        regenerateSignal();
    };
    envelopeCurveSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;

        recipe.envelopeCurveMode = envelopeCurveSelector.getSelectedId() == 3 ? "stepped"
                                  : envelopeCurveSelector.getSelectedId() == 1 ? "linear"
                                                                               : "smooth";
        ensureEnvelopePoints(recipe.envelopePoints, recipe.envelopeCurveMode);
        for (auto& point : recipe.envelopePoints)
            point.curve = recipe.envelopeCurveMode;
        regenerateSignal();
    };
    automationCurveSelector.onChange = [this]
    {
        if (suppressCallbacks)
            return;

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
    filterCutoffSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.filterCutoffHz = (float) filterCutoffSlider.getValue(); regenerateSignal(); } };
    filterResonanceSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.filterResonance = (float) filterResonanceSlider.getValue(); regenerateSignal(); } };
    filterEnvelopeSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.filterEnvelopeAmount = (float) filterEnvelopeSlider.getValue(); regenerateSignal(); } };
    macroHardnessSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.macroHardness = (float) macroHardnessSlider.getValue(); regenerateSignal(); } };
    macroWeightSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.macroWeight = (float) macroWeightSlider.getValue(); regenerateSignal(); } };
    macroAirSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.macroAir = (float) macroAirSlider.getValue(); regenerateSignal(); } };
    macroGritSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.macroGrit = (float) macroGritSlider.getValue(); regenerateSignal(); } };
    macroSizeSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.macroSize = (float) macroSizeSlider.getValue(); regenerateSignal(); } };
    sineSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.sineLevel = (float) sineSlider.getValue(); regenerateSignal(); } };
    sawSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.sawLevel = (float) sawSlider.getValue(); regenerateSignal(); } };
    squareSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.squareLevel = (float) squareSlider.getValue(); regenerateSignal(); } };
    triangleSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.triangleLevel = (float) triangleSlider.getValue(); regenerateSignal(); } };
    noiseSlider.onValueChange = [this] { if (! suppressCallbacks) { recipe.noiseLevel = (float) noiseSlider.getValue(); regenerateSignal(); } };

    envelopeEditor.onEnvelopeChanged = [this](const juce::Array<cw::PatchAutomationPoint>& points)
    {
        recipe.envelopePoints = points;
        ensureEnvelopePoints(recipe.envelopePoints, recipe.envelopeCurveMode);
        regenerateSignal();
    };

    previewButton.onClick = [this]
    {
        if (onPreviewRequested && generatedBuffer.getNumSamples() > 0)
            onPreviewRequested(generatedBuffer, recipe.sampleRate, recipe.name);
    };
    previewButton.setTooltip("Preview the generated signal");
    addAndMakeVisible(previewButton);

    renderButton.onClick = [this]
    {
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
        auto spec = getAutomationTargetSpecs()[(size_t) (recipe.automationLanes.size() % (int) getAutomationTargetSpecs().size())];
        recipe.automationLanes.add(makeLaneForSpec(spec, recipe.automationCurveMode));
        rebuildAutomationChrome();
        regenerateSignal();
    };
    addAutomationLaneButton.setTooltip("Add another automation lane");
    addAndMakeVisible(addAutomationLaneButton);

    addAndMakeVisible(envelopeEditor);
    addAndMakeVisible(scopePanel);
    addAndMakeVisible(spectrumPanel);

    rebuildAutomationChrome();
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
    recipe.name = state.getProperty("name", recipe.name).toString();
    recipe.sampleRate = (double) state.getProperty("sampleRate", recipe.sampleRate);
    recipe.durationSeconds = (double) state.getProperty("durationSeconds", recipe.durationSeconds);
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
    recipe.name = document.name.isNotEmpty() ? document.name : recipe.name;

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

    recipe.envelopePoints.clear();
    for (const auto& node : document.nodes)
    {
        if (node.kind == "envelope")
        {
            recipe.envelopeCurveMode = node.properties.getWithDefault("curveMode", recipe.envelopeCurveMode).toString();
            auto pointsJson = node.properties.getWithDefault("pointsJson", {}).toString();
            if (pointsJson.isNotEmpty())
                recipe.envelopePoints = parsePointsJson(pointsJson, recipe.envelopeCurveMode);

            if (recipe.envelopePoints.isEmpty())
            {
                setEnvelopeFromLegacy(recipe,
                                      (float) node.properties.getWithDefault("attackPosition", 0.12f),
                                      (float) node.properties.getWithDefault("sustainPosition", 0.42f),
                                      (float) node.properties.getWithDefault("releasePosition", 0.82f),
                                      (float) node.properties.getWithDefault("sustainLevel", 0.48f));
            }
        }
        else if (node.kind == "filter")
        {
            recipe.filterMode = node.properties.getWithDefault("mode", recipe.filterMode).toString();
            recipe.filterCutoffHz = (float) node.properties.getWithDefault("cutoffHz", recipe.filterCutoffHz);
            recipe.filterResonance = (float) node.properties.getWithDefault("resonance", recipe.filterResonance);
            recipe.filterEnvelopeAmount = (float) node.properties.getWithDefault("envelopeAmount", recipe.filterEnvelopeAmount);
        }
    }

    recipe.automationLanes = document.automationLanes;
    for (auto& lane : recipe.automationLanes)
    {
        if (lane.interpolation.isNotEmpty())
            recipe.automationCurveMode = lane.interpolation;
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
    auto addComboRow = [&](juce::Label& label, juce::ComboBox& combo)
    {
        label.setBounds(controlArea.removeFromTop(20));
        combo.setBounds(controlArea.removeFromTop(30));
        controlArea.removeFromTop(14);
    };

    addRow(frequencyLabel, frequencySlider);
    addRow(durationLabel, durationSlider);
    addRow(pitchLabel, pitchSlider);
    addComboRow(filterModeLabel, filterModeSelector);
    addComboRow(envelopeCurveLabel, envelopeCurveSelector);
    addComboRow(automationCurveLabel, automationCurveSelector);
    addRow(filterCutoffLabel, filterCutoffSlider);
    addRow(filterResonanceLabel, filterResonanceSlider);
    addRow(filterEnvelopeLabel, filterEnvelopeSlider);
    addRow(macroHardnessLabel, macroHardnessSlider);
    addRow(macroWeightLabel, macroWeightSlider);
    addRow(macroAirLabel, macroAirSlider);
    addRow(macroGritLabel, macroGritSlider);
    addRow(macroSizeLabel, macroSizeSlider);
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

    auto automationArea = area.removeFromTop(420);
    addAutomationLaneButton.setBounds(automationArea.removeFromTop(28).removeFromLeft(160));
    automationArea.removeFromTop(8);
    for (int index = 0; index < automationLaneEditors.size(); ++index)
    {
        auto row = automationArea.removeFromTop(130);
        auto header = row.removeFromTop(26);
        automationTargetSelectors[index]->setBounds(header.removeFromLeft(220));
        header.removeFromLeft(8);
        removeAutomationLaneButtons[index]->setBounds(header.removeFromLeft(80));
        row.removeFromTop(6);
        automationLaneEditors[index]->setBounds(row);
        automationArea.removeFromTop(10);
    }

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
    ensureRecipe(recipe);
    auto patchDocument = buildPatchDocument(recipe);
    juce::String errorMessage;
    if (! runtimePlayer.renderPatchToBuffer(patchDocument, recipe.durationSeconds, generatedBuffer, errorMessage))
        generatedBuffer = buildSignalBuffer(recipe);

    envelopeEditor.setRecipe(recipe);
    syncAutomationEditors();
    scopePanel.setBuffer(generatedBuffer);
    spectrumPanel.setBuffer(generatedBuffer, recipe.sampleRate);
    updateStatusText();
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
        auto noiseMotion = sampleTargetLanes(recipeCopy.automationLanes, "noiseLevel", t, recipeCopy.noiseLevel);
        auto baseFrequencyMotion = sampleTargetLanes(recipeCopy.automationLanes, "baseFrequency", t, recipeCopy.baseFrequencyHz);
        auto pitchSemitones = recipeCopy.pitchSweepSemitones
                            + (float) pitchMotion
                            + juce::jmap(recipeCopy.macroWeight, 0.0f, 1.0f, 2.0f, -2.0f) * ((float) t - 0.5f) * 2.0f;
        auto baseFrequency = (float) baseFrequencyMotion * juce::jmap(recipeCopy.macroWeight, 0.0f, 1.0f, 1.16f, 0.86f);
        auto frequency = baseFrequency * std::pow(2.0f, pitchSemitones / 12.0f);
        auto phase = juce::MathConstants<float>::twoPi * frequency * ((float) sample / (float) recipeCopy.sampleRate);

        auto sine = std::sin(phase);
        auto saw = 2.0f * (phase / juce::MathConstants<float>::twoPi - std::floor(0.5f + phase / juce::MathConstants<float>::twoPi));
        auto square = std::sin(phase) >= 0.0f ? 1.0f : -1.0f;
        auto triangle = std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
        auto noise = random.nextFloat() * 2.0f - 1.0f;
        auto macroNoise = juce::jlimit(0.0f, 1.0f, (float) noiseMotion + recipeCopy.macroAir * 0.18f + recipeCopy.macroGrit * 0.12f);
        auto gritDrive = 1.0f + recipeCopy.macroGrit * 5.5f;
        auto envelope = (float) sampleLane(envelopePoints, recipeCopy.envelopeCurveMode, t, 0.0);

        auto sampleValue = normalizer * envelope * (float) gainMotion
                         * ((recipeCopy.sineLevel + recipeCopy.macroWeight * 0.10f) * sine
                            + (recipeCopy.sawLevel + recipeCopy.macroGrit * 0.14f) * saw
                            + (recipeCopy.squareLevel + recipeCopy.macroHardness * 0.12f) * square
                            + (recipeCopy.triangleLevel + recipeCopy.macroWeight * 0.08f) * triangle
                            + macroNoise * noise);

        sampleValue = std::tanh(sampleValue * gritDrive) / std::tanh(gritDrive);

        auto filterNormalized = clamp01(cutoffToNormalized(recipeCopy.filterCutoffHz)
                                        + ((float) filterMotion - 0.5f) * 0.75f
                                        + (envelope - 0.5f) * (recipeCopy.filterEnvelopeAmount + recipeCopy.macroHardness * 0.30f)
                                        + recipeCopy.macroAir * 0.18f
                                        - recipeCopy.macroWeight * 0.10f);
        auto brightnessEquivalent = juce::jmap(filterNormalized, 0.0f, 1.0f, 0.02f, 1.0f);
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
    document.description = "Signal Lab exported instrument patch.";
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

    if (recipeCopy.sineLevel > 0.0f)
        document.sources.add({ "osc1", "oscillator", "sine", {}, recipeCopy.sineLevel, "baseFrequency" });
    if (recipeCopy.sawLevel > 0.0f)
        document.sources.add({ "osc2", "oscillator", "saw", {}, recipeCopy.sawLevel, "baseFrequency" });
    if (recipeCopy.squareLevel > 0.0f)
        document.sources.add({ "osc3", "oscillator", "square", {}, recipeCopy.squareLevel, "baseFrequency" });
    if (recipeCopy.triangleLevel > 0.0f)
        document.sources.add({ "osc4", "oscillator", "triangle", {}, recipeCopy.triangleLevel, "baseFrequency" });
    if (recipeCopy.noiseLevel > 0.0f)
        document.sources.add({ "noise1", "noise", {}, "white", recipeCopy.noiseLevel, {} });

    cw::PatchNode mixNode;
    mixNode.id = "mix1";
    mixNode.kind = "mix";
    document.nodes.add(mixNode);

    cw::PatchNode filterNode;
    filterNode.id = "filter1";
    filterNode.kind = "filter";
    filterNode.properties.set("mode", recipeCopy.filterMode);
    filterNode.properties.set("cutoffHz", recipeCopy.filterCutoffHz);
    filterNode.properties.set("resonance", recipeCopy.filterResonance);
    filterNode.properties.set("envelopeAmount", recipeCopy.filterEnvelopeAmount);
    document.nodes.add(filterNode);

    cw::PatchNode envelopeNode;
    envelopeNode.id = "env1";
    envelopeNode.kind = "envelope";
    envelopeNode.properties.set("attackPosition", getLegacyAttackPosition(recipeCopy.envelopePoints));
    envelopeNode.properties.set("sustainPosition", getLegacySustainPosition(recipeCopy.envelopePoints));
    envelopeNode.properties.set("releasePosition", getLegacyReleasePosition(recipeCopy.envelopePoints));
    envelopeNode.properties.set("sustainLevel", getLegacySustainLevel(recipeCopy.envelopePoints));
    envelopeNode.properties.set("curveMode", recipeCopy.envelopeCurveMode);
    envelopeNode.properties.set("pointsJson", serialisePointsJson(recipeCopy.envelopePoints));
    document.nodes.add(envelopeNode);

    for (const auto& source : document.sources)
        document.connections.add({ source.id, "mix1" });
    document.connections.add({ "mix1", "filter1" });
    document.connections.add({ "filter1", "env1" });

    document.output.channelMode = "stereo";
    document.output.gain = 0.9;
    document.output.pan = 0.0;

    return document;
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
    regenerateSignal();
}

void SignalLabPanel::refreshControlsFromRecipe()
{
    ensureRecipe(recipe);
    suppressCallbacks = true;
    nameEditor.setText(recipe.name, juce::dontSendNotification);
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
    auto sampleCount = generatedBuffer.getNumSamples();
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
                recipe.automationLanes.getReference(index) = updatedLane;
                ensureLane(recipe.automationLanes.getReference(index), recipe.automationCurveMode);
                regenerateSignal();
            }
        };
        addAndMakeVisible(editor);

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
        addAndMakeVisible(selector);

        auto* removeButton = removeAutomationLaneButtons.add(new juce::TextButton("Remove"));
        removeButton->onClick = [this, removeButton]
        {
            auto index = removeAutomationLaneButtons.indexOf(removeButton);
            if (index >= 0 && index < recipe.automationLanes.size() && recipe.automationLanes.size() > 1)
            {
                recipe.automationLanes.remove(index);
                rebuildAutomationChrome();
                regenerateSignal();
            }
        };
        addAndMakeVisible(removeButton);
    }

    while (automationLaneEditors.size() > recipe.automationLanes.size())
    {
        removeChildComponent(automationLaneEditors.getLast());
        automationLaneEditors.removeLast();
        removeChildComponent(automationTargetSelectors.getLast());
        automationTargetSelectors.removeLast();
        removeChildComponent(removeAutomationLaneButtons.getLast());
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
        for (int itemIndex = 0; itemIndex < (int) getAutomationTargetSpecs().size(); ++itemIndex)
        {
            if (lane.targetParameter == juce::String(getAutomationTargetSpecs()[(size_t) itemIndex].parameterId))
            {
                selector->setSelectedItemIndex(itemIndex, juce::dontSendNotification);
                break;
            }
        }
    }
    suppressCallbacks = false;

    resized();
}
