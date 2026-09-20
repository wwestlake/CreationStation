// Automated checks for PatchLiveVoice::renderAt(), the timeline-driven entry point that lets a
// Signal clip run its patch live (one voice per clip) instead of playing a pre-rendered WAV.
//
// Needs no audio hardware: it renders blocks the way the arrangement mixer does and compares the
// results. Exit code 0 = every check passed.

#include <JuceHeader.h>

#include "Audio/PatchLiveVoice.h"
#include "Audio/PatchRuntimePlayer.h"
#include "Patch/PatchModel.h"

#include <cmath>
#include <cstdio>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kPatchSeconds = 1.0;
constexpr int64 kPatchSamples = (int64) (kSampleRate * kPatchSeconds);

int failures = 0;

void check(bool ok, const juce::String& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

// One sine oscillator, no mixer/filter/envelope nodes: rebuild() sums unconnected sources through
// an implicit mixer, so this is the smallest valid patch.
cw::PatchDocument makePatch(bool pitchSweep = false)
{
    cw::PatchDocument doc;
    doc.type = "instrument";
    doc.name = "clip-live-smoke";
    doc.durationSeconds = kPatchSeconds;

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

    if (pitchSweep)
    {
        // Patch-internal motion: the pitch bends across the whole patch, so the oscillator's
        // frequency changes on every sample.
        cw::PatchAutomationLane lane;
        lane.id = "lane_pitch";
        lane.name = "Pitch";
        lane.targetParameter = "pitchOffsetSemitones";
        lane.startTime = 0.0;
        lane.endTime = 1.0;
        lane.rangeMin = -12.0;
        lane.rangeMax = 12.0;
        lane.points.add({ 0.0, 0.5, "linear" });
        lane.points.add({ 1.0, 1.0, "linear" });
        doc.automationLanes.add(lane);
    }

    return doc;
}

std::unique_ptr<PatchLiveVoice> makeVoice(const cw::PatchDocument& patch = makePatch(),
                                          const PatchLiveBindingMap& bindings = PatchLiveBindingMap {})
{
    auto voice = std::make_unique<PatchLiveVoice>();
    voice->prepareToPlay(kBlockSize, kSampleRate);
    voice->rebuild(patch, bindings);
    voice->setPatchDurationSeconds(kPatchSeconds);
    voice->setOutputScale(1.0f); // as the engine does for a timeline clip voice
    voice->adoptPublishedGraphNow();
    return voice;
}

// Renders [from, from + count) of the patch the way the mixer does: block by block, each call
// continuing where the last one ended.
juce::AudioBuffer<float> renderBlocks(PatchLiveVoice& voice, int64 from, int count, int block = kBlockSize)
{
    juce::AudioBuffer<float> out(2, count);
    out.clear();
    for (int done = 0; done < count; done += block)
    {
        const auto n = juce::jmin(block, count - done);
        voice.renderAt(from + done, out, done, n);
    }
    return out;
}

double maxAbsDiff(const juce::AudioBuffer<float>& a, int aStart, const juce::AudioBuffer<float>& b, int bStart, int count)
{
    double worst = 0.0;
    for (int ch = 0; ch < juce::jmin(a.getNumChannels(), b.getNumChannels()); ++ch)
        for (int i = 0; i < count; ++i)
            worst = juce::jmax(worst, (double) std::abs(a.getSample(ch, aStart + i) - b.getSample(ch, bStart + i)));
    return worst;
}

double rms(const juce::AudioBuffer<float>& buffer, int start, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double v = buffer.getSample(0, start + i);
        sum += v * v;
    }
    return std::sqrt(sum / juce::jmax(1, count));
}

// Largest jump between two consecutive samples of channel 0 - a click shows up as one huge step.
double maxSampleStep(const juce::AudioBuffer<float>& buffer, int start, int count)
{
    double worst = 0.0;
    for (int i = 1; i < count; ++i)
        worst = juce::jmax(worst, (double) std::abs(buffer.getSample(0, start + i) - buffer.getSample(0, start + i - 1)));
    return worst;
}

int zeroCrossings(const juce::AudioBuffer<float>& buffer, int start, int count)
{
    int crossings = 0;
    for (int i = 1; i < count; ++i)
        if ((buffer.getSample(0, start + i - 1) < 0.0f) != (buffer.getSample(0, start + i) < 0.0f))
            ++crossings;
    return crossings;
}

// RMS of (a - b) relative to RMS of a, over `count` samples.
double relativeError(const juce::AudioBuffer<float>& a, int aStart, const juce::AudioBuffer<float>& b, int bStart, int count)
{
    double diffSum = 0.0, refSum = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double x = a.getSample(0, aStart + i);
        const double y = b.getSample(0, bStart + i);
        diffSum += (x - y) * (x - y);
        refSum += x * x;
    }
    return std::sqrt(diffSum / juce::jmax(1e-12, refSum));
}
} // namespace

int main()
{
    // Cost of creating a live voice: the timeline makes one per Signal clip, on the message thread,
    // so this must stay cheap. Reported, and bounded generously to catch a regression. One voice is
    // created first because the engine's own Signal Lab preview voice already pays the one-time
    // FRust runtime compile at startup, so later (per-clip) voices never see it.
    {
        makeVoice();
        constexpr int voiceCount = 8;
        std::vector<std::unique_ptr<PatchLiveVoice>> voices;
        double worstMs = 0.0, totalMs = 0.0;
        for (int i = 0; i < voiceCount; ++i)
        {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            voices.push_back(makeVoice());
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            worstMs = juce::jmax(worstMs, ms);
            totalMs += ms;
        }
        std::printf("INFO: created %d voices, total %.1f ms, worst single %.1f ms\n", voiceCount, totalMs, worstMs);
        check(worstMs < 50.0, "creating a live voice is cheap (worst " + juce::String(worstMs, 1) + " ms, limit 50 ms)");
    }

    // Reference: one fresh voice rendered straight through, like transport playing the clip.
    auto referenceVoice = makeVoice();
    const auto reference = renderBlocks(*referenceVoice, 0, (int) kPatchSamples);

    check(rms(reference, 0, (int) kPatchSamples) > 0.01,
          "live voice produces audible output (rms " + juce::String(rms(reference, 0, (int) kPatchSamples), 4) + ")");

    // Two independent voices must agree exactly - the voice has no hidden global/shared state, so
    // two clips of the same patch on a track can't disturb each other.
    {
        auto other = makeVoice();
        const auto again = renderBlocks(*other, 0, (int) kPatchSamples);
        check(maxAbsDiff(reference, 0, again, 0, (int) kPatchSamples) == 0.0,
              "two fresh voices render bit-identical audio");
    }

    // A request larger than the voice's max block size must be chunked internally, giving the same
    // result as the mixer's own block-by-block calls.
    {
        auto big = makeVoice();
        const auto chunked = renderBlocks(*big, 0, (int) kPatchSamples, 4096);
        check(maxAbsDiff(reference, 0, chunked, 0, (int) kPatchSamples) == 0.0,
              "oversized request (4096) is chunked to match block-by-block rendering");
    }

    // Loop: the transport jumps back to the clip start. State is reset on the discontinuity, so the
    // first block after the jump must equal the first block of a brand-new voice.
    {
        auto looping = makeVoice();
        renderBlocks(*looping, 0, (int) kPatchSamples);
        const auto afterLoop = renderBlocks(*looping, 0, kBlockSize);
        check(maxAbsDiff(reference, 0, afterLoop, 0, kBlockSize) == 0.0,
              "after a loop jump back to 0, output equals a fresh voice at 0");
    }

    // Scrub / start mid-clip: seek straight to the middle. Output must line up in phase with the
    // uninterrupted render; only a short filter warm-up transient at the seek point may differ,
    // so compare the settled tail of the block.
    {
        auto seeking = makeVoice();
        constexpr int64 midpoint = kPatchSamples / 2;
        const auto seeked = renderBlocks(*seeking, midpoint, kBlockSize * 4);
        const auto error = relativeError(reference, (int) midpoint + kBlockSize * 2, seeked, kBlockSize * 2, kBlockSize * 2);
        std::printf("INFO: mid-clip seek settled relative error = %.5f\n", error);
        check(error < 0.05, "seeking mid-clip stays phase-aligned with continuous playback (rel err " + juce::String(error, 5) + ")");
    }

    // Live vs offline: the same patch rendered by the live voice (playback) and by the offline
    // renderer (bounce / final render) must sound the same - what you hear is what you render.
    {
        PatchRuntimePlayer offlinePlayer;
        offlinePlayer.prepare(kSampleRate, kBlockSize);
        juce::AudioBuffer<float> offline;
        juce::String offlineError;
        const auto rendered = offlinePlayer.renderPatchToBuffer(makePatch(), kPatchSeconds, offline, offlineError, nullptr);
        check(rendered, "offline renderer accepts the same patch");

        if (rendered)
        {
            const auto count = juce::jmin((int) kPatchSamples, offline.getNumSamples());
            const auto liveRms = rms(reference, 0, count);
            const auto offlineRms = rms(offline, 0, count);
            const auto ratio = offlineRms > 0.0 ? liveRms / offlineRms : 0.0;
            const auto correlationError = relativeError(offline, 0, reference, 0, count);
            std::printf("INFO: live rms %.4f, offline rms %.4f, live/offline ratio %.4f, waveform relative error %.5f\n",
                        liveRms, offlineRms, ratio, correlationError);
            check(std::abs(ratio - 1.0) < 0.05,
                  "live and offline renders have the same loudness (ratio " + juce::String(ratio, 4) + ")");
            check(correlationError < 0.05,
                  "live and offline renders have the same waveform (rel err " + juce::String(correlationError, 5) + ")");
        }
    }

    // A frequency that changes while the note is sounding (automation writing a new value every
    // block) must bend the pitch smoothly. With phase = 2*pi*f*t a frequency jump also jumps the
    // phase, which is a click; the integrated phase is continuous by construction.
    {
        PatchLiveBindingMap bindings;
        bindings.entries.add({ "src_sine", "frequency", "var_freq" });
        bindings.midiNodeValues.add({ "var_freq", 0.5f });

        auto automated = makeVoice(makePatch(), bindings);
        juce::AudioBuffer<float> out(2, (int) kPatchSamples);
        out.clear();
        const int blocks = (int) kPatchSamples / kBlockSize;
        for (int b = 0; b < blocks; ++b)
        {
            // A stepped automation curve: 0.5 for the first third, then a jump up, then a jump down.
            const float value = b < blocks / 3 ? 0.5f : (b < 2 * blocks / 3 ? 0.75f : 0.4f);
            automated->setLiveMidiValue("var_freq", value);
            automated->renderAt((int64) b * kBlockSize, out, b * kBlockSize, kBlockSize);
        }

        const auto step = maxSampleStep(out, 0, blocks * kBlockSize);

        // The reference for "no click": the steepest step of *steady* playback at the highest frequency
        // used above. (The mix stage's saturation steepens the waveform, so a fixed bound derived from
        // the sine alone would be wrong.) A phase jump at a frequency change would add a step well
        // beyond anything steady playback produces.
        auto steadyVoice = makeVoice(makePatch(), bindings);
        juce::AudioBuffer<float> steady(2, blocks * kBlockSize);
        steady.clear();
        for (int b = 0; b < blocks; ++b)
        {
            steadyVoice->setLiveMidiValue("var_freq", 0.75f);
            steadyVoice->renderAt((int64) b * kBlockSize, steady, b * kBlockSize, kBlockSize);
        }
        const auto steadyStep = maxSampleStep(steady, 0, blocks * kBlockSize);
        const auto lowBand = zeroCrossings(out, 0, kBlockSize * (blocks / 3));
        const auto highBand = zeroCrossings(out, kBlockSize * (blocks / 3), kBlockSize * (blocks / 3));
        std::printf("INFO: frequency-step render: max sample step %.4f (steady playback at the highest frequency: %.4f), zero crossings low %d / high %d\n",
                    step, steadyStep, lowBand, highBand);
        check(highBand > lowBand, "an automated frequency change really changes the pitch");
        check(step <= steadyStep * 1.05,
              "automated frequency jumps do not click (max step " + juce::String(step, 4) + " vs steady " + juce::String(steadyStep, 4) + ")");
    }

    // A patch whose own pitch lane bends the note across its length: live and offline renders must
    // still match, because both integrate the phase the same way.
    {
        const auto sweepPatch = makePatch(true);
        auto sweepVoice = makeVoice(sweepPatch);
        const auto liveSweep = renderBlocks(*sweepVoice, 0, (int) kPatchSamples);

        PatchRuntimePlayer sweepPlayer;
        sweepPlayer.prepare(kSampleRate, kBlockSize);
        juce::AudioBuffer<float> offlineSweep;
        juce::String sweepError;
        const auto ok = sweepPlayer.renderPatchToBuffer(sweepPatch, kPatchSeconds, offlineSweep, sweepError, nullptr);
        check(ok, "offline renderer accepts a patch with a pitch lane");
        if (ok)
        {
            const auto count = juce::jmin((int) kPatchSamples, offlineSweep.getNumSamples());
            const auto error = relativeError(offlineSweep, 0, liveSweep, 0, count);
            std::printf("INFO: pitch-lane live vs offline relative error %.6f, max sample step live %.4f\n", error, maxSampleStep(liveSweep, 0, count));
            check(error < 0.01, "live and offline agree on a patch with a pitch lane (rel err " + juce::String(error, 6) + ")");
        }
    }

    // Variables saved in the patch: a public variable wired to a port must survive save/load, and a
    // clip's voice must be drivable by the variable's id alone (this is what timeline automation writes).
    {
        cw::PatchDocument doc = makePatch();

        cw::PatchVariable crunch;
        crunch.id = "var_crunch";
        crunch.name = "FootStepCrunch";
        crunch.description = "how much crunch is in the step";
        crunch.valueType = "Float";
        crunch.isPublic = true;
        crunch.defaultValue = 1.0;
        doc.variables.add(crunch);

        cw::PatchVariable hidden;
        hidden.id = "var_hidden";
        hidden.name = "Internal";
        hidden.valueType = "Int";
        hidden.isPublic = false;
        hidden.defaultValue = 0.25;
        doc.variables.add(hidden);

        doc.variableBindings.add({ "var_crunch", "src_sine", "level" });

        const auto json = cw::serialisePatchDocumentJson(doc);
        cw::PatchDocument parsed;
        juce::String parseError;
        const auto parsedOk = cw::parsePatchDocumentJson(json, parsed, parseError);
        check(parsedOk, "a patch with variables saves and loads");
        check(parsed.variables.size() == 2
                  && parsed.variables[0].id == "var_crunch" && parsed.variables[0].name == "FootStepCrunch"
                  && parsed.variables[0].valueType == "Float" && parsed.variables[0].isPublic
                  && std::abs(parsed.variables[0].defaultValue - 1.0) < 1e-9
                  && parsed.variables[1].valueType == "Int" && ! parsed.variables[1].isPublic
                  && std::abs(parsed.variables[1].defaultValue - 0.25) < 1e-9,
              "variables (id, name, type, public/private, default) round-trip");
        check(parsed.variableBindings.size() == 1 && parsed.variableBindings[0].variableId == "var_crunch"
                  && parsed.variableBindings[0].targetNodeId == "src_sine" && parsed.variableBindings[0].targetPort == "level",
              "variable-to-port bindings round-trip");

        // A patch saved before variables existed has neither section and must still load.
        auto rootVar = juce::JSON::parse(json);
        rootVar.getDynamicObject()->removeProperty("variables");
        rootVar.getDynamicObject()->removeProperty("variableBindings");
        cw::PatchDocument legacy;
        const auto legacyOk = cw::parsePatchDocumentJson(juce::JSON::toString(rootVar), legacy, parseError);
        check(legacyOk && legacy.variables.isEmpty() && legacy.variableBindings.isEmpty(),
              "a patch saved before variables existed still loads (exposes nothing)");

        // A voice built from the saved patch alone, driven by variable id.
        auto rmsWithVariable = [&](float value)
        {
            auto voice = makeVoice(parsed, makeVariableBindingMap(parsed));
            voice->setVariableValue("var_crunch", value);
            const auto out = renderBlocks(*voice, 0, (int) kPatchSamples);
            return rms(out, 0, (int) kPatchSamples);
        };
        const auto atZero = rmsWithVariable(0.0f);
        const auto atQuarter = rmsWithVariable(0.25f);
        const auto atHalf = rmsWithVariable(0.5f);
        const auto atFull = rmsWithVariable(1.0f);
        std::printf("INFO: variable -> level: rms at 0 / 0.25 / 0.5 / 1.0 = %.4f / %.4f / %.4f / %.4f\n", atZero, atQuarter, atHalf, atFull);
        // At 0 the oscillator's contribution is gone; what remains is the mix stage's own noise floor, which
        // does not depend on any oscillator level.
        check(atZero < atFull * 0.6, "variable at 0 removes the oscillator it drives (floor " + juce::String(atZero, 4)
                                         + " vs full " + juce::String(atFull, 4) + ")");
        check(atQuarter < atHalf && atHalf < atFull, "raising the variable raises the sound (monotonic)");

        // Untouched, the slot holds the variable's saved default.
        auto defaultVoice = makeVoice(parsed, makeVariableBindingMap(parsed));
        const auto defaultRender = renderBlocks(*defaultVoice, 0, (int) kPatchSamples);
        check(std::abs(rms(defaultRender, 0, (int) kPatchSamples) - atFull) < 1e-6,
              "an unwritten variable holds its saved default (1.0)");

        // A variable that is not wired to anything changes nothing.
        auto unwiredVoice = makeVoice(parsed, makeVariableBindingMap(parsed));
        unwiredVoice->setVariableValue("var_hidden", 0.9f);
        const auto unwiredRender = renderBlocks(*unwiredVoice, 0, (int) kPatchSamples);
        check(maxAbsDiff(defaultRender, 0, unwiredRender, 0, (int) kPatchSamples) == 0.0,
              "writing a variable that drives no port has no effect");
    }

    // Additive: the voice adds into the track buffer (it must not clobber other clips on the track).
    {
        auto adding = makeVoice();
        juce::AudioBuffer<float> track(2, kBlockSize);
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(track.getWritePointer(ch), 0.25f, kBlockSize);
        adding->renderAt(0, track, 0, kBlockSize);

        double worst = 0.0;
        for (int i = 0; i < kBlockSize; ++i)
            worst = juce::jmax(worst, (double) std::abs(track.getSample(0, i) - (0.25f + reference.getSample(0, i))));
        check(worst < 1e-6, "renders additively into a buffer that already has audio in it");
    }

    // The patch ends at its designed length: nothing is written past it, and a request straddling
    // the end writes only the part inside.
    {
        auto ending = makeVoice();
        juce::AudioBuffer<float> past(2, kBlockSize);
        past.clear();
        ending->renderAt(kPatchSamples + 100, past, 0, kBlockSize);
        check(rms(past, 0, kBlockSize) == 0.0, "nothing is rendered past the end of the patch");

        auto straddle = makeVoice();
        juce::AudioBuffer<float> tail(2, kBlockSize);
        tail.clear();
        straddle->renderAt(kPatchSamples - 100, tail, 0, kBlockSize);
        check(rms(tail, 0, 100) >= 0.0 && rms(tail, 100, kBlockSize - 100) == 0.0,
              "a block straddling the end writes only the samples inside the patch");
    }

    // Signal Lab "Play": a sine wired straight to a Sink node (no Mix), played the way the live engine plays it -
    // start(), then the mixer pulling blocks - with the preview voice's own default output scale.
    {
        auto patch = makePatch();
        cw::PatchNode sink;
        sink.id = "sink1";
        sink.kind = "output";
        patch.nodes.add(sink);

        cw::PatchConnection wire;
        wire.from = "src_sine";
        wire.to = "sink1";
        wire.fromPort = "signalOut";
        wire.toPort = "signalIn";
        wire.weight = 1.0;
        patch.connections.add(wire);

        auto voice = std::make_unique<PatchLiveVoice>();
        voice->prepareToPlay(kBlockSize, kSampleRate);
        voice->rebuild(patch, PatchLiveBindingMap {});
        voice->start(kPatchSeconds);

        juce::AudioBuffer<float> out(2, kBlockSize * 8);
        out.clear();
        for (int done = 0; done < out.getNumSamples(); done += kBlockSize)
        {
            juce::AudioSourceChannelInfo info(&out, done, kBlockSize);
            voice->getNextAudioBlock(info);
        }
        std::printf("INFO: sine -> sink via start()/getNextAudioBlock: rms %.4f\n", rms(out, 0, out.getNumSamples()));
        check(rms(out, 0, out.getNumSamples()) > 0.0, "a sine wired to a Sink is not silent when played live (Signal Lab Play path)");
    }

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
