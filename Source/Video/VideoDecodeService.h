#pragma once

#include <JuceHeader.h>
#include <memory>

namespace cs
{
// Describes the streams found in an opened video file - populated by VideoDecodeService::open,
// valid == false means the file couldn't be opened or has no usable video stream.
struct VideoStreamInfo
{
    bool valid = false;
    int width = 0;
    int height = 0;
    double durationSeconds = 0.0;
    double frameRate = 0.0;
    bool hasAudio = false;
    double audioSampleRate = 0.0;
    int audioNumChannels = 0;
};

// Custom hardware-accelerated video decode for the video track's scrub/thumbnail display -
// deliberately not JUCE's juce_video module. Pipeline: Media Foundation opens the file and
// decodes via D3D11VA hardware acceleration straight into an NV12 GPU texture (no CPU roundtrip
// for the decode itself); an HLSL pixel shader does the BT.709 YUV->RGB conversion into an RGBA8
// D3D11 render target; only that final small render target gets read back to the CPU, for
// painting into a juce::Image. Every VideoDecodeService instance shares one process-wide D3D11
// device/DXGI device manager (see the .cpp) - construction/destruction just ref-counts it.
class VideoDecodeService
{
public:
    VideoDecodeService();
    ~VideoDecodeService();

    VideoDecodeService(const VideoDecodeService&) = delete;
    VideoDecodeService& operator=(const VideoDecodeService&) = delete;

    // Opens a video file for decode. Call from a background thread - this does file I/O and
    // media-type negotiation with the decoder, neither of which is safe to block the message
    // thread on. Returns stream info with valid == false on failure (unreadable file, no
    // decodable video stream, no hardware decoder available for the codec).
    VideoStreamInfo open(const juce::File& file);
    void close();
    bool isOpen() const noexcept;

    // Seeks to sourceSeconds and decodes the nearest frame, then runs the GPU BT.709 conversion
    // pass scaled to at most maxOutputWidth x maxOutputHeight (aspect-preserved), and reads the
    // result back into a JUCE image ready to paint. Call from a background thread, same reason as
    // open(). Returns an invalid juce::Image on failure (end of stream, decode error).
    juce::Image decodeFrameAt(double sourceSeconds, int maxOutputWidth = 320, int maxOutputHeight = 180);

    // Demuxes and decodes the file's embedded audio track (if any) to 32-bit float PCM at its
    // native sample rate/channel count (see VideoStreamInfo::audioSampleRate/audioNumChannels).
    // Returns false if there's no audio stream or decode failed; destination is resized to fit.
    bool decodeAudioToFloatPCM(juce::AudioBuffer<float>& destination);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
