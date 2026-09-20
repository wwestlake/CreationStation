#include "VideoGlView.h"
#include "VideoShaders.h"

using namespace juce::gl;

namespace cs
{
namespace
{
// A rectangle in pixels (origin top-left) as clip-space (x0, y0, x1, y1) for a target of the given size.
void setRect(juce::OpenGLShaderProgram& program, juce::Rectangle<float> pixels, int targetWidth, int targetHeight)
{
    const auto x0 = pixels.getX() / (float) targetWidth * 2.0f - 1.0f;
    const auto x1 = pixels.getRight() / (float) targetWidth * 2.0f - 1.0f;
    const auto y0 = 1.0f - pixels.getBottom() / (float) targetHeight * 2.0f; // bottom edge
    const auto y1 = 1.0f - pixels.getY() / (float) targetHeight * 2.0f;      // top edge
    program.setUniform("uRect", x0, y0, x1, y1);
}
}

// ------------------------------------------------------------------------------------------------ renderer
VideoGlRenderer::VideoGlRenderer()
    : composer(ce::ShaderComposer::SourceProvider(videogl::findVideoShaderSource))
{
}

VideoGlRenderer::~VideoGlRenderer() = default;

void VideoGlRenderer::prepare(juce::OpenGLContext& context)
{
    lastError = {};
    frameProgram = composer.GetProgram(context, "programs/video_quad.vert", "programs/video_frame.frag");
    backgroundProgram = composer.GetProgram(context, "programs/video_quad.vert", "programs/video_background.frag");
    if (frameProgram == nullptr || backgroundProgram == nullptr)
        lastError = "the video shaders could not be compiled";

    textures.clear();
    uploadedFrameIds.clear();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    vertexArray = vao;
}

void VideoGlRenderer::release()
{
    if (vertexArray != 0)
    {
        GLuint vao = vertexArray;
        glDeleteVertexArrays(1, &vao);
        vertexArray = 0;
    }

    textures.clear();
    uploadedFrameIds.clear();
    frameProgram = nullptr;
    backgroundProgram = nullptr;
    composer.ClearCache(); // compiled programs belong to this context, which is going away
}

void VideoGlRenderer::render(const VideoFrameSnapshot& snapshot, int targetWidth, int targetHeight)
{
    glViewport(0, 0, targetWidth, targetHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (! isReady() || snapshot.layers.empty() || targetWidth <= 0 || targetHeight <= 0)
        return;

    // The canvas: the video frame, fitted (letterboxed) inside the target.
    const auto targetRect = juce::Rectangle<float>(0.0f, 0.0f, (float) targetWidth, (float) targetHeight);
    const auto canvasWidth = juce::jmin((float) targetWidth, (float) targetHeight * snapshot.canvasAspect);
    const auto canvasHeight = canvasWidth / snapshot.canvasAspect;
    const auto canvas = juce::Rectangle<float>(canvasWidth, canvasHeight).withCentre(targetRect.getCentre());

    glBindVertexArray(vertexArray);

    backgroundProgram->use();
    setRect(*backgroundProgram, canvas, targetWidth, targetHeight);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // layers are premultiplied by the shader

    frameProgram->use();
    for (size_t i = 0; i < snapshot.layers.size(); ++i)
    {
        const auto& layer = snapshot.layers[i];
        if (! layer.frame.isValid())
            continue;

        if (textures.size() <= i)
        {
            textures.resize(i + 1);
            uploadedFrameIds.resize(i + 1, 0);
        }
        if (textures[i] == nullptr)
            textures[i] = std::make_unique<juce::OpenGLTexture>();

        if (layer.frameId != uploadedFrameIds[i])
        {
            textures[i]->loadImage(layer.frame);
            uploadedFrameIds[i] = layer.frameId;
        }

        // Where the layer sits: fitted inside the canvas, scaled, then moved by a fraction of the canvas.
        const auto pictureAspect = (float) layer.frame.getWidth() / (float) juce::jmax(1, layer.frame.getHeight());
        auto size = juce::Rectangle<float>(canvas.getWidth(), canvas.getWidth() / pictureAspect);
        if (size.getHeight() > canvas.getHeight())
            size = juce::Rectangle<float>(canvas.getHeight() * pictureAspect, canvas.getHeight());
        size = size * juce::jmax(0.0f, layer.scale);
        const auto centre = canvas.getCentre() + juce::Point<float>(layer.x * canvas.getWidth(), layer.y * canvas.getHeight());

        glActiveTexture(GL_TEXTURE0);
        textures[i]->bind();
        setRect(*frameProgram, size.withCentre(centre), targetWidth, targetHeight);
        frameProgram->setUniform("uFrame", 0);
        frameProgram->setUniform("uOpacity", layer.opacity);
        frameProgram->setUniform("uKeyEnabled", layer.keyEnabled ? 1 : 0);
        frameProgram->setUniform("uKeyColor", layer.keyColor[0], layer.keyColor[1], layer.keyColor[2]);
        frameProgram->setUniform("uTolerance", layer.tolerance);
        frameProgram->setUniform("uSoftness", layer.softness);
        frameProgram->setUniform("uSpill", layer.spill);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// ------------------------------------------------------------------------------------------------ view
VideoGlView::VideoGlView()
{
    published.store(std::make_shared<const VideoFrameSnapshot>());

    context.setRenderer(this);
    context.setContinuousRepainting(false); // drawn when a new snapshot arrives, not in a busy loop
    context.setComponentPaintingEnabled(false);
    context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    context.attachTo(*this);
}

VideoGlView::~VideoGlView()
{
    context.detach();
}

void VideoGlView::setLayers(std::vector<VideoLayer> layers)
{
    // A layer whose picture is the very same one as last time keeps its id, so it is not uploaded again.
    lastPixelData.resize(layers.size(), nullptr);
    lastFrameIds.resize(layers.size(), 0);
    for (size_t i = 0; i < layers.size(); ++i)
    {
        const void* pixels = layers[i].frame.isValid() ? layers[i].frame.getPixelData().get() : nullptr;
        if (pixels != lastPixelData[i] || lastFrameIds[i] == 0)
        {
            lastPixelData[i] = pixels;
            lastFrameIds[i] = nextFrameId++;
        }
        layers[i].frameId = lastFrameIds[i];
    }

    auto snapshot = std::make_shared<VideoFrameSnapshot>();
    snapshot->layers = std::move(layers);
    if (! snapshot->layers.empty() && snapshot->layers.front().frame.isValid())
        snapshot->canvasAspect = (float) snapshot->layers.front().frame.getWidth() / (float) juce::jmax(1, snapshot->layers.front().frame.getHeight());

    published.store(std::move(snapshot));
    context.triggerRepaint();
    repaint();
}

void VideoGlView::newOpenGLContextCreated()
{
    renderer.prepare(context);
    rendererError = renderer.getLastError();
    rendererReady.store(renderer.isReady());
    ++contextsCreated;
}

void VideoGlView::renderOpenGL()
{
    // The start of the frame: take the latest snapshot and use only that.
    const auto snapshot = published.load();
    const auto scale = (float) context.getRenderingScale();
    const auto width = juce::roundToInt(scale * (float) getWidth());
    const auto height = juce::roundToInt(scale * (float) getHeight());
    renderer.render(*snapshot, width, height);
    ++framesDrawn;
    lastLayerCount.store((int) snapshot->layers.size());
    lastWidth.store(width);
    lastHeight.store(height);

    if (captureRequested.exchange(false) && width > 0 && height > 0)
    {
        std::vector<std::uint8_t> rgba((size_t) width * (size_t) height * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, context.getFrameBufferID());
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

        juce::Image image(juce::Image::ARGB, width, height, true);
        juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < height; ++y) // GL rows are bottom-first
            for (int x = 0; x < width; ++x)
            {
                const auto* p = rgba.data() + ((size_t) (height - 1 - y) * (size_t) width + (size_t) x) * 4;
                data.setPixelColour(x, y, juce::Colour::fromRGB(p[0], p[1], p[2]));
            }

        const juce::ScopedLock lock(captureLock);
        capturedFrame = image;
    }
}

juce::Image VideoGlView::captureOnScreenFrame()
{
    {
        const juce::ScopedLock lock(captureLock);
        capturedFrame = {};
    }

    captureRequested.store(true);
    context.triggerRepaint();

    const auto deadline = juce::Time::getMillisecondCounter() + 3000;
    while (juce::Time::getMillisecondCounter() < deadline)
    {
        juce::Thread::sleep(20); // the GL thread does the drawing; nothing here needs the message loop
        {
            const juce::ScopedLock lock(captureLock);
            if (capturedFrame.isValid())
                return capturedFrame;
        }
        context.triggerRepaint();
    }
    return {};
}

void VideoGlView::openGLContextClosing()
{
    rendererReady.store(false);
    renderer.release();
}

juce::Image VideoGlView::renderToImage(int width, int height)
{
    juce::Image result(juce::Image::ARGB, width, height, true);
    if (! context.isAttached())
        return result;

    const auto snapshot = published.load();
    context.executeOnGLThread([this, snapshot, width, height, &result](juce::OpenGLContext& glContext)
    {
        juce::OpenGLFrameBuffer buffer;
        if (! buffer.initialise(glContext, width, height))
            return;

        buffer.makeCurrentRenderingTarget();
        renderer.render(*snapshot, width, height);
        buffer.releaseAsRenderingTarget();

        // GL hands the rows back bottom-first; an Image is top-first.
        std::vector<juce::PixelARGB> pixels((size_t) width * (size_t) height);
        buffer.readPixels(pixels.data(), { 0, 0, width, height });
        buffer.release();

        juce::Image::BitmapData data(result, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < height; ++y)
            std::memcpy(data.getLinePointer(y), pixels.data() + (size_t) (height - 1 - y) * (size_t) width, (size_t) width * sizeof(juce::PixelARGB));
    }, true);

    return result;
}

juce::String VideoGlView::describeState() const
{
    juce::String text = "OpenGL ";
    if (rendererReady.load())
        text << "ready";
    else if (rendererError.isNotEmpty())
        text << "NOT ready: " << rendererError;
    else
        text << (context.isAttached() ? "starting..." : "not attached to a window");

    text << "  |  layers " << lastLayerCount.load() << "  |  frames drawn " << (juce::int64) framesDrawn.load()
         << "  |  " << lastWidth.load() << "x" << lastHeight.load();
    return text;
}

void VideoGlView::paint(juce::Graphics& g)
{
    // Only seen if OpenGL is unavailable: say so, rather than showing a blank panel.
    g.fillAll(juce::Colour(0xff0a0e14));
    if (! context.isAttached() || ! rendererReady.load())
    {
        g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText("Video (waiting for the graphics context)", getLocalBounds(), juce::Justification::centred);
    }
}

void VideoGlView::resized()
{
    if (onSizeChanged)
        onSizeChanged();
}
}
