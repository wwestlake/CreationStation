#include <JuceHeader.h>
#include "Branding.h"
#include "MainComponent.h"

class CreativeWorkstationApplication : public juce::JUCEApplication,
                                       private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Creation Station"; }
    const juce::String getApplicationVersion() override { return "0.1.2"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        splash = new juce::SplashScreen(getApplicationName(), branding::createCreationStationSplashImage(), true);
        splash->toFront(true);
        startTimer(1800);
    }

    void shutdown() override
    {
        stopTimer();
        splash = nullptr;
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setIcon(branding::createCreationStationLogoImage(128));
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1400, 900);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    void timerCallback() override
    {
        stopTimer();
        mainWindow.reset(new MainWindow(getApplicationName()));

        if (splash != nullptr)
            splash->deleteAfterDelay(juce::RelativeTime::seconds(2.0), false);

        splash = nullptr;
    }

    juce::SplashScreen* splash = nullptr;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(CreativeWorkstationApplication)
