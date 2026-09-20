// Headless check of the GPU video compositor: renders known pictures offscreen through the real GLSL shaders and
// checks actual pixels - orientation, the green-screen effect, picture-in-picture layout, layer stacking, and opacity.
// Needs an OpenGL driver; if none is available the test says so and exits 77 (skipped) instead of failing.
// Exit 0 = pass.
#include <JuceHeader.h>

#include "Video/Gl/VideoGlView.h"

#include <cstdio>

namespace
{
int failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

bool isNear(juce::Colour c, int r, int g, int b, int tolerance = 40)
{
    return std::abs((int) c.getRed() - r) <= tolerance && std::abs((int) c.getGreen() - g) <= tolerance
        && std::abs((int) c.getBlue() - b) <= tolerance;
}

juce::Image solid(juce::Colour colour)
{
    juce::Image image(juce::Image::ARGB, 64, 64, true);
    juce::Graphics g(image);
    g.fillAll(colour);
    return image;
}

// Top-left blue, top-right green, bottom-left red, bottom-right green.
juce::Image quadrants()
{
    juce::Image image(juce::Image::ARGB, 64, 64, true);
    juce::Graphics g(image);
    g.setColour(juce::Colour::fromRGB(0, 0, 255));   g.fillRect(0, 0, 32, 32);
    g.setColour(juce::Colour::fromRGB(0, 255, 0));   g.fillRect(32, 0, 32, 32);
    g.setColour(juce::Colour::fromRGB(255, 0, 0));   g.fillRect(0, 32, 32, 32);
    g.setColour(juce::Colour::fromRGB(0, 255, 0));   g.fillRect(32, 32, 32, 32);
    return image;
}

cs::VideoLayer layer(juce::Image frame)
{
    cs::VideoLayer l;
    l.frame = std::move(frame);
    return l;
}

juce::Colour at(const juce::Image& image, int x, int y) { return image.getPixelAt(x, y); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    cs::VideoGlView view;
    juce::DocumentWindow window("video gl smoke", juce::Colours::black, 0);
    window.setUsingNativeTitleBar(false);
    window.setContentNonOwned(&view, true);
    window.setBounds(-4000, -4000, 256, 256); // on screen as far as the driver is concerned, out of sight
    window.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    window.setVisible(true);

    const auto deadline = juce::Time::getMillisecondCounter() + 8000;
    while (! view.isRendererReady() && juce::Time::getMillisecondCounter() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    if (! view.isRendererReady())
    {
        std::printf("SKIPPED: no usable OpenGL context here (%s)\n", view.getRendererError().toRawUTF8());
        return 77;
    }

    // 1. One layer, no effect: the picture comes out as it went in, the right way up.
    view.setLayers({ layer(quadrants()) });
    auto out = view.renderToImage(64, 64);
    check(isNear(at(out, 16, 16), 0, 0, 255), "top-left is blue (right way up)");
    check(isNear(at(out, 48, 16), 0, 255, 0), "top-right is green");
    check(isNear(at(out, 16, 48), 255, 0, 0), "bottom-left is red");
    check(isNear(at(out, 48, 48), 0, 255, 0), "bottom-right is green");

    // 1b. The ON-SCREEN path (what the app really shows): let the context draw a few frames, then read its frame buffer.
    auto screen = view.captureOnScreenFrame();
    if (screen.isValid() && screen.getWidth() > 8)
    {
        const auto w = screen.getWidth(), h = screen.getHeight();
        // The picture is square, letterboxed in the middle of the view; sample inside each quadrant of it.
        const auto side = juce::jmin(w, h);
        const auto x0 = (w - side) / 2, y0 = (h - side) / 2;
        const auto q = [&](float fx, float fy) { return screen.getPixelAt(x0 + (int) (fx * (float) side), y0 + (int) (fy * (float) side)); };
        check(isNear(q(0.25f, 0.25f), 0, 0, 255) && isNear(q(0.75f, 0.25f), 0, 255, 0) && isNear(q(0.25f, 0.75f), 255, 0, 0),
              "the ON-SCREEN context draws the picture, the right way up");
    }
    else
    {
        check(false, "the on-screen context produced no frame to read back");
    }

    // 1c. The app docks the video panel and then floats it into its own window: the view gets a new window, and the
    // graphics context is torn down and recreated. Drawing must still work afterwards (compiled shaders belong to
    // the context that made them).
    {
        juce::DocumentWindow second("video gl smoke 2", juce::Colours::black, 0);
        second.setUsingNativeTitleBar(false);
        second.setBounds(-4000, -3600, 256, 256);
        second.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        second.setVisible(true);
        second.setContentNonOwned(&view, true); // the view moves to the second window

        const auto until = juce::Time::getMillisecondCounter() + 8000;
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
        while (! view.isRendererReady() && juce::Time::getMillisecondCounter() < until)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(300);

        view.setLayers({ layer(quadrants()) });
        const auto moved = view.captureOnScreenFrame();
        bool drawsAfterMove = false;
        if (moved.isValid() && moved.getWidth() > 8)
        {
            const auto side = juce::jmin(moved.getWidth(), moved.getHeight());
            const auto ox = (moved.getWidth() - side) / 2, oy = (moved.getHeight() - side) / 2;
            drawsAfterMove = isNear(moved.getPixelAt(ox + side / 4, oy + side / 4), 0, 0, 255)
                          && isNear(moved.getPixelAt(ox + side * 3 / 4, oy + side / 4), 0, 255, 0);
        }
        check(drawsAfterMove, "after the view moves to another window (new graphics context) it still draws");

        second.clearContentComponent();
        window.clearContentComponent(); // it still remembers the view; without this it would not re-add it
        window.setContentNonOwned(&view, true); // hand the view back so the rest of the test uses the first window
        juce::MessageManager::getInstance()->runDispatchLoopUntil(300);
        const auto again = juce::Time::getMillisecondCounter() + 8000;
        while (! view.isRendererReady() && juce::Time::getMillisecondCounter() < again)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    }

    // 2. Green screen: green disappears (the dark backdrop shows through), other colours stay.
    auto keyed = layer(quadrants());
    keyed.keyEnabled = true;
    view.setLayers({ keyed });
    out = view.renderToImage(64, 64);
    check(at(out, 48, 16).getGreen() < 90 && at(out, 48, 48).getGreen() < 90, "green screen removes the green");
    check(isNear(at(out, 16, 16), 0, 0, 255) && isNear(at(out, 16, 48), 255, 0, 0), "blue and red are left alone by a green key");

    // 3. The key colour is a parameter.
    keyed.keyColor[0] = 0.0f; keyed.keyColor[1] = 0.0f; keyed.keyColor[2] = 1.0f;
    view.setLayers({ keyed });
    out = view.renderToImage(64, 64);
    check(at(out, 16, 16).getBlue() < 90 && isNear(at(out, 48, 16), 0, 255, 0), "a blue key removes the blue and keeps the green");

    // 4. Picture-in-picture: a small blue layer over a red background.
    auto small = layer(solid(juce::Colour::fromRGB(0, 0, 255)));
    small.scale = 0.5f;
    view.setLayers({ layer(solid(juce::Colour::fromRGB(255, 0, 0))), small });
    out = view.renderToImage(64, 64);
    check(isNear(at(out, 32, 32), 0, 0, 255) && isNear(at(out, 4, 4), 255, 0, 0), "a half-size layer sits in the middle over the background");

    small.x = 0.25f; // move it right by a quarter of the canvas
    view.setLayers({ layer(solid(juce::Colour::fromRGB(255, 0, 0))), small });
    out = view.renderToImage(64, 64);
    check(isNear(at(out, 48, 32), 0, 0, 255) && isNear(at(out, 16, 32), 255, 0, 0), "moving a layer moves it on the canvas");

    // 5. Green screen over a background: the keyed-out area shows the layer below, not a backdrop.
    auto overlay = layer(quadrants());
    overlay.keyEnabled = true;
    view.setLayers({ layer(solid(juce::Colour::fromRGB(255, 255, 0))), overlay });
    out = view.renderToImage(64, 64);
    check(isNear(at(out, 48, 16), 255, 255, 0) && isNear(at(out, 48, 48), 255, 255, 0), "keyed-out green shows the layer underneath");
    check(isNear(at(out, 16, 16), 0, 0, 255), "the rest of the keyed layer stays on top");

    // 6. Opacity: half-transparent blue over red is half of each.
    auto faded = layer(solid(juce::Colour::fromRGB(0, 0, 255)));
    faded.opacity = 0.5f;
    view.setLayers({ layer(solid(juce::Colour::fromRGB(255, 0, 0))), faded });
    out = view.renderToImage(64, 64);
    check(isNear(at(out, 32, 32), 127, 0, 127, 30), "opacity blends a layer with what is below it");

    // 6b. Real video pictures are odd sizes, and a second clip can have a different shape (portrait): both must draw.
    {
        auto wideRed = juce::Image(juce::Image::ARGB, 318, 180, true);
        { juce::Graphics g(wideRed); g.fillAll(juce::Colour::fromRGB(255, 0, 0)); }
        auto tallBlue = juce::Image(juce::Image::ARGB, 180, 320, true);
        { juce::Graphics g(tallBlue); g.fillAll(juce::Colour::fromRGB(0, 0, 255)); }

        auto top = layer(tallBlue);
        top.scale = 0.5f;
        view.setLayers({ layer(wideRed), top });
        out = view.renderToImage(320, 180);
        check(isNear(at(out, 160, 90), 0, 0, 255) && isNear(at(out, 10, 10), 255, 0, 0),
              "odd-sized and differently shaped layers both draw (tall blue over wide red)");

        // A different set of layers every frame, as during playback (new pictures each time).
        bool everyFrameDraws = true;
        for (int frame = 0; frame < 6 && everyFrameDraws; ++frame)
        {
            auto next = juce::Image(juce::Image::ARGB, 318, 180, true);
            { juce::Graphics g(next); g.fillAll(juce::Colour::fromRGB(255, 0, 0)); }
            auto smallTop = layer(juce::Image(tallBlue.createCopy()));
            smallTop.scale = 0.3f + 0.05f * (float) frame;
            view.setLayers({ layer(next), smallTop });
            out = view.renderToImage(320, 180);
            everyFrameDraws = isNear(at(out, 160, 90), 0, 0, 255) && isNear(at(out, 10, 10), 255, 0, 0);
        }
        check(everyFrameDraws, "two layers keep drawing as new pictures arrive every frame");
    }

    // 7. Nothing at the playhead: no picture.
    view.setIdle();
    out = view.renderToImage(64, 64);
    check(at(out, 16, 16).getBrightness() < 0.2f && at(out, 48, 48).getBrightness() < 0.2f, "no layers draws no picture");

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    window.setVisible(false);
    return failures == 0 ? 0 : 1;
}
