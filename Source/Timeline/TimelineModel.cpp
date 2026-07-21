#include "TimelineModel.h"

namespace cs
{
void TimelineModel::setTempo(double bpm, int numerator, int denominator)
{
    tempoBpm = juce::jlimit(20.0, 320.0, bpm);
    timeSignatureNumerator = juce::jlimit(1, 16, numerator);
    timeSignatureDenominator = juce::jlimit(1, 32, denominator);
}

void TimelineModel::setPixelsPerSecond(double newPixelsPerSecond)
{
    pixelsPerSecond = juce::jlimit(30.0, 900.0, newPixelsPerSecond);
}

void TimelineModel::zoomIn()
{
    setPixelsPerSecond(pixelsPerSecond * 1.35);
}

void TimelineModel::zoomOut()
{
    setPixelsPerSecond(pixelsPerSecond / 1.35);
}

void TimelineModel::setTransportSeconds(double seconds)
{
    transportSeconds = juce::jmax(0.0, seconds);
}

double TimelineModel::getNextBoundarySeconds(double fromSeconds) const noexcept
{
    auto next = std::numeric_limits<double>::max();
    auto threshold = fromSeconds + 0.01;

    for (const auto& clip : clips)
    {
        if (clip.startSeconds > threshold)
            next = juce::jmin(next, clip.startSeconds);

        auto endSeconds = clip.startSeconds + clip.durationSeconds;
        if (endSeconds > threshold)
            next = juce::jmin(next, endSeconds);
    }

    if (next != std::numeric_limits<double>::max())
        return next;

    auto measureSeconds = beatToSeconds(timeSignatureNumerator);
    return fromSeconds + juce::jmax(0.25, measureSeconds);
}

double TimelineModel::getPreviousBoundarySeconds(double fromSeconds) const noexcept
{
    auto previous = 0.0;
    auto threshold = fromSeconds - 0.01;

    for (const auto& clip : clips)
    {
        if (clip.startSeconds < threshold)
            previous = juce::jmax(previous, clip.startSeconds);

        auto endSeconds = clip.startSeconds + clip.durationSeconds;
        if (endSeconds < threshold)
            previous = juce::jmax(previous, endSeconds);
    }

    return previous;
}

double TimelineModel::beatToSeconds(double beat) const noexcept
{
    return beat * 60.0 / tempoBpm;
}

double TimelineModel::secondsToBeat(double seconds) const noexcept
{
    return seconds * tempoBpm / 60.0;
}

int TimelineModel::beginRecordingClip(int trackIndex, const juce::File& file)
{
    TimelineClip clip;
    clip.id = juce::Uuid().toString();
    clip.trackIndex = trackIndex;
    clip.file = file;
    clip.startSeconds = transportSeconds;
    clip.durationSeconds = 0.0;
    clip.recording = true;

    clips.push_back(std::move(clip));
    auto clipIndex = static_cast<int>(clips.size()) - 1;
    activeRecordingClips.push_back(clipIndex);

    if (activeRecordingClips.size() == 1)
        recordingStartSeconds = transportSeconds;

    return clipIndex;
}

void TimelineModel::updateRecordingClip(double nowSeconds)
{
    setTransportSeconds(nowSeconds);

    for (auto clipIndex : activeRecordingClips)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        auto& clip = clips[(size_t) clipIndex];
        clip.durationSeconds = juce::jmax(0.05, transportSeconds - clip.startSeconds);
    }
}

void TimelineModel::addRecordingPeak(int trackIndex, float peak)
{
    auto clampedPeak = juce::jlimit(0.0f, 1.0f, peak);

    for (auto clipIndex : activeRecordingClips)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        auto& clip = clips[(size_t) clipIndex];
        if (clip.trackIndex != trackIndex || ! clip.recording)
            continue;

        clip.peaks.push_back(clampedPeak);

        constexpr auto maxLivePeaks = 4096;
        if (clip.peaks.size() > maxLivePeaks)
            clip.peaks.erase(clip.peaks.begin(), clip.peaks.begin() + static_cast<std::ptrdiff_t>(clip.peaks.size() - maxLivePeaks));
    }
}

void TimelineModel::finishRecordingClip(double nowSeconds)
{
    updateRecordingClip(nowSeconds);

    auto clipsToAnalyze = activeRecordingClips;
    activeRecordingClips.clear();

    for (auto clipIndex : clipsToAnalyze)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        clips[(size_t) clipIndex].recording = false;

        juce::String errorMessage;
        analyzeClipWaveform(clipIndex, errorMessage);
    }
}

void TimelineModel::cancelRecordingClip()
{
    std::sort(activeRecordingClips.begin(), activeRecordingClips.end(), std::greater<int>());

    for (auto clipIndex : activeRecordingClips)
        if (juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            clips.erase(clips.begin() + clipIndex);

    activeRecordingClips.clear();
}

int TimelineModel::addAudioClip(int trackIndex, const juce::File& file, double startSeconds, juce::String& errorMessage)
{
    if (trackIndex < 0)
    {
        errorMessage = "Choose a valid track before placing the sound.";
        return -1;
    }

    if (! file.existsAsFile())
    {
        errorMessage = "The rendered sound file was not found.";
        return -1;
    }

    TimelineClip clip;
    clip.id = juce::Uuid().toString();
    clip.trackIndex = trackIndex;
    clip.file = file;
    clip.startSeconds = juce::jmax(0.0, startSeconds);
    clip.durationSeconds = 0.05;
    clip.recording = false;

    clips.push_back(std::move(clip));
    const auto clipIndex = static_cast<int>(clips.size()) - 1;

    if (! analyzeClipWaveform(clipIndex, errorMessage))
    {
        clips.erase(clips.begin() + clipIndex);
        return -1;
    }

    return clipIndex;
}

bool TimelineModel::moveClip(int clipIndex, int trackIndex, double startSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())) || trackIndex < 0)
        return false;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.recording)
        return false;

    clip.trackIndex = trackIndex;
    clip.startSeconds = juce::jmax(0.0, startSeconds);
    return true;
}

void TimelineModel::clear()
{
    clips.clear();
    activeRecordingClips.clear();
    transportSeconds = 0.0;
}

double TimelineModel::getTotalDurationSeconds() const noexcept
{
    auto total = 8.0;
    for (const auto& clip : clips)
        total = juce::jmax(total, clip.startSeconds + clip.durationSeconds + 2.0);

    return total;
}

void TimelineModel::removeTrack(int trackIndex)
{
    if (trackIndex < 0)
        return;

    clips.erase(std::remove_if(clips.begin(),
                               clips.end(),
                               [trackIndex](const TimelineClip& clip)
                               {
                                   return clip.trackIndex == trackIndex;
                               }),
                clips.end());

    for (auto& clip : clips)
        if (clip.trackIndex > trackIndex)
            --clip.trackIndex;

    activeRecordingClips.clear();
}

bool TimelineModel::analyzeClipWaveform(int clipIndex, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    clip.peaks.clear();

    if (! clip.file.existsAsFile())
    {
        errorMessage = "Recorded audio file does not exist yet.";
        return false;
    }

    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(clip.file));
    if (reader == nullptr)
    {
        errorMessage = "Could not read recorded audio for waveform display.";
        return false;
    }

    clip.durationSeconds = reader->sampleRate > 0.0 ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate
                                                    : clip.durationSeconds;

    auto peakCount = juce::jlimit(32, 2048, static_cast<int>(clip.durationSeconds * 120.0));
    clip.peaks.resize((size_t) peakCount, 0.0f);

    juce::AudioBuffer<float> scratch((int) reader->numChannels, 4096);
    for (int peakIndex = 0; peakIndex < peakCount; ++peakIndex)
    {
        auto startSample = static_cast<juce::int64>((double) peakIndex * (double) reader->lengthInSamples / (double) peakCount);
        auto endSample = static_cast<juce::int64>((double) (peakIndex + 1) * (double) reader->lengthInSamples / (double) peakCount);
        auto samplesRemaining = static_cast<int>(juce::jmax<juce::int64>(1, endSample - startSample));
        auto readPosition = startSample;
        auto peak = 0.0f;

        while (samplesRemaining > 0)
        {
            auto samplesThisRead = juce::jmin(samplesRemaining, scratch.getNumSamples());
            scratch.clear();
            reader->read(&scratch, 0, samplesThisRead, readPosition, true, true);

            for (int channel = 0; channel < scratch.getNumChannels(); ++channel)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax(scratch.getReadPointer(channel), samplesThisRead);
                peak = juce::jmax(peak, std::abs(range.getStart()), std::abs(range.getEnd()));
            }

            readPosition += samplesThisRead;
            samplesRemaining -= samplesThisRead;
        }

        clip.peaks[(size_t) peakIndex] = juce::jlimit(0.0f, 1.0f, peak);
    }

    return true;
}

juce::ValueTree TimelineModel::createState() const
{
    juce::ValueTree state("Timeline");
    state.setProperty("tempoBpm", tempoBpm, nullptr);
    state.setProperty("timeSignatureNumerator", timeSignatureNumerator, nullptr);
    state.setProperty("timeSignatureDenominator", timeSignatureDenominator, nullptr);
    state.setProperty("pixelsPerSecond", pixelsPerSecond, nullptr);
    state.setProperty("transportSeconds", transportSeconds, nullptr);

    juce::ValueTree clipsState("Clips");
    for (const auto& clip : clips)
    {
        juce::ValueTree clipState("Clip");
        clipState.setProperty("id", clip.id, nullptr);
        clipState.setProperty("trackIndex", clip.trackIndex, nullptr);
        clipState.setProperty("file", clip.file.getFullPathName(), nullptr);
        clipState.setProperty("startSeconds", clip.startSeconds, nullptr);
        clipState.setProperty("durationSeconds", clip.durationSeconds, nullptr);
        clipState.setProperty("recording", false, nullptr);
        clipsState.addChild(clipState, -1, nullptr);
    }

    state.addChild(clipsState, -1, nullptr);
    return state;
}

void TimelineModel::restoreState(const juce::ValueTree& state)
{
    clear();

    if (! state.isValid() || state.getType() != juce::Identifier("Timeline"))
        return;

    setTempo((double) state.getProperty("tempoBpm", tempoBpm),
             (int) state.getProperty("timeSignatureNumerator", timeSignatureNumerator),
             (int) state.getProperty("timeSignatureDenominator", timeSignatureDenominator));
    setPixelsPerSecond((double) state.getProperty("pixelsPerSecond", pixelsPerSecond));
    setTransportSeconds((double) state.getProperty("transportSeconds", 0.0));

    auto clipsState = state.getChildWithName("Clips");
    if (! clipsState.isValid())
        return;

    for (const auto child : clipsState)
    {
        if (! child.hasType("Clip"))
            continue;

        TimelineClip clip;
        clip.id = child.getProperty("id").toString();
        if (clip.id.isEmpty())
            clip.id = juce::Uuid().toString();

        clip.trackIndex = (int) child.getProperty("trackIndex", -1);
        clip.file = juce::File(child.getProperty("file").toString());
        clip.startSeconds = (double) child.getProperty("startSeconds", 0.0);
        clip.durationSeconds = (double) child.getProperty("durationSeconds", 0.0);
        clip.recording = false;
        clips.push_back(std::move(clip));

        juce::String errorMessage;
        analyzeClipWaveform(static_cast<int>(clips.size()) - 1, errorMessage);
    }
}
}
