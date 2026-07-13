#include <JuceHeader.h>
#include "../Branding.h"
#include "RouterMainComponent.h"

class DjehutiRouterApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Djehuti Router"; }
    const juce::String getApplicationVersion() override { return "0.1.2"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setIcon(branding::createDjehutiRouterLogoImage(128));
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new RouterMainComponent(), true);
            centreWithSize(1380, 860);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(DjehutiRouterApplication)
