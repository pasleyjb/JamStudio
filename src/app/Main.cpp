#include "MainWindow.h"

#include "../ui/JamStudioLookAndFeel.h"

namespace jamstudio::app
{

class JamStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "JamStudio"; }
    const juce::String getApplicationVersion() override    { return "0.9.2"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        lookAndFeel = std::make_unique<jamstudio::ui::JamStudioLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        audioDeviceManager.initialiseWithDefaultDevices (2, 2);
        mainWindow = std::make_unique<MainWindow> (audioDeviceManager);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        audioDeviceManager.closeAudioDevice();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<jamstudio::ui::JamStudioLookAndFeel> lookAndFeel;
    juce::AudioDeviceManager audioDeviceManager;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace jamstudio::app

START_JUCE_APPLICATION (jamstudio::app::JamStudioApplication)