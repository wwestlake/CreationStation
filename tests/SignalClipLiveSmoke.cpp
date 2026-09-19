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
cw::PatchDocument makePatch()
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

    return doc;
}

std::unique_ptr<PatchLiveVoice> makeVoice()
{
    auto voice = std::make_unique<PatchLiveVoice>();
    voice->prepareToPlay(kBlockSize, kSampleRate);
    voice->rebuild(makePatch(), PatchLiveBindingMap {});
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

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
