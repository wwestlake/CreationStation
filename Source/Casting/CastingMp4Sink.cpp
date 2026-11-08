#include "CastingMp4Sink.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#if JUCE_WINDOWS
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <winrt/base.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "propsys.lib")
#endif

namespace cs::casting
{

#if JUCE_WINDOWS

namespace
{
// A seekable, growing, in-memory IMFByteStream. MFCreateFMPEG4MediaSink requires its output
// IMFByteStream to be both writable and seekable (the muxer rewrites small header fields as it
// finalizes each fragment); this stream satisfies that over a plain memory buffer rather than a
// real file, and reports every byte range written -- in order, exactly once -- to a callback so
// CastingStreamServer can forward it to a receiver without ever touching a temp file (the
// suite's storage-boundary rule keeps OS-filesystem writes to the VFS root pointer only, and a
// cast session's live video is not suite-managed content in the first place).
struct MemoryByteStream : winrt::implements<MemoryByteStream, IMFByteStream>
{
    std::vector<uint8_t> buffer;
    uint64_t position = 0;
    std::function<void(const void*, size_t)> onWritten;
    std::mutex mutex;

    HRESULT __stdcall GetCapabilities(DWORD* caps) noexcept override
    {
        if (! caps) return E_POINTER;
        *caps = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_WRITABLE | MFBYTESTREAM_IS_SEEKABLE;
        return S_OK;
    }

    HRESULT __stdcall GetLength(QWORD* length) noexcept override
    {
        if (! length) return E_POINTER;
        std::lock_guard<std::mutex> lock(mutex);
        *length = buffer.size();
        return S_OK;
    }

    HRESULT __stdcall SetLength(QWORD length) noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex);
        buffer.resize((size_t) length);
        return S_OK;
    }

    HRESULT __stdcall GetCurrentPosition(QWORD* pos) noexcept override
    {
        if (! pos) return E_POINTER;
        *pos = position;
        return S_OK;
    }

    HRESULT __stdcall SetCurrentPosition(QWORD pos) noexcept override
    {
        position = pos;
        return S_OK;
    }

    HRESULT __stdcall IsEndOfStream(BOOL* eos) noexcept override
    {
        if (! eos) return E_POINTER;
        std::lock_guard<std::mutex> lock(mutex);
        *eos = (position >= buffer.size()) ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT __stdcall Read(BYTE* dest, ULONG count, ULONG* numRead) noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex);
        const uint64_t available = position < buffer.size() ? buffer.size() - position : 0;
        const ULONG toCopy = (ULONG) std::min<uint64_t>(available, count);
        if (toCopy > 0)
            memcpy(dest, buffer.data() + position, toCopy);
        position += toCopy;
        if (numRead) *numRead = toCopy;
        return S_OK;
    }

    HRESULT __stdcall BeginRead(BYTE* dest, ULONG count, IMFAsyncCallback* callback, IUnknown* state) noexcept override
    {
        return completeSynchronously(callback, state, [this, dest, count](ULONG* out) { return Read(dest, count, out); });
    }

    HRESULT __stdcall EndRead(IMFAsyncResult* result, ULONG* numRead) noexcept override
    {
        return finishSynchronous(result, numRead);
    }

    HRESULT __stdcall Write(const BYTE* src, ULONG count, ULONG* numWritten) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            const uint64_t endPos = position + count;
            if (endPos > buffer.size())
                buffer.resize((size_t) endPos);
            memcpy(buffer.data() + position, src, count);
            position += count;
        }
        if (onWritten && count > 0)
            onWritten(src, count);
        if (numWritten) *numWritten = count;
        return S_OK;
    }

    HRESULT __stdcall BeginWrite(const BYTE* src, ULONG count, IMFAsyncCallback* callback, IUnknown* state) noexcept override
    {
        return completeSynchronously(callback, state, [this, src, count](ULONG* out) { return Write(src, count, out); });
    }

    HRESULT __stdcall EndWrite(IMFAsyncResult* result, ULONG* numWritten) noexcept override
    {
        return finishSynchronous(result, numWritten);
    }

    HRESULT __stdcall Seek(MFBYTESTREAM_SEEK_ORIGIN origin, LONGLONG offset, DWORD, QWORD* newPos) noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex);
        int64_t base = (origin == msoBegin) ? 0 : (int64_t) position;
        int64_t result = base + offset;
        if (result < 0) return E_INVALIDARG;
        position = (uint64_t) result;
        if (newPos) *newPos = position;
        return S_OK;
    }

    HRESULT __stdcall Flush() noexcept override { return S_OK; }
    HRESULT __stdcall Close() noexcept override { return S_OK; }

private:
    // IMFByteStream's async Begin/End pair is satisfied here by doing the work immediately and
    // completing the async result inline -- correct (callers are required to tolerate a
    // same-thread, immediate completion) and far simpler than a real worker-queue round trip for
    // a stream that is, underneath, just memory.
    ULONG lastCount = 0;
    HRESULT completeSynchronously(IMFAsyncCallback* callback, IUnknown* state, const std::function<HRESULT(ULONG*)>& work)
    {
        ULONG count = 0;
        const HRESULT hr = work(&count);
        lastCount = count;
        if (! callback)
            return hr;
        winrt::com_ptr<IMFAsyncResult> result;
        if (FAILED(MFCreateAsyncResult(nullptr, callback, state, result.put())))
            return hr;
        result->SetStatus(hr);
        return MFInvokeCallback(result.get());
    }
    HRESULT finishSynchronous(IMFAsyncResult*, ULONG* out) noexcept
    {
        if (out) *out = lastCount;
        return S_OK;
    }
};
}

struct CastingMp4Sink::Impl
{
    bool mfStarted = false;
    winrt::com_ptr<MemoryByteStream> byteStream;
    winrt::com_ptr<IMFMediaSink> mediaSink;
    winrt::com_ptr<IMFSinkWriter> sinkWriter;
    DWORD videoStreamIndex = 0;
    bool usingNv12 = false;
    int width = 0, height = 0;
    double frameDurationSeconds = 1.0 / 30.0;
    bool running = false;

    ~Impl() { stop(); }

    bool start(int w, int h, double frameRate, std::function<void(const void*, size_t)> onBytes)
    {
        width = w;
        height = h;
        frameDurationSeconds = frameRate > 0.0 ? 1.0 / frameRate : 1.0 / 30.0;

        if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE)))
            return false;
        mfStarted = true;

        byteStream = winrt::make_self<MemoryByteStream>();
        byteStream->onWritten = std::move(onBytes);

        winrt::com_ptr<IMFMediaType> outputType;
        if (FAILED(MFCreateMediaType(outputType.put())))
            return fail();
        outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        outputType->SetUINT32(MF_MT_AVG_BITRATE, 4'000'000);
        outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(outputType.get(), MF_MT_FRAME_SIZE, (UINT32) w, (UINT32) h);
        MFSetAttributeRatio(outputType.get(), MF_MT_FRAME_RATE, (UINT32) (frameRate * 1000.0), 1000);
        MFSetAttributeRatio(outputType.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

        winrt::com_ptr<IMFMediaSink> sink;
        if (FAILED(MFCreateFMPEG4MediaSink(byteStream.get(), outputType.get(), nullptr, sink.put())))
            return fail();
        mediaSink = sink;

        winrt::com_ptr<IMFAttributes> writerAttributes;
        MFCreateAttributes(writerAttributes.put(), 1);
        if (writerAttributes)
            writerAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

        if (FAILED(MFCreateSinkWriterFromMediaSink(mediaSink.get(), writerAttributes.get(), sinkWriter.put())))
            return fail();

        // Try an uncompressed RGB32 input first -- our captured frames are already BGRA and
        // this avoids a manual colour-space conversion entirely if the sink writer can resolve
        // an RGB32 -> H.264 transform chain on its own. If it can't, fall back to feeding NV12
        // (the format Media Foundation's H.264 encoder is guaranteed to accept) and convert
        // BGRA -> NV12 ourselves in encodeFrame().
        winrt::com_ptr<IMFMediaType> rgbInput;
        if (FAILED(MFCreateMediaType(rgbInput.put())))
            return fail();
        rgbInput->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        rgbInput->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        rgbInput->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(rgbInput.get(), MF_MT_FRAME_SIZE, (UINT32) w, (UINT32) h);
        MFSetAttributeRatio(rgbInput.get(), MF_MT_FRAME_RATE, (UINT32) (frameRate * 1000.0), 1000);
        rgbInput->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32) w * 4);

        videoStreamIndex = 0;
        if (SUCCEEDED(sinkWriter->SetInputMediaType(videoStreamIndex, rgbInput.get(), nullptr)))
        {
            usingNv12 = false;
        }
        else
        {
            winrt::com_ptr<IMFMediaType> nv12Input;
            if (FAILED(MFCreateMediaType(nv12Input.put())))
                return fail();
            nv12Input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            nv12Input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            nv12Input->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
            MFSetAttributeSize(nv12Input.get(), MF_MT_FRAME_SIZE, (UINT32) w, (UINT32) h);
            MFSetAttributeRatio(nv12Input.get(), MF_MT_FRAME_RATE, (UINT32) (frameRate * 1000.0), 1000);
            nv12Input->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32) w);
            if (FAILED(sinkWriter->SetInputMediaType(videoStreamIndex, nv12Input.get(), nullptr)))
                return fail();
            usingNv12 = true;
        }

        if (FAILED(sinkWriter->BeginWriting()))
            return fail();

        running = true;
        return true;
    }

    bool fail()
    {
        stop();
        return false;
    }

    bool encode(const juce::Image& image, double captureTimeSeconds)
    {
        if (! running)
            return false;

        const LONGLONG sampleTimeHns = (LONGLONG) (captureTimeSeconds * 1.0e7);
        const LONGLONG durationHns = (LONGLONG) (frameDurationSeconds * 1.0e7);

        const size_t pixelBytes = usingNv12 ? (size_t) (width * height * 3 / 2) : (size_t) (width * height * 4);
        winrt::com_ptr<IMFMediaBuffer> buffer;
        if (FAILED(MFCreateMemoryBuffer((DWORD) pixelBytes, buffer.put())))
            return false;

        BYTE* dest = nullptr;
        if (FAILED(buffer->Lock(&dest, nullptr, nullptr)))
            return false;

        juce::Image::BitmapData src(image, juce::Image::BitmapData::readOnly);
        if (usingNv12)
            convertBgraToNv12(src, dest);
        else
        {
            for (int y = 0; y < height; ++y)
                memcpy(dest + (size_t) y * width * 4, src.getLinePointer(y), (size_t) width * 4);
        }

        buffer->Unlock();
        buffer->SetCurrentLength((DWORD) pixelBytes);

        winrt::com_ptr<IMFSample> sample;
        if (FAILED(MFCreateSample(sample.put())))
            return false;
        sample->AddBuffer(buffer.get());
        sample->SetSampleTime(sampleTimeHns);
        sample->SetSampleDuration(durationHns);

        return SUCCEEDED(sinkWriter->WriteSample(videoStreamIndex, sample.get()));
    }

    void convertBgraToNv12(const juce::Image::BitmapData& src, BYTE* dest)
    {
        auto clamp255 = [](int v) { return (uint8_t) juce::jlimit(0, 255, v); };
        BYTE* yPlane = dest;
        BYTE* uvPlane = dest + (size_t) width * height;

        for (int y = 0; y < height; ++y)
        {
            const uint8_t* row = src.getLinePointer(y);
            for (int x = 0; x < width; ++x)
            {
                const uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2];
                yPlane[(size_t) y * width + x] = clamp255((299 * r + 587 * g + 114 * b) / 1000);
            }
        }
        for (int y = 0; y < height; y += 2)
        {
            const uint8_t* rowA = src.getLinePointer(y);
            const uint8_t* rowB = (y + 1 < height) ? src.getLinePointer(y + 1) : rowA;
            for (int x = 0; x < width; x += 2)
            {
                int sumR = 0, sumG = 0, sumB = 0, n = 0;
                for (int dy = 0; dy < 2; ++dy)
                {
                    const uint8_t* row = dy == 0 ? rowA : rowB;
                    for (int dx = 0; dx < 2 && x + dx < width; ++dx)
                    {
                        const uint8_t* p = row + (x + dx) * 4;
                        sumB += p[0]; sumG += p[1]; sumR += p[2]; ++n;
                    }
                }
                const int r = sumR / n, g = sumG / n, b = sumB / n;
                const uint8_t u = clamp255(((-169 * r - 331 * g + 499 * b) / 1000) + 128);
                const uint8_t v = clamp255(((499 * r - 418 * g - 81 * b) / 1000) + 128);
                const size_t uvIndex = (size_t) (y / 2) * width + (x & ~1);
                uvPlane[uvIndex] = u;
                uvPlane[uvIndex + 1] = v;
            }
        }
    }

    void stop()
    {
        if (sinkWriter)
        {
            sinkWriter->Finalize();
            sinkWriter = nullptr;
        }
        mediaSink = nullptr;
        byteStream = nullptr;
        running = false;
        if (mfStarted)
        {
            MFShutdown();
            mfStarted = false;
        }
    }
};

#else // ! JUCE_WINDOWS -- Media Foundation is Windows-only.

struct CastingMp4Sink::Impl
{
    bool start(int, int, double, std::function<void(const void*, size_t)>) { return false; }
    bool encode(const juce::Image&, double) { return false; }
    void stop() {}
    bool running = false;
};

#endif

CastingMp4Sink::CastingMp4Sink() : impl(std::make_unique<Impl>()) {}
CastingMp4Sink::~CastingMp4Sink() { stop(); }

bool CastingMp4Sink::start(int frameWidth, int frameHeight, double frameRate)
{
#if JUCE_WINDOWS
    return impl->start(frameWidth, frameHeight, frameRate, onBytesProduced);
#else
    juce::ignoreUnused(frameWidth, frameHeight, frameRate);
    return false;
#endif
}

bool CastingMp4Sink::encodeFrame(const juce::Image& bgraImage, double captureTimeSeconds)
{
    return impl->encode(bgraImage, captureTimeSeconds);
}

void CastingMp4Sink::stop() { impl->stop(); }

bool CastingMp4Sink::isRunning() const noexcept { return impl->running; }

} // namespace cs::casting
