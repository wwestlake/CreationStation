#pragma once

#include <JuceHeader.h>

#include "Render/Shaders/ShaderComposer.h"

#include <atomic>
#include <memory>
#include <vector>

namespace cs
{
// One video layer as the renderer sees it: a picture, where it sits on the canvas, and the effect applied to it.
struct VideoLayer
{
    juce::Image frame;          // the decoded picture (shared, read-only)
    std::uint64_t frameId = 0;  // changes when `frame` does, so the texture is only re-uploaded when needed

    // Layout on the canvas. scale 1 fits the picture inside the canvas; x/y move it by a fraction of the canvas
    // (positive x right, positive y down); opacity 1 is solid.
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    float opacity = 1.0f;

    // Green-screen (chroma key) effect.
    bool keyEnabled = false;
    float keyColor[3] = { 0.0f, 1.0f, 0.0f };
    float tolerance = 0.30f;
    float softness = 0.10f;
    float spill = 0.50f;
};

// Everything the render thread needs for one frame, published as one immutable object - the same idea as the
// Engine's FrameSnapshot. The control side (the message thread today; FRust nodes, automation lanes and sliders
// through the same door later) builds a new snapshot whenever anything changes and swaps it in. The render thread
// takes the latest one at the start of each frame and uses it, unchanged, for the whole frame. It never waits for
// the control side, and the control side never waits for it.
struct VideoFrameSnapshot
{
    std::vector<VideoLayer> layers; // bottom to top
    float canvasAspect = 16.0f / 9.0f;
};

// The GL-thread half: owns the shader programs, the layer textures and the vertex array. Used by the on-screen view
// and by offscreen rendering (a test today, video export later), so both draw exactly the same way.
class VideoGlRenderer
{
public:
    VideoGlRenderer();
    ~VideoGlRenderer();

    // GL thread only.
    void prepare(juce::OpenGLContext& context);
    void release();
    // Draws the snapshot into the current target: the canvas fitted (letterboxed) inside the target, then every
    // layer bottom to top.
    void render(const VideoFrameSnapshot& snapshot, int targetWidth, int targetHeight);
    bool isReady() const noexcept { return frameProgram != nullptr && backgroundProgram != nullptr; }
    juce::String getLastError() const { return lastError; }

private:
    ce::ShaderComposer composer;
    juce::OpenGLShaderProgram* frameProgram = nullptr;      // owned by the composer's cache
    juce::OpenGLShaderProgram* backgroundProgram = nullptr; // owned by the composer's cache
    std::vector<std::unique_ptr<juce::OpenGLTexture>> textures; // one per layer slot
    std::vector<std::uint64_t> uploadedFrameIds;
    unsigned int vertexArray = 0;
    juce::String lastError;
};

// A video picture drawn by OpenGL, with effects applied on the GPU. Same shape as the plain VideoPreviewComponent
// (setIdle / onSizeChanged) so it drops into the video panel.
class VideoGlView final : public juce::Component, private juce::OpenGLRenderer
{
public:
    VideoGlView();
    ~VideoGlView() override;

    // Publishes the layers (bottom to top) to the render thread. A layer whose `frame` is the same picture as last
    // time is not uploaded again.
    void setLayers(std::vector<VideoLayer> layers);
    void setIdle() { setLayers({}); }

    // Renders the current snapshot into an offscreen buffer of the given size and returns it (blocks until done).
    // For tests, and the same call video export will use. Requires the view to be on screen so it has a context.
    juce::Image renderToImage(int width, int height);
    bool hasContext() const { return context.isAttached(); }
    // What the on-screen context actually draws: asks the next real frame to be read back from the window's own
    // frame buffer (not the offscreen path) and waits for it. For tests. Empty if no frame arrived in time.
    juce::Image captureOnScreenFrame();
    // True once the graphics context is up and the shaders compiled (set on the GL thread).
    bool isRendererReady() const noexcept { return rendererReady.load(); }
    juce::String getRendererError() const { return rendererError; }

    std::function<void()> onSizeChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext context;
    VideoGlRenderer renderer;

    std::atomic<std::shared_ptr<const VideoFrameSnapshot>> published;
    std::vector<const void*> lastPixelData; // per layer slot, to tell a new picture from the same one
    std::vector<std::uint64_t> lastFrameIds;
    std::uint64_t nextFrameId = 1;
    std::atomic<bool> rendererReady { false };
    juce::String rendererError;

    std::atomic<bool> captureRequested { false };
    juce::CriticalSection captureLock;
    juce::Image capturedFrame;
};
}
