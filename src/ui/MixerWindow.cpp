#include "MixerWindow.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

class MixerWindow::Content : public juce::Component,
                             private juce::Timer
{
public:
    explicit Content (jamstudio::audio::TransportController& transport)
        : transportController (transport)
    {
        playButton.onClick = [this]
        {
            transportController.play();
            updateTransportIndicators();
        };
        pauseButton.onClick = [this]
        {
            transportController.pause();
            updateTransportIndicators();
        };
        stopButton.onClick = [this]
        {
            transportController.stop();
            updateTransportIndicators();
        };

        addAndMakeVisible (playButton);
        addAndMakeVisible (pauseButton);
        addAndMakeVisible (stopButton);
        addAndMakeVisible (remoteLabel);
        remoteLabel.setText ("TRANSPORT", juce::dontSendNotification);
        remoteLabel.setJustificationType (juce::Justification::centred);
        remoteLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        viewport.setViewedComponent (&stripContainer, false);
        viewport.setScrollBarsShown (false, false);
        addAndMakeVisible (viewport);

        emptyLabel.setText ("Load a song or separate stems\nto mix channels here.",
                            juce::dontSendNotification);
        emptyLabel.setJustificationType (juce::Justification::centred);
        emptyLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (emptyLabel);

        startTimerHz (20);
    }

    void rebuild (jamstudio::audio::StemMixer& mixer, MixerChannelStrip::StemChangedCallback onChanged)
    {
        strips.clear();
        stripContainer.removeAllChildren();

        for (int i = 0; i < mixer.getNumStems(); ++i)
        {
            if (mixer.getStem (i) != nullptr)
            {
                auto strip = std::make_unique<MixerChannelStrip> (i, mixer, onChanged);
                stripContainer.addAndMakeVisible (strip.get());
                strips.add (std::move (strip));
            }
        }

        emptyLabel.setVisible (strips.isEmpty());
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = JamStudioTheme::getColours();
        g.fillAll (colours.windowBackground);

        // Remote strip background
        auto remoteArea = getLocalBounds().removeFromTop (54).toFloat().reduced (6.0f, 4.0f);
        g.setColour (colours.panelBackground.brighter (0.03f));
        g.fillRoundedRectangle (remoteArea, 8.0f);
        g.setColour (colours.border);
        g.drawRoundedRectangle (remoteArea, 8.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (8);

        auto remote = bounds.removeFromTop (46);
        remoteLabel.setBounds (remote.removeFromLeft (84));
        const auto deck = juce::jmin (remote.getHeight(), 40);
        playButton.setBounds (remote.removeFromLeft (deck + 4).withSizeKeepingCentre (deck, deck));
        pauseButton.setBounds (remote.removeFromLeft (deck + 4).withSizeKeepingCentre (deck, deck));
        stopButton.setBounds (remote.removeFromLeft (deck + 4).withSizeKeepingCentre (deck, deck));

        bounds.removeFromTop (6);
        emptyLabel.setBounds (bounds);
        viewport.setBounds (bounds);

        const auto area = viewport.getLocalBounds();
        const int count = juce::jmax (1, strips.size());
        stripContainer.setBounds (area);

        const auto stripWidth = area.getWidth() / count;
        auto row = area;

        for (auto* strip : strips)
        {
            if (strip != nullptr)
                strip->setBounds (row.removeFromLeft (stripWidth).reduced (2, 0));
        }
    }

private:
    void timerCallback() override
    {
        updateTransportIndicators();
    }

    void updateTransportIndicators()
    {
        const auto playing = transportController.isPlaying();
        playButton.setActive (playing);
        pauseButton.setActive (! playing && transportController.getPosition() > 0.0);
        stopButton.setActive (! playing && transportController.getPosition() <= 0.001);
    }

    jamstudio::audio::TransportController& transportController;
    juce::Label remoteLabel;
    TapeDeckButton playButton { "mixerPlay", TapeDeckButton::Icon::play };
    TapeDeckButton pauseButton { "mixerPause", TapeDeckButton::Icon::pause };
    TapeDeckButton stopButton { "mixerStop", TapeDeckButton::Icon::stop };

    juce::Viewport viewport;
    juce::Component stripContainer;
    juce::OwnedArray<MixerChannelStrip> strips;
    juce::Label emptyLabel;
};

MixerWindow::MixerWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("Mixer",
                      JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::closeButton),
      transportController (transport)
{
    juce::ignoreUnused (transportController);
    setUsingNativeTitleBar (true);
    content = std::make_unique<Content> (transport);
    setContentNonOwned (content.get(), true);
    setResizable (true, true);
    setResizeLimits (360, 320, 2400, 900);
    centreWithSize (680, 460);
    setVisible (false);
}

MixerWindow::~MixerWindow()
{
    setContentNonOwned (nullptr, false);
    content.reset();
}

void MixerWindow::rebuild (jamstudio::audio::StemMixer& mixer, StemChangedCallback onChanged)
{
    if (content != nullptr)
        content->rebuild (mixer, std::move (onChanged));
}

void MixerWindow::closeButtonPressed()
{
    setVisible (false);

    if (visibilityChanged != nullptr)
        visibilityChanged (false);
}

void MixerWindow::showMixer (const bool shouldShow)
{
    setVisible (shouldShow);

    if (shouldShow)
        toFront (true);

    if (visibilityChanged != nullptr)
        visibilityChanged (shouldShow);
}

void MixerWindow::setVisibilityChangedCallback (std::function<void (bool visible)> callback)
{
    visibilityChanged = std::move (callback);
}

} // namespace jamstudio::ui
