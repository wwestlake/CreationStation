#include "SamplePackBuilderEngine.h"
#include "PitchDetector.h"
#include <cmath>

juce::AudioBuffer<float> SamplePackBuilderEngine::resampleBySemitoneCents(const juce::AudioBuffer<float>& source,
                                                                          double appliedCents)
{
    auto speedRatio = std::pow(2.0, appliedCents / 1200.0);
    auto inputLength = source.getNumSamples();
    auto outputLength = juce::jmax(1, (int) std::round((double) inputLength / speedRatio));

    // A few guard samples of silence at the end so the interpolator never reads past the buffer.
    juce::AudioBuffer<float> padded(1, inputLength + 8);
    padded.clear();
    padded.copyFrom(0, 0, source, 0, 0, inputLength);

    juce::AudioBuffer<float> output(1, outputLength);
    juce::LagrangeInterpolator interpolator;
    interpolator.reset();
    interpolator.process(speedRatio, padded.getReadPointer(0), output.getWritePointer(0), outputLength);

    return output;
}

void SamplePackBuilderEngine::trimLeadingSilence(juce::AudioBuffer<float>& buffer)
{
    constexpr float silenceThreshold = 0.02f;
    constexpr int preRollSamples = 128; // keep a little runway before the onset so the transient isn't clipped

    auto numSamples = buffer.getNumSamples();
    auto* data = buffer.getReadPointer(0);

    auto firstLoud = 0;
    for (; firstLoud < numSamples; ++firstLoud)
        if (std::abs(data[firstLoud]) > silenceThreshold)
            break;

    firstLoud = juce::jmax(0, firstLoud - preRollSamples);
    if (firstLoud <= 0)
        return;

    auto remaining = juce::jmax(1, numSamples - firstLoud);
    juce::AudioBuffer<float> trimmed(buffer.getNumChannels(), remaining);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        trimmed.copyFrom(channel, 0, buffer, channel, firstLoud, remaining);

    buffer = std::move(trimmed);
}

void SamplePackBuilderEngine::normalizePeak(juce::AudioBuffer<float>& buffer, float targetPeak)
{
    auto peak = buffer.getMagnitude(0, buffer.getNumSamples());
    if (peak > 0.0f)
        buffer.applyGain(targetPeak / peak);
}

bool SamplePackBuilderEngine::readAndAnalyzeFile(const juce::File& file,
                                                 juce::AudioFormatManager& formatManager,
                                                 AnalyzedCapture& outCapture,
                                                 juce::String& outError)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        outError = "could not open as audio";
        return false;
    }

    auto numSamples = (int) reader->lengthInSamples;
    if (numSamples <= 0)
    {
        outError = "file is empty";
        return false;
    }

    juce::AudioBuffer<float> raw((int) reader->numChannels, numSamples);
    reader->read(&raw, 0, numSamples, 0, true, true);

    // Sample packs are mono, single-note captures - sum any stereo source down for analysis and
    // for the exported file.
    juce::AudioBuffer<float> mono(1, numSamples);
    mono.clear();
    for (int channel = 0; channel < raw.getNumChannels(); ++channel)
        mono.addFrom(0, 0, raw, channel, 0, numSamples, 1.0f / (float) raw.getNumChannels());

    auto sampleRate = reader->sampleRate;

    auto pitch = PitchDetector::detectPitch(mono.getReadPointer(0), numSamples, sampleRate);
    if (! pitch.detected)
    {
        outError = "no confident pitch detected";
        return false;
    }

    auto midiNote = juce::jlimit(0, 127, (int) std::round(69.0 + 12.0 * std::log2(pitch.frequencyHz / 440.0)));
    auto targetFrequency = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    auto centsOffset = 1200.0 * std::log2(pitch.frequencyHz / targetFrequency);

    // Peak-to-noise-floor ratio, measured before any trimming/correction - a proxy for capture
    // cleanliness, used later to break ties when two takes land on the same note.
    auto peak = mono.getMagnitude(0, numSamples);
    auto noiseFloorSamples = juce::jmin(numSamples, (int) (0.01 * sampleRate));
    auto noiseFloorRms = noiseFloorSamples > 0 ? mono.getRMSLevel(0, 0, noiseFloorSamples) : 0.0f;
    auto peakToNoise = peak / juce::jmax(1.0e-6f, noiseFloorRms);

    // Correct tuning drift: shift by the negative of the measured offset so the corrected file
    // lands exactly on the target note's standard-tuning frequency.
    auto corrected = resampleBySemitoneCents(mono, -centsOffset);
    trimLeadingSilence(corrected);
    normalizePeak(corrected);

    outCapture.sourceFileName = file.getFileName();
    outCapture.midiNote = midiNote;
    outCapture.detectedFrequencyHz = pitch.frequencyHz;
    outCapture.centsOffset = centsOffset;
    outCapture.confidence = pitch.confidence;
    outCapture.peakToNoiseRatio = peakToNoise;
    outCapture.correctedBuffer = std::move(corrected);
    outCapture.sampleRate = sampleRate;
    return true;
}

bool SamplePackBuilderEngine::writeNoteFile(const juce::File& outputFolder, int midiNote,
                                            const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    auto file = outputFolder.getChildFile("Note_" + juce::String(midiNote).paddedLeft('0', 3) + ".wav");
    file.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());
    if (outputStream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(), sampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0));
    if (writer == nullptr)
        return false;

    outputStream.release(); // writer now owns the stream
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    return true;
}

SamplePackBuilderEngine::BuildResult SamplePackBuilderEngine::build(const juce::File& inputFolder,
                                                                    const juce::File& outputFolder,
                                                                    std::function<void(const juce::String&)> progressCallback)
{
    BuildResult result;

    if (! inputFolder.isDirectory())
    {
        result.errorMessage = "Input folder does not exist.";
        return result;
    }

    if (! outputFolder.exists() && ! outputFolder.createDirectory())
    {
        result.errorMessage = "Could not create the output folder.";
        return result;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::Array<juce::File> inputFiles;
    inputFolder.findChildFiles(inputFiles, juce::File::findFiles, false);

    std::vector<AnalyzedCapture> captures;

    for (const auto& file : inputFiles)
    {
        FileReport report;
        report.fileName = file.getFileName();

        AnalyzedCapture capture;
        juce::String error;

        if (readAndAnalyzeFile(file, formatManager, capture, error))
        {
            report.analyzed = true;
            report.midiNote = capture.midiNote;
            report.detectedFrequencyHz = capture.detectedFrequencyHz;
            report.centsOffset = capture.centsOffset;
            report.confidence = capture.confidence;

            if (progressCallback)
                progressCallback(report.fileName + " -> note " + juce::String(report.midiNote)
                                  + " (" + juce::String(report.centsOffset, 1) + " cents)");

            captures.push_back(std::move(capture));
        }
        else
        {
            report.analyzed = false;
            report.skipReason = error;

            if (progressCallback)
                progressCallback(report.fileName + " -> skipped (" + error + ")");
        }

        result.fileReports.push_back(std::move(report));
    }

    if (captures.empty())
    {
        result.errorMessage = "No usable pitched captures were found in that folder.";
        return result;
    }

    // Resolve near-duplicate notes down to the single cleanest capture per note: highest
    // pitch-detection confidence wins, breaking ties by peak-to-noise-floor ratio.
    std::array<int, 128> winnerIndex;
    winnerIndex.fill(-1);

    for (int i = 0; i < (int) captures.size(); ++i)
    {
        auto note = captures[(size_t) i].midiNote;
        auto currentWinner = winnerIndex[(size_t) note];

        if (currentWinner < 0)
        {
            winnerIndex[(size_t) note] = i;
            continue;
        }

        const auto& challenger = captures[(size_t) i];
        const auto& incumbent = captures[(size_t) currentWinner];

        auto challengerIsBetter = challenger.confidence > incumbent.confidence
                                    || (challenger.confidence == incumbent.confidence
                                        && challenger.peakToNoiseRatio > incumbent.peakToNoiseRatio);

        if (challengerIsBetter)
            winnerIndex[(size_t) note] = i;
    }

    for (auto& report : result.fileReports)
    {
        if (! report.analyzed)
            continue;

        auto winner = winnerIndex[(size_t) report.midiNote];
        report.keptAsCleanest = winner >= 0 && captures[(size_t) winner].sourceFileName == report.fileName;
    }

    for (int note = 0; note < 128; ++note)
        result.notesCaptured[(size_t) note] = winnerIndex[(size_t) note] >= 0;

    // Gap fill: for every uncaptured note, find the nearest ACTUALLY-captured note (never chain
    // from another gap-filled note, to avoid compounding pitch-shift artifacts across hops).
    std::array<int, 128> nearestCapturedNote;
    for (int note = 0; note < 128; ++note)
    {
        if (winnerIndex[(size_t) note] >= 0)
        {
            nearestCapturedNote[(size_t) note] = note;
            continue;
        }

        auto bestDistance = 1000;
        auto bestNote = -1;

        for (int otherNote = 0; otherNote < 128; ++otherNote)
        {
            if (winnerIndex[(size_t) otherNote] < 0)
                continue;

            auto distance = std::abs(otherNote - note);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestNote = otherNote;
            }
        }

        nearestCapturedNote[(size_t) note] = bestNote;
    }

    for (int note = 0; note < 128; ++note)
    {
        auto sourceNote = nearestCapturedNote[(size_t) note];
        if (sourceNote < 0)
            continue;

        const auto& sourceCapture = captures[(size_t) winnerIndex[(size_t) sourceNote]];

        if (sourceNote == note)
        {
            if (writeNoteFile(outputFolder, note, sourceCapture.correctedBuffer, sourceCapture.sampleRate))
                result.notesExported[(size_t) note] = true;
        }
        else
        {
            auto semitoneShift = note - sourceNote;
            auto shifted = resampleBySemitoneCents(sourceCapture.correctedBuffer, semitoneShift * 100.0);

            if (progressCallback)
                progressCallback("Note " + juce::String(note) + " -> gap-filled from note " + juce::String(sourceNote));

            if (writeNoteFile(outputFolder, note, shifted, sourceCapture.sampleRate))
                result.notesExported[(size_t) note] = true;
        }
    }

    result.success = true;
    return result;
}
