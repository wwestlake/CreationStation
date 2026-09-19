#pragma once

#include <JuceHeader.h>

#include "Render/Shaders/ShaderComposer.h"

#include <atomic>
#include <memory>

namespace cs
{
// Everything the render thread needs for one frame, published as one immutable object - the same idea as the
// Engine's FrameSnapshot. The control side (the message thread today; FRust nodes, automation lanes and sliders
// through the same door later) builds a new snapshot whenever anything changes and swaps it in. The render thread
// takes the latest one at the start of each frame and uses it, unchanged, for the whole frame. It never waits for
// the control side, and the control side never waits for it.
struct VideoFrameSnapshot
{
    juce::Image frame;          // the decoded picture (shared, read-only)
    std::uint64_t frameId = 0;  // changes when `frame` does, so the texture is only re-uploaded when needed
    bool idle = true;           // nothing at the playhead

    // Green-screen (chroma key) effect parameters.
    bool keyEnabled = false;
    float keyColor[3] = { 0.0f, 1.0f, 0.0f };
    float tolerance = 0.30f;
    float softness = 0.10f;
    float spill = 0.50f;
};

// The GL-thread half: owns the shader program, the frame texture and the vertex array. Used by the on-screen view
// and by offscreen rendering (a test today, video export later), so both draw exactly the same way.
class VideoGlRenderer
{
public:
    VideoGlRenderer();
    ~VideoGlRenderer();

    // GL thread only.
    void prepare(juce::OpenGLContext& context);
    void release();
    // Draws the snapshot's picture, fitted (letterboxed) inside the pixel rectangle `area` of the current target.
    void render(juce::OpenGLContext& context, const VideoFrameSnapshot& snapshot, juce::Rectangle<int> area, int targetWidth, int targetHeight);
    bool isReady() const noexcept { return program != nullptr; }
    juce::String getLastError() const { return lastError; }

private:
    ce::ShaderComposer composer;
    juce::OpenGLShaderProgram* program = nullptr; // owned by the composer's cache
    std::unique_ptr<juce::OpenGLTexture> texture;
    std::uint64_t uploadedFrameId = 0;
    unsigned int vertexArray = 0;
    juce::String lastError;
};

// A video picture drawn by OpenGL, with an effect applied on the GPU. Same interface as the plain
// VideoPreviewComponent (setImage / setIdle / onSizeChanged), so it drops into the video panel.
class VideoGlView final : public juce::Component, private juce::OpenGLRenderer
{
public:
    VideoGlView();
    ~VideoGlView() override;

    void setImage(juce::Image newImage);
    void resetDecodingState() { setIdle(); }
    void setIdle();

    // Effect parameters: published to the render thread as a new snapshot.
    void setKeyEnabled(bool enabled);
    void setKeyColor(float r, float g, float b);
    void setKeyTolerance(float tolerance, float softness);
    bool isKeyEnabled() const noexcept { return working.keyEnabled; }

    // Renders the current snapshot into an offscreen buffer of the given size and returns it (blocks until done).
    // For tests, and the same call video export will use. Requires the view to be on screen so it has a context.
    juce::Image renderToImage(int width, int height);
    bool hasContext() const { return context.isAttached(); }
    // True once the graphics context is up and the shader compiled (set on the GL thread).
    bool isRendererReady() const noexcept { return rendererReady.load(); }
    juce::String getRendererError() const { return rendererError; }

    std::function<void()> onSizeChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void publish();

    juce::OpenGLContext context;
    VideoGlRenderer renderer;

    // Message-thread copy that is edited, and the immutable copy the render thread reads.
    VideoFrameSnapshot working;
    std::atomic<std::shared_ptr<const VideoFrameSnapshot>> published;
    std::uint64_t nextFrameId = 1;
    std::atomic<bool> rendererReady { false };
    juce::String rendererError;
};
}
