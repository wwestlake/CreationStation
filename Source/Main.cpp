#include <JuceHeader.h>
#if JUCE_WINDOWS
 #include <windows.h>
#endif
#include "Branding.h"
#include "MainComponent.h"

namespace
{
void pumpStartupPaintMessages(int milliseconds)
{
   #if JUCE_WINDOWS
    const auto endTime = juce::Time::getMillisecondCounter() + (juce::uint32) juce::jmax(1, milliseconds);
    MSG message;

    do
    {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0)
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        juce::Thread::sleep(1);
    }
    while (juce::Time::getMillisecondCounter() < endTime);
   #else
    juce::Thread::sleep(milliseconds);
   #endif
}
}

class CreativeWorkstationApplication : public juce::JUCEApplication,
                                       private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Creation Station"; }
    const juce::String getApplicationVersion() override { return "0.5.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        splashWindow.reset(new StartupSplashWindow());
        splashWindow->setProgress("Starting Creation Station...", 0.02f);
        splashWindow->setVisible(true);
        splashWindow->toFront(true);
        pumpStartupPaintMessages(40);
        juce::MessageManager::callAsync([this]
        {
            createMainWindow();
        });
    }

    void shutdown() override
    {
        stopTimer();
        splashWindow = nullptr;
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
        {
            mainWindow->confirmCloseAsync([this](bool shouldQuit)
            {
                if (shouldQuit)
                    quit();
            });
            return;
        }

        quit();
    }

private:
    class StartupSplashContent final : public juce::Component
    {
    public:
        StartupSplashContent()
        {
            logo = branding::createCreationStationLogoImage(180);
        }

        void setProgress(juce::String newStatus, float newProgress)
        {
            status = std::move(newStatus);
            progress = juce::jlimit(0.0f, 1.0f, newProgress);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.fillAll(juce::Colour(0xff090b10));
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff15253b), bounds.getX(), bounds.getY(),
                                                    juce::Colour(0xff090b10), bounds.getX(), bounds.getBottom(), false));
            g.fillRoundedRectangle(bounds.reduced(14.0f), 30.0f);

            g.setColour(juce::Colour(0xff273451));
            g.drawRoundedRectangle(bounds.reduced(14.0f), 30.0f, 1.0f);

            g.setColour(juce::Colour(0x22394a6a));
            for (int i = 0; i < 7; ++i)
            {
                auto y = 74.0f + (float) (i * 40);
                g.drawLine(320.0f, y, 676.0f, y, 1.0f);
            }

            g.drawImageWithin(logo, 58, 112, 180, 180, juce::RectanglePlacement::centred, false);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(34.0f).boldened());
            g.drawText("Creation Station", 274, 92, 380, 40, juce::Justification::left, false);

            g.setColour(juce::Colour(0xff9fb0c8));
            g.setFont(juce::Font(18.0f));
            g.drawText("Audio workstation - mixer - node graph - AI assist",
                       274, 136, 380, 28, juce::Justification::left, false);

            g.setColour(juce::Colour(0xff7fcfff));
            g.setFont(juce::Font(17.0f).boldened());
            g.drawText(status.isNotEmpty() ? status : "Loading your creative deck...",
                       274, 176, 380, 26, juce::Justification::left, false);

            auto bar = juce::Rectangle<float>(274.0f, 218.0f, 376.0f, 8.0f);
            g.setColour(juce::Colour(0x403c4a66));
            g.fillRoundedRectangle(bar, 4.0f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff56f4ff), bar.getX(), bar.getY(),
                                                    juce::Colour(0xff7fcfff), bar.getRight(), bar.getY(), false));
            g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * progress), 4.0f);

            g.setColour(juce::Colour(0xffd8e2ff));
            g.setFont(juce::Font(15.0f));
            g.drawText(juce::String(juce::roundToInt(progress * 100.0f)) + "%  -  Banked mixer - X-Touch control - VST hosting - DSP graph",
                       274, 258, 410, 24, juce::Justification::left, false);
        }

    private:
        juce::Image logo;
        juce::String status { "Starting Creation Station..." };
        float progress = 0.02f;
    };

    class StartupSplashWindow final : public juce::DocumentWindow
    {
    public:
        StartupSplashWindow()
            : juce::DocumentWindow("Creation Station",
                                   juce::Colours::transparentBlack,
                                   0)
        {
            setUsingNativeTitleBar(false);
            setDropShadowEnabled(true);
            setResizable(false, false);
            setAlwaysOnTop(true);
            content = new StartupSplashContent();
            setContentOwned(content, true);
            centreWithSize(720, 420);
        }

        void setProgress(const juce::String& statusText, float progress)
        {
            if (content != nullptr)
            {
                content->setProgress(statusText, progress);
                content->repaint();
                if (auto* peer = getPeer())
                    peer->performAnyPendingRepaintsNow();
                pumpStartupPaintMessages(28);
            }
        }

    private:
        StartupSplashContent* content = nullptr;
    };

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, MainComponent::StartupProgressCallback startupProgressCallback)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setIcon(branding::createCreationStationLogoImage(128));
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new MainComponent(std::move(startupProgressCallback)), true);
            centreWithSize(1400, 900);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void confirmCloseAsync(const std::function<void(bool shouldClose)>& onDecision)
        {
            if (auto* mainComponent = dynamic_cast<MainComponent*> (getContentComponent()))
            {
                mainComponent->confirmCloseApplication(onDecision);
                return;
            }

            if (onDecision)
                onDecision(true);
        }
    };

    void createMainWindow()
    {
        auto progressCallback = [this](const juce::String& statusText, float progress)
        {
            if (splashWindow != nullptr)
            {
                splashWindow->setProgress(statusText, progress);
            }
        };

        mainWindow.reset(new MainWindow(getApplicationName(), std::move(progressCallback)));

        if (splashWindow != nullptr)
            startTimer(650);
    }

    void timerCallback() override
    {
        stopTimer();
        splashWindow = nullptr;
    }

    std::unique_ptr<StartupSplashWindow> splashWindow;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(CreativeWorkstationApplication)
