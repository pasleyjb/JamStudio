#include "MainWindow.h"

namespace jamstudio::app
{

class JamStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "JamStudio"; }
    const juce::String getApplicationVersion() override    { return "0.6.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        audioDeviceManager.initialiseWithDefaultDevices (2, 2);
        mainWindow = std::make_unique<MainWindow> (audioDeviceManager);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        audioDeviceManager.closeAudioDevice();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    juce::AudioDeviceManager audioDeviceManager;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace jamstudio::app

START_JUCE_APPLICATION (jamstudio::app::JamStudioApplication)