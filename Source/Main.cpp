#include <JuceHeader.h>
#if JUCE_WINDOWS
 #include <windows.h>
#endif
#include "MainComponent.h"
#include <creation/ui/CreationSuiteLogos.h>
#include <creation/ui/SuiteCommonSpacePanel.h>

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
            commonSpacePanel.setMode(creation::ui::SuiteCommonSpacePanel::Mode::splash);
            commonSpacePanel.setSelectedLogoId(creation::ui::SuiteLogoId::station);
            commonSpacePanel.setFooterText("Creation Station loading inside the shared Creation Suite surface.");
            addAndMakeVisible(commonSpacePanel);
        }

        void setProgress(juce::String newStatus, float newProgress)
        {
            status = std::move(newStatus);
            progress = juce::jlimit(0.0f, 1.0f, newProgress);
            commonSpacePanel.setStatusText(status);
            commonSpacePanel.setProgress(progress);
        }

        void resized() override
        {
            commonSpacePanel.setBounds(getLocalBounds());
        }

    private:
        juce::String status { "Starting Creation Station..." };
        float progress = 0.02f;
        creation::ui::SuiteCommonSpacePanel commonSpacePanel;
    };

    class StartupSplashWindow final : public juce::DocumentWindow
    {
    public:
        StartupSplashWindow()
            : juce::DocumentWindow("Creation Station",
                                   juce::Colour(0xff0b0f14),
                                   0)
        {
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);
            setDropShadowEnabled(true);
            setResizable(false, false);
            setAlwaysOnTop(true);
            setOpaque(true);
            setBackgroundColour(juce::Colour(0xff0b0f14));
            content = new StartupSplashContent();
            setContentOwned(content, true);
            centreWithSize(1040, 620);
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
            setIcon(creation::ui::getSuiteLogoImage(creation::ui::SuiteLogoId::station));
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
