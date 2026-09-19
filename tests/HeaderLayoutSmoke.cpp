// Headless check that the header bar never loses a control when the window is narrow: at every width from well
// below the window's minimum (920) up, every button must exist, be wide enough to click, sit inside the bar, and not overlap
// its neighbours. Exit code 0 = pass.
#include <JuceHeader.h>

#include <creation/ui/CreationSuiteHeaderBar.h>

#include <cstdio>

namespace
{
int failures = 0;

void check(bool ok, const juce::String& what)
{
    if (! ok)
    {
        std::printf("FAIL  %s\n", what.toRawUTF8());
        ++failures;
    }
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    CreationSuiteHeaderBar bar;
    bar.setStatusLabelVisible(false); // as Station does

    struct Named { const char* name; juce::Component* component; };
    const Named utility[] = {
        { "project", &bar.projectButton }, { "suite", &bar.suiteButton }, { "audio", &bar.audioButton },
        { "assets", &bar.assetsButton }, { "pods", &bar.podsButton }, { "tour", &bar.tourButton }, { "about", &bar.aboutButton } };
    const Named transport[] = {
        { "rewind", &bar.rewindButton }, { "fastForward", &bar.fastForwardButton }, { "stop", &bar.stopButton },
        { "play", &bar.playButton }, { "loop", &bar.loopButton }, { "click", &bar.clickButton }, { "record", &bar.recordButton } };

    int widthsChecked = 0;
    for (int width = 700; width <= 2000; width += 20)
    {
        bar.setBounds(0, 0, width, 118);
        ++widthsChecked;

        const auto label = "width " + juce::String(width) + ": ";
        auto checkRow = [&](const Named* row, int count, const char* rowName)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto b = row[i].component->getBounds();
                check(row[i].component->isVisible(), label + rowName + " " + row[i].name + " is hidden");
                check(b.getWidth() >= 24 && b.getHeight() >= 20, label + rowName + " " + row[i].name + " is too small to click ("
                                                                    + juce::String(b.getWidth()) + "x" + juce::String(b.getHeight()) + ")");
                check(bar.getLocalBounds().contains(b), label + rowName + " " + row[i].name + " is outside the bar");
                for (int j = i + 1; j < count; ++j)
                    check(! b.intersects(row[j].component->getBounds()), label + rowName + " " + row[i].name + " overlaps " + row[j].name);
            }
        };

        checkRow(utility, (int) std::size(utility), "utility");
        checkRow(transport, (int) std::size(transport), "transport");
    }

    // Wide window: words on the buttons; narrow window: icons (names move to the hover text).
    bar.setBounds(0, 0, 1800, 118);
    check(! bar.projectButton.isCompact() && ! bar.audioButton.isCompact(), "a wide bar keeps the words on its buttons");
    bar.setBounds(0, 0, 700, 118);
    check(bar.projectButton.isCompact() && bar.audioButton.isCompact(), "a narrow bar shows icons");

    std::printf("%s (%d widths checked, %d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", widthsChecked, failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
