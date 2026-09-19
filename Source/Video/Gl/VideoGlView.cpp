#include "VideoGlView.h"
#include "VideoShaders.h"

using namespace juce::gl;

namespace cs
{
// ------------------------------------------------------------------------------------------------ renderer
VideoGlRenderer::VideoGlRenderer()
    : composer(ce::ShaderComposer::SourceProvider(videogl::findVideoShaderSource))
{
}

VideoGlRenderer::~VideoGlRenderer() = default;

void VideoGlRenderer::prepare(juce::OpenGLContext& context)
{
    lastError = {};
    program = composer.GetProgram(context, "programs/video_fullscreen.vert", "programs/video_frame.frag");
    if (program == nullptr)
        lastError = "the video shader could not be compiled";

    texture = std::make_unique<juce::OpenGLTexture>();
    uploadedFrameId = 0;

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

    texture.reset();
    program = nullptr;
}

void VideoGlRenderer::render(juce::OpenGLContext&, const VideoFrameSnapshot& snapshot, juce::Rectangle<int> area, int targetWidth, int targetHeight)
{
    glViewport(0, 0, targetWidth, targetHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (program == nullptr || texture == nullptr || snapshot.idle || ! snapshot.frame.isValid())
        return;

    if (snapshot.frameId != uploadedFrameId)
    {
        texture->loadImage(snapshot.frame);
        uploadedFrameId = snapshot.frameId;
    }

    // Fit the picture inside `area` without stretching it.
    const auto imageAspect = (float) snapshot.frame.getWidth() / (float) juce::jmax(1, snapshot.frame.getHeight());
    auto fitted = area;
    if ((float) area.getWidth() / (float) juce::jmax(1, area.getHeight()) > imageAspect)
        fitted = area.withSizeKeepingCentre(juce::roundToInt((float) area.getHeight() * imageAspect), area.getHeight());
    else
        fitted = area.withSizeKeepingCentre(area.getWidth(), juce::roundToInt((float) area.getWidth() / imageAspect));

    // GL's origin is the bottom-left of the target; `area` is measured from the top-left.
    glViewport(fitted.getX(), targetHeight - fitted.getBottom(), fitted.getWidth(), fitted.getHeight());

    program->use();
    glActiveTexture(GL_TEXTURE0);
    texture->bind();
    program->setUniform("uFrame", 0);
    program->setUniform("uKeyEnabled", snapshot.keyEnabled ? 1 : 0);
    program->setUniform("uKeyColor", snapshot.keyColor[0], snapshot.keyColor[1], snapshot.keyColor[2]);
    program->setUniform("uTolerance", snapshot.tolerance);
    program->setUniform("uSoftness", snapshot.softness);
    program->setUniform("uSpill", snapshot.spill);

    glBindVertexArray(vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

// ------------------------------------------------------------------------------------------------ view
VideoGlView::VideoGlView()
{
    published.store(std::make_shared<const VideoFrameSnapshot>(working));

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

void VideoGlView::publish()
{
    published.store(std::make_shared<const VideoFrameSnapshot>(working));
    context.triggerRepaint();
    repaint();
}

void VideoGlView::setImage(juce::Image newImage)
{
    working.frame = std::move(newImage);
    working.frameId = nextFrameId++;
    working.idle = ! working.frame.isValid();
    publish();
}

void VideoGlView::setIdle()
{
    if (working.idle)
        return;

    working.idle = true;
    working.frame = {};
    publish();
}

void VideoGlView::setKeyEnabled(bool enabled)
{
    working.keyEnabled = enabled;
    publish();
}

void VideoGlView::setKeyColor(float r, float g, float b)
{
    working.keyColor[0] = r;
    working.keyColor[1] = g;
    working.keyColor[2] = b;
    publish();
}

void VideoGlView::setKeyTolerance(float tolerance, float softness)
{
    working.tolerance = tolerance;
    working.softness = softness;
    publish();
}

void VideoGlView::newOpenGLContextCreated()
{
    renderer.prepare(context);
    rendererError = renderer.getLastError();
    rendererReady.store(renderer.isReady());
}

void VideoGlView::renderOpenGL()
{
    // The start of the frame: take the latest snapshot and use only that.
    const auto snapshot = published.load();
    const auto scale = (float) context.getRenderingScale();
    const auto width = juce::roundToInt(scale * (float) getWidth());
    const auto height = juce::roundToInt(scale * (float) getHeight());
    renderer.render(context, *snapshot, { 0, 0, width, height }, width, height);
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
        renderer.render(glContext, *snapshot, { 0, 0, width, height }, width, height);
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

void VideoGlView::paint(juce::Graphics& g)
{
    // Only seen if OpenGL is unavailable: say so, rather than showing a blank panel.
    g.fillAll(juce::Colour(0xff0a0e14));
    if (! context.isAttached() || ! renderer.isReady())
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

void VideoGlView::mouseDown(const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu())
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Green screen (test effect)", true, working.keyEnabled);
    menu.addSeparator();
    menu.addItem(2, "Key colour: green", true, working.keyColor[1] > 0.5f && working.keyColor[2] < 0.5f);
    menu.addItem(3, "Key colour: blue", true, working.keyColor[2] > 0.5f && working.keyColor[1] < 0.5f);
    menu.addSeparator();
    menu.addItem(4, "Tight", true, working.tolerance < 0.2f);
    menu.addItem(5, "Normal", true, working.tolerance >= 0.2f && working.tolerance < 0.4f);
    menu.addItem(6, "Loose", true, working.tolerance >= 0.4f);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ event.getScreenX(), event.getScreenY(), 1, 1 }),
                       [safe = juce::Component::SafePointer<VideoGlView>(this)](int result)
                       {
                           if (safe == nullptr)
                               return;

                           switch (result)
                           {
                               case 1: safe->setKeyEnabled(! safe->working.keyEnabled); break;
                               case 2: safe->setKeyColor(0.0f, 1.0f, 0.0f); break;
                               case 3: safe->setKeyColor(0.0f, 0.0f, 1.0f); break;
                               case 4: safe->setKeyTolerance(0.12f, 0.05f); break;
                               case 5: safe->setKeyTolerance(0.30f, 0.10f); break;
                               case 6: safe->setKeyTolerance(0.50f, 0.15f); break;
                               default: break;
                           }
                       });
}
}
