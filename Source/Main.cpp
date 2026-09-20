#include <JuceHeader.h>
#include "MainComponent.h"
#include <creation/ui/CreationSuiteLogos.h>
#include <creation/ui/SuiteJUCEApplication.h>

class CreativeWorkstationApplication : public creation::ui::SuiteJUCEApplication
{
public:
    CreativeWorkstationApplication() : SuiteJUCEApplication(creation::ui::SuiteLogoId::station)
    {
    }

    const juce::String getApplicationName() override { return "Djehuti Station"; }
    const juce::String getApplicationVersion() override { return "0.5.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void systemRequestedQuit() override
    {
        // getMainWindow() (from the shared base class) reads nullptr once
        // shutdown() has run, so this stays safe without this class needing
        // to keep and separately null its own copy of the pointer.
        if (auto* window = dynamic_cast<MainWindow*>(getMainWindow()))
        {
            window->confirmCloseAsync([this](bool shouldQuit)
            {
                if (shouldQuit)
                    quit();
            });
            return;
        }

        quit();
    }

protected:
    std::unique_ptr<juce::DocumentWindow> createMainWindow() override
    {
        return std::make_unique<MainWindow>(getApplicationName(),
            [this](const juce::String& statusText, float progress) { reportSplashProgress(statusText, progress); });
    }

private:
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
            setResizeLimits(920, 560, 10000, 10000); // below this even the icon-only header would not fit
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
};

START_JUCE_APPLICATION(CreativeWorkstationApplication)
