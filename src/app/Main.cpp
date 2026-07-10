#include "MainWindow.h"

#include "../ui/BrandAssets.h"
#include "../ui/JamStudioLookAndFeel.h"

namespace jamstudio::app
{

class JamStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "JamStudio"; }
    const juce::String getApplicationVersion() override    { return "0.9.6"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        lookAndFeel = std::make_unique<jamstudio::ui::JamStudioLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        // Startup splash - full-bleed brand/background art, then main window.
        auto splashImage = jamstudio::ui::BrandAssets::loadWizardBackground();

        if (! splashImage.isValid())
            splashImage = jamstudio::ui::BrandAssets::loadSplashImage();

        if (splashImage.isValid())
        {
            // Size the splash to a readable landscape window (~ half the image).
            const auto maxW = 720;
            const auto maxH = 480;
            const auto scale = juce::jmin (static_cast<float> (maxW) / static_cast<float> (splashImage.getWidth()),
                                           static_cast<float> (maxH) / static_cast<float> (splashImage.getHeight()),
                                           1.0f);
            const auto w = juce::jmax (320, juce::roundToInt (splashImage.getWidth() * scale));
            const auto h = juce::jmax (240, juce::roundToInt (splashImage.getHeight() * scale));

            auto displayImg = splashImage.rescaled (w, h, juce::Graphics::highResamplingQuality);

            // Optional brand mark in the corner of the splash.
            if (const auto logo = jamstudio::ui::BrandAssets::loadSplashImage(); logo.isValid())
            {
                juce::Graphics g (displayImg);
                const auto logoH = juce::jlimit (48, 96, h / 5);
                const auto logoScale = static_cast<float> (logoH) / static_cast<float> (logo.getHeight());
                const auto logoW = juce::roundToInt (static_cast<float> (logo.getWidth()) * logoScale);
                g.setOpacity (0.95f);
                g.drawImage (logo, 16, 16, logoW, logoH,
                             0, 0, logo.getWidth(), logo.getHeight());
            }

            splash = std::make_unique<juce::SplashScreen> ("JamStudio", displayImg, true);
        }

        // Prefer multi-channel outputs for FOH + band monitors (falls back if unavailable).
        audioDeviceManager.initialiseWithDefaultDevices (0, 6);

        {
            auto setup = audioDeviceManager.getAudioDeviceSetup();
            // Request first 6 output channels when the interface supports them.
            setup.useDefaultOutputChannels = false;
            setup.outputChannels.clear();
            const int maxOut = setup.outputDeviceName.isNotEmpty() ? 6 : 2;
            for (int c = 0; c < maxOut; ++c)
                setup.outputChannels.setBit (c);
            juce::String err;
            audioDeviceManager.setAudioDeviceSetup (setup, true);
            juce::ignoreUnused (err);
        }

        mainWindow = std::make_unique<MainWindow> (audioDeviceManager);

        if (splash != nullptr)
            splash->deleteAfterDelay (juce::RelativeTime::seconds (1.8), false);
    }

    void shutdown() override
    {
        splash = nullptr;
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
    std::unique_ptr<juce::SplashScreen> splash;
};

} // namespace jamstudio::app

START_JUCE_APPLICATION (jamstudio::app::JamStudioApplication)
