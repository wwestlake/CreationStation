// Automated checks for the offline render of Signal clips through the real WorkstationAudioEngine.
//
// The offline render runs the same arrangement/mixer path playback uses, block by block. These checks
// guard the properties a final render must have: a Signal clip is rendered from its live voice exactly
// once, stale live voices never leak into a render, and timeline automation lanes are applied.
// Needs no audio hardware. Exit code 0 = every check passed.

#include <JuceHeader.h>

#include "Audio/WorkstationAudioEngine.h"
#include "Patch/PatchModel.h"

#include <cmath>
#include <cstdio>

// TimelineModel's session save/restore uses these two conversions, which live in Station's project
// storage code. A render never saves or restores a session, so minimal stand-ins keep this test from
// linking the whole project-storage layer.
namespace cs
{
juce::String toStorageToken(creation::assets::AssetReferenceMode) { return "exact"; }
creation::assets::AssetReferenceMode assetReferenceModeFromStorageToken(const juce::String&)
{
    return creation::assets::AssetReferenceMode::exact;
}
} // namespace cs

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kSeconds = 1.0;

int failures = 0;

void check(bool ok, const juce::String& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

// One sine oscillator whose level is driven by the public variable "var_level" (default 1.0).
cw::PatchDocument makeVariablePatch()
{
    cw::PatchDocument doc;
    doc.type = "instrument";
    doc.name = "render-smoke";
    doc.durationSeconds = kSeconds;

    cw::PatchParameter base;
    base.id = "baseFrequency";
    base.name = "Base Frequency";
    base.kind = "number";
    base.defaultValue = 440.0;
    doc.parameters.add(base);

    cw::PatchSource sine;
    sine.id = "src_sine";
    sine.kind = "oscillator";
    sine.waveform = "sine";
    sine.level = 0.6;
    sine.frequencyParameter = "baseFrequency";
    doc.sources.add(sine);

    cw::PatchVariable level;
    level.id = "var_level";
    level.name = "Level";
    level.valueType = "Float";
    level.isPublic = true;
    level.defaultValue = 1.0;
    doc.variables.add(level);
    doc.variableBindings.add({ "var_level", "src_sine", "level" });
    return doc;
}

WorkstationAudioEngine::SignalClipTarget makeClipTarget()
{
    WorkstationAudioEngine::SignalClipTarget target;
    target.clipId = "clip_a";
    target.patchKey = "key_a";
    target.trackIndex = 0;
    target.startSeconds = 0.0;
    target.sourceStartSeconds = 0.0;
    target.durationSeconds = kSeconds;
    target.patch = makeVariablePatch();
    return target;
}

juce::AudioBuffer<float> render(WorkstationAudioEngine& engine, bool withLane, bool prepublishLiveVoices)
{
    // A lane on track 1 ramps the clip's "var_level" from 0 up to 1 across the clip.
    juce::Array<WorkstationAudioEngine::SignalClipTarget> signalTargets;
    signalTargets.add(makeClipTarget());

    juce::String error;
    if (prepublishLiveVoices)
        engine.setTrackerSignalClips(signalTargets, error); // what playback would have published earlier

    cs::AutomationTarget target;
    if (withLane)
    {
        target.kind = cs::AutomationTargetKind::signalClipInput;
        target.targetTrackIndex = 0;
        target.targetClipId = "clip_a";
        target.parameterId = "var_level";
        target.displayName = "Signal -> Level";
    }
    std::vector<cs::AutomationPoint> points;
    if (withLane)
    {
        cs::AutomationPoint start;
        start.seconds = 0.0;
        start.value = 0.0f;
        cs::AutomationPoint end;
        end.seconds = kSeconds;
        end.value = 1.0f;
        points.push_back(start);
        points.push_back(end);
    }
    engine.setTrackAutomationData(1, target, points);

    WorkstationAudioEngine::RenderSettings settings;
    settings.sampleRate = kSampleRate;
    settings.blockSize = 512;

    juce::AudioBuffer<float> out;
    if (! engine.renderTrackerMixToBuffer({}, kSeconds, settings, out, error, signalTargets))
        std::printf("render failed: %s\n", error.toRawUTF8());
    return out;
}

double rmsWindow(const juce::AudioBuffer<float>& buffer, double fromFraction, double toFraction)
{
    const auto n = buffer.getNumSamples();
    const auto a = (int) (fromFraction * n), b = (int) (toFraction * n);
    double sum = 0.0;
    for (int i = a; i < b; ++i)
    {
        const double v = buffer.getSample(0, i);
        sum += v * v;
    }
    return std::sqrt(sum / juce::jmax(1, b - a));
}

double maxAbsDiff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double worst = 0.0;
    const auto n = juce::jmin(a.getNumSamples(), b.getNumSamples());
    for (int ch = 0; ch < juce::jmin(a.getNumChannels(), b.getNumChannels()); ++ch)
        for (int i = 0; i < n; ++i)
            worst = juce::jmax(worst, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
    return worst;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    WorkstationAudioEngine engine;
    engine.addTrack("Signal");     // track 0: the Signal clip lives here
    engine.addTrack("Automation"); // track 1: an automation lane targeting track 0's clip
    engine.setTrackIsAutomationKind(1, true);

    const auto plain = render(engine, false, false);
    check(plain.getNumSamples() > 0 && plain.getMagnitude(0, plain.getNumSamples()) > 0.01f,
          "a project with only a Signal clip renders audible audio");

    // The live voices playback published earlier must not leak into (or double up in) a render.
    const auto withStaleLive = render(engine, false, true);
    check(maxAbsDiff(plain, withStaleLive) == 0.0,
          "a render is identical whether or not live voices were published first (no doubling, no leak)");

    const auto again = render(engine, false, false);
    check(maxAbsDiff(plain, again) == 0.0, "rendering twice gives identical audio");

    // Automation lanes apply in the render: level ramps 0 -> 1, so the early part is much quieter than
    // the same part of the un-automated render.
    const auto automated = render(engine, true, false);
    const auto plainEarly = rmsWindow(plain, 0.15, 0.30);
    const auto autoEarly = rmsWindow(automated, 0.15, 0.30);
    std::printf("INFO: early window rms: plain %.4f, automated %.4f\n", plainEarly, autoEarly);
    check(autoEarly < plainEarly * 0.8, "an automation lane changes the rendered Signal clip (early window is quieter)");

    const auto automatedAgain = render(engine, true, false);
    check(maxAbsDiff(automated, automatedAgain) == 0.0, "an automated render is repeatable");

    // The engine must be left ready for playback: the live set is back, and the render did not disturb it.
    const auto afterAll = render(engine, false, false);
    check(maxAbsDiff(plain, afterAll) == 0.0, "renders after an automated render are unaffected by it");

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
