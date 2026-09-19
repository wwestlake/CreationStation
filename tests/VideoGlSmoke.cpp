// Headless check of the GPU video renderer: renders a known picture offscreen through the real GLSL shader and
// checks actual pixels - orientation, that the green-screen effect removes only the key colour, and that the
// parameters published as a snapshot are what the shader uses. Needs an OpenGL driver; if none is available the test
// says so and exits 77 (skipped) instead of failing. Exit 0 = pass.
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

// The picture: top-left blue, top-right green, bottom-left red, bottom-right green.
juce::Image makeTestPicture()
{
    juce::Image image(juce::Image::ARGB, 64, 64, true);
    juce::Graphics g(image);
    g.setColour(juce::Colour::fromRGB(0, 0, 255));   g.fillRect(0, 0, 32, 32);
    g.setColour(juce::Colour::fromRGB(0, 255, 0));   g.fillRect(32, 0, 32, 32);
    g.setColour(juce::Colour::fromRGB(255, 0, 0));   g.fillRect(0, 32, 32, 32);
    g.setColour(juce::Colour::fromRGB(0, 255, 0));   g.fillRect(32, 32, 32, 32);
    return image;
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

    // Wait for the graphics context to come up.
    const auto deadline = juce::Time::getMillisecondCounter() + 8000;
    while (! view.isRendererReady() && juce::Time::getMillisecondCounter() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

    if (! view.isRendererReady())
    {
        std::printf("SKIPPED: no usable OpenGL context here (%s)\n", view.getRendererError().toRawUTF8());
        return 77;
    }

    view.setImage(makeTestPicture());

    // 1. No effect: the picture comes out as it went in, the right way up.
    auto out = view.renderToImage(64, 64);
    for (auto [x, y] : { std::pair{ 16, 16 }, std::pair{ 48, 16 }, std::pair{ 16, 48 }, std::pair{ 48, 48 } })
        std::printf("   pixel (%d,%d) = rgb(%d,%d,%d)\n", x, y, (int) at(out, x, y).getRed(), (int) at(out, x, y).getGreen(), (int) at(out, x, y).getBlue());
    check(isNear(at(out, 16, 16), 0, 0, 255), "top-left is blue (right way up)");
    check(isNear(at(out, 48, 16), 0, 255, 0), "top-right is green");
    check(isNear(at(out, 16, 48), 255, 0, 0), "bottom-left is red");
    check(isNear(at(out, 48, 48), 0, 255, 0), "bottom-right is green");

    // 2. Green screen on: green disappears (checker shows through), other colours stay.
    view.setKeyEnabled(true);
    out = view.renderToImage(64, 64);
    check(at(out, 48, 16).getGreen() < 90 && at(out, 48, 48).getGreen() < 90, "green screen removes the green");
    check(isNear(at(out, 16, 16), 0, 0, 255), "blue is left alone by a green key");
    check(isNear(at(out, 16, 48), 255, 0, 0), "red is left alone by a green key");

    // 3. The key colour is a parameter: switching it to blue removes blue and keeps green.
    view.setKeyColor(0.0f, 0.0f, 1.0f);
    out = view.renderToImage(64, 64);
    check(at(out, 16, 16).getBlue() < 90, "a blue key removes the blue");
    check(isNear(at(out, 48, 16), 0, 255, 0), "green is left alone by a blue key");

    // 4. Turning the effect off puts everything back.
    view.setKeyEnabled(false);
    out = view.renderToImage(64, 64);
    check(isNear(at(out, 16, 16), 0, 0, 255) && isNear(at(out, 48, 16), 0, 255, 0), "effect off: the picture is untouched");

    // 5. Nothing at the playhead: a plain dark frame, no picture.
    view.setIdle();
    out = view.renderToImage(64, 64);
    check(at(out, 16, 16).getBrightness() < 0.2f && at(out, 48, 48).getBrightness() < 0.2f, "idle draws no picture");

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    window.setVisible(false);
    return failures == 0 ? 0 : 1;
}
