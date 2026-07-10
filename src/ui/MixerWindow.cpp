#include "MixerWindow.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** Permanent mixer strip for stage media — transport + VIDEO sound fader. */
class VideoSoundStrip : public juce::Component,
                        private juce::Timer
{
public:
    explicit VideoSoundStrip (jamstudio::audio::StageMediaPlayer& player)
        : stageMedia (player)
    {
        nameLabel.setText ("VIDEO", juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (nameLabel);

        playButton.onClick = [this]
        {
            stageMedia.play();
            updateTransport();
        };
        pauseButton.onClick = [this]
        {
            stageMedia.pause();
            updateTransport();
        };
        stopButton.onClick = [this]
        {
            stageMedia.stop();
            updateTransport();
        };
        addAndMakeVisible (playButton);
        addAndMakeVisible (pauseButton);
        addAndMakeVisible (stopButton);

        volumeSlider.setRange (0.0, 1.0, 0.01);
        volumeSlider.setSliderSnapsToMousePosition (true);
        volumeSlider.setValue (stageMedia.getVolume(), juce::dontSendNotification);
        volumeSlider.onValueChange = [this]
        {
            stageMedia.setVolume (static_cast<float> (volumeSlider.getValue()));
            levelLabel.setText (juce::String (static_cast<int> (volumeSlider.getValue() * 100)),
                                juce::dontSendNotification);
        };
        addAndMakeVisible (volumeSlider);

        levelLabel.setJustificationType (juce::Justification::centred);
        levelLabel.setFont (juce::FontOptions (10.0f));
        levelLabel.setText (juce::String (static_cast<int> (stageMedia.getVolume() * 100)),
                            juce::dontSendNotification);
        addAndMakeVisible (levelLabel);

        posLabel.setJustificationType (juce::Justification::centred);
        posLabel.setFont (juce::FontOptions (9.0f));
        posLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (posLabel);

        startTimerHz (20);
    }

    void syncFromPlayer()
    {
        volumeSlider.setValue (stageMedia.getVolume(), juce::dontSendNotification);
        levelLabel.setText (juce::String (static_cast<int> (stageMedia.getVolume() * 100)),
                            juce::dontSendNotification);
        updateTransport();
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = JamStudioTheme::getColours();
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours.panelBackground.brighter (0.06f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xff9b59f5));
        g.fillRoundedRectangle (bounds.removeFromTop (4.0f).reduced (4.0f, 0.0f), 2.0f);
        g.setColour (colours.border);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.0f);

        auto meter = meterBounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (meter, 2.0f);
        const auto lvl = stageMedia.getMeterLevel();
        g.setColour (lvl > 0.8f ? juce::Colours::red : juce::Colour (0xff9b59f5));
        const auto fillH = meter.getHeight() * lvl;
        g.fillRect (meter.getX(), meter.getBottom() - fillH, meter.getWidth(), fillH);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);
        nameLabel.setBounds (bounds.removeFromTop (18));

        auto deck = bounds.removeFromTop (28);
        const auto b = juce::jmin (24, deck.getHeight());
        const auto total = b * 3 + 6;
        auto deckRow = juce::Rectangle<int> (total, b).withCentre (deck.getCentre());
        playButton.setBounds (deckRow.removeFromLeft (b));
        deckRow.removeFromLeft (3);
        pauseButton.setBounds (deckRow.removeFromLeft (b));
        deckRow.removeFromLeft (3);
        stopButton.setBounds (deckRow.removeFromLeft (b));

        posLabel.setBounds (bounds.removeFromTop (14));
        levelLabel.setBounds (bounds.removeFromBottom (16));
        meterBounds = bounds.removeFromRight (10).reduced (1, 4);
        bounds.removeFromRight (2);
        volumeSlider.setBounds (bounds.reduced (2, 2));
    }

private:
    void timerCallback() override
    {
        updateTransport();
        repaint (meterBounds.expanded (1));
    }

    void updateTransport()
    {
        const auto playing = stageMedia.isPlaying();
        playButton.setActive (playing);
        pauseButton.setActive (! playing && stageMedia.getPosition() > 0.05);
        stopButton.setActive (! playing && stageMedia.getPosition() <= 0.05);

        auto fmt = [] (double s)
        {
            const auto t = juce::jmax (0, juce::roundToInt (s));
            return juce::String::formatted ("%d:%02d", t / 60, t % 60);
        };

        if (stageMedia.hasMedia())
            posLabel.setText (fmt (stageMedia.getPosition()) + "/" + fmt (stageMedia.getLengthInSeconds()),
                              juce::dontSendNotification);
        else
            posLabel.setText ("--:--", juce::dontSendNotification);
    }

    jamstudio::audio::StageMediaPlayer& stageMedia;
    juce::Label nameLabel;
    TapeDeckButton playButton { "videoPlay", TapeDeckButton::Icon::play };
    TapeDeckButton pauseButton { "videoPause", TapeDeckButton::Icon::pause };
    TapeDeckButton stopButton { "videoStop", TapeDeckButton::Icon::stop };
    juce::Slider volumeSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label levelLabel;
    juce::Label posLabel;
    juce::Rectangle<int> meterBounds;
};


/** Masters for FOH / Mon A / Mon B + click & stage routing hints. */
class BusMasterStrip : public juce::Component
{
public:
    explicit BusMasterStrip (jamstudio::audio::TransportController& transport)
        : transportController (transport)
    {
        title.setText ("BUSES", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (title);

        auto setup = [this] (juce::Slider& s, juce::Label& lab, jamstudio::audio::MixBus bus)
        {
            lab.setText (jamstudio::audio::mixBusName (bus), juce::dontSendNotification);
            lab.setJustificationType (juce::Justification::centred);
            lab.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            lab.setColour (juce::Label::textColourId, jamstudio::audio::mixBusColour (bus));
            addAndMakeVisible (lab);

            s.setRange (0.0, 1.25, 0.01);
            s.setValue (transportController.getStemMixer().getBusMaster (bus), juce::dontSendNotification);
            s.setSliderSnapsToMousePosition (true);
            s.setColour (juce::Slider::thumbColourId, jamstudio::audio::mixBusColour (bus));
            s.setTooltip (jamstudio::audio::mixBusLongName (bus) + " master");
            s.onValueChange = [this, bus, &s]
            {
                transportController.getStemMixer().setBusMaster (
                    bus, static_cast<float> (s.getValue()));
            };
            addAndMakeVisible (s);
        };

        setup (fohSlider, fohLabel, jamstudio::audio::MixBus::foh);
        setup (monASlider, monALabel, jamstudio::audio::MixBus::monitorA);
        setup (monBSlider, monBLabel, jamstudio::audio::MixBus::monitorB);

        routeHint.setText ("Out 1-2 FOH\\n3-4 Mon A\\n5-6 Mon B\\nClick->Mon",
                           juce::dontSendNotification);
        routeHint.setJustificationType (juce::Justification::centredTop);
        routeHint.setFont (juce::FontOptions (9.0f));
        routeHint.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (routeHint);

        // Click sends (quick toggles)
        clickFoh.setButtonText ("Clk FOH");
        clickMon.setButtonText ("Clk Mon");
        clickFoh.setClickingTogglesState (true);
        clickMon.setClickingTogglesState (true);
        clickFoh.setToggleState (transportController.getMultiBusMaster().getClickBusSend (
                                     jamstudio::audio::MixBus::foh) > 0.1f,
                                 juce::dontSendNotification);
        clickMon.setToggleState (transportController.getMultiBusMaster().getClickBusSend (
                                     jamstudio::audio::MixBus::monitorA) > 0.1f,
                                 juce::dontSendNotification);
        clickFoh.onClick = [this]
        {
            transportController.getMultiBusMaster().setClickBusSend (
                jamstudio::audio::MixBus::foh, clickFoh.getToggleState() ? 0.7f : 0.0f);
        };
        clickMon.onClick = [this]
        {
            const float g = clickMon.getToggleState() ? 0.85f : 0.0f;
            transportController.getMultiBusMaster().setClickBusSend (jamstudio::audio::MixBus::monitorA, g);
            transportController.getMultiBusMaster().setClickBusSend (jamstudio::audio::MixBus::monitorB, g * 0.65f);
        };
        addAndMakeVisible (clickFoh);
        addAndMakeVisible (clickMon);
    }

    void syncFromMixer()
    {
        auto& m = transportController.getStemMixer();
        fohSlider.setValue (m.getBusMaster (jamstudio::audio::MixBus::foh), juce::dontSendNotification);
        monASlider.setValue (m.getBusMaster (jamstudio::audio::MixBus::monitorA), juce::dontSendNotification);
        monBSlider.setValue (m.getBusMaster (jamstudio::audio::MixBus::monitorB), juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = JamStudioTheme::getColours();
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours.panelBackground.brighter (0.05f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (colours.border);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (4);
        title.setBounds (a.removeFromTop (18));
        a.removeFromTop (2);
        auto clicks = a.removeFromBottom (48);
        clickFoh.setBounds (clicks.removeFromTop (22).reduced (1));
        clickMon.setBounds (clicks.reduced (1));
        a.removeFromBottom (4);
        routeHint.setBounds (a.removeFromBottom (52));
        a.removeFromBottom (4);

        const auto labH = 14;
        auto col = a;
        const auto w = col.getWidth() / 3;
        auto c0 = col.removeFromLeft (w);
        auto c1 = col.removeFromLeft (w);
        auto c2 = col;
        fohLabel.setBounds (c0.removeFromTop (labH));
        fohSlider.setBounds (c0.reduced (1));
        monALabel.setBounds (c1.removeFromTop (labH));
        monASlider.setBounds (c1.reduced (1));
        monBLabel.setBounds (c2.removeFromTop (labH));
        monBSlider.setBounds (c2.reduced (1));
    }

private:
    jamstudio::audio::TransportController& transportController;
    juce::Label title;
    juce::Label fohLabel, monALabel, monBLabel;
    juce::Slider fohSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider monASlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider monBSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label routeHint;
    juce::ToggleButton clickFoh, clickMon;
};

class MixerWindow::Content : public juce::Component,
                             private juce::Timer
{
public:
    explicit Content (jamstudio::audio::TransportController& transport)
        : transportController (transport),
          videoStrip (transport.getStageMedia()),
          busStrip (transport)
    {
        // Main transport: setlist stems + stage video (linked).
        skipBackButton.onClick = [this]
        {
            transportController.skipBack (5.0);
            updateTransportIndicators();
        };
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
        skipForwardButton.onClick = [this]
        {
            transportController.skipForward (5.0);
            updateTransportIndicators();
        };

        countInButton.setClickingTogglesState (true);
        countInButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
        countInButton.setToggleState (transportController.isCountInEnabled(), juce::dontSendNotification);
        countInButton.setTooltip ("4-count intro before play (all modes)");
        countInButton.onClick = [this]
        {
            transportController.setCountInEnabled (countInButton.getToggleState());
            updateCountInIndicator();
        };

        linkVideoButton.setClickingTogglesState (true);
        linkVideoButton.setIndicatorColour (juce::Colour (0xff9b59f5));
        linkVideoButton.setToggleState (transportController.isStageMediaLinked(), juce::dontSendNotification);
        linkVideoButton.setTooltip ("Main transport also controls stage video");
        linkVideoButton.onClick = [this]
        {
            transportController.setLinkStageMedia (linkVideoButton.getToggleState());
            updateLinkIndicator();
        };

        saveSetMixButton.setButtonText ("Save Mix -> Set Track");
        saveSetMixButton.setTooltip ("Save FOH/Mon/mute/solo for the current setlist song (Performance mode)");
        saveSetMixButton.setEnabled (false);
        saveSetMixButton.onClick = [this]
        {
            if (saveSetlistMix)
                saveSetlistMix();
        };
        addAndMakeVisible (saveSetMixButton);

        setTrackLabel.setText ("No set track", juce::dontSendNotification);
        setTrackLabel.setJustificationType (juce::Justification::centredLeft);
        setTrackLabel.setFont (juce::FontOptions (11.0f));
        setTrackLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (setTrackLabel);

        addAndMakeVisible (skipBackButton);
        addAndMakeVisible (playButton);
        addAndMakeVisible (pauseButton);
        addAndMakeVisible (stopButton);
        addAndMakeVisible (skipForwardButton);
        addAndMakeVisible (countInButton);
        addAndMakeVisible (linkVideoButton);
        addAndMakeVisible (remoteLabel);
        remoteLabel.setText ("MAIN", juce::dontSendNotification);
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

        addAndMakeVisible (videoStrip);
        addAndMakeVisible (busStrip);

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

    void syncFromMixer (const jamstudio::audio::StemMixer& mixer)
    {
        if (strips.size() != mixer.getNumStems())
            return;

        for (int i = 0; i < strips.size(); ++i)
            if (auto* stem = mixer.getStem (i))
                if (auto* strip = strips[i])
                    strip->syncFromTrack (*stem);

        videoStrip.syncFromPlayer();
        busStrip.syncFromMixer();
    }

    void syncVideoSoundSlider()
    {
        videoStrip.syncFromPlayer();
        busStrip.syncFromMixer();
    }

    void setSaveSetlistMixCallback (std::function<void()> cb)
    {
        saveSetlistMix = std::move (cb);
    }

    void setPerformanceMixContext (const bool active,
                                   const int songIndex,
                                   const int songCount,
                                   const juce::String& songName)
    {
        const bool canSave = active && songIndex >= 0 && songCount > 0;
        saveSetMixButton.setEnabled (canSave);

        if (! active)
            setTrackLabel.setText ("No set loaded", juce::dontSendNotification);
        else if (songIndex < 0)
            setTrackLabel.setText ("Set ready — start a song to save mix", juce::dontSendNotification);
        else
            setTrackLabel.setText ("Set track " + juce::String (songIndex + 1) + "/"
                                       + juce::String (songCount) + ": " + songName,
                                   juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = JamStudioTheme::getColours();
        g.fillAll (colours.windowBackground);

        // Remote strip background
        auto remoteArea = getLocalBounds().removeFromTop (78).toFloat().reduced (6.0f, 4.0f);
        g.setColour (colours.panelBackground.brighter (0.03f));
        g.fillRoundedRectangle (remoteArea, 8.0f);
        g.setColour (colours.border);
        g.drawRoundedRectangle (remoteArea, 8.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (8);

        // Top: save mix to current setlist track
        auto saveRow = bounds.removeFromTop (28);
        saveSetMixButton.setBounds (saveRow.removeFromLeft (160).reduced (1));
        saveRow.removeFromLeft (8);
        setTrackLabel.setBounds (saveRow);
        bounds.removeFromTop (4);

        auto remote = bounds.removeFromTop (46);
        remoteLabel.setBounds (remote.removeFromLeft (52));
        const auto deck = juce::jmin (remote.getHeight(), 40);
        skipBackButton.setBounds (remote.removeFromLeft (deck + 2).withSizeKeepingCentre (deck, deck));
        playButton.setBounds (remote.removeFromLeft (deck + 2).withSizeKeepingCentre (deck, deck));
        pauseButton.setBounds (remote.removeFromLeft (deck + 2).withSizeKeepingCentre (deck, deck));
        stopButton.setBounds (remote.removeFromLeft (deck + 2).withSizeKeepingCentre (deck, deck));
        skipForwardButton.setBounds (remote.removeFromLeft (deck + 2).withSizeKeepingCentre (deck, deck));
        remote.removeFromLeft (6);
        countInButton.setBounds (remote.removeFromLeft (56).reduced (1));
        linkVideoButton.setBounds (remote.removeFromLeft (64).reduced (1));

        bounds.removeFromTop (6);

        // Bus masters + VIDEO on the right.
        const auto videoW = juce::jlimit (72, 96, bounds.getWidth() / 8);
        const auto busW = juce::jlimit (96, 130, bounds.getWidth() / 6);
        videoStrip.setBounds (bounds.removeFromRight (videoW));
        bounds.removeFromRight (4);
        busStrip.setBounds (bounds.removeFromRight (busW));
        bounds.removeFromRight (6);

        emptyLabel.setBounds (bounds);
        viewport.setBounds (bounds);

        const auto area = viewport.getLocalBounds();
        const int count = juce::jmax (1, strips.size());
        // Wider strips for FOH + Mon A + Mon B send columns.
        const int minStripW = 96;
        const auto stripWidth = juce::jmax (minStripW, area.getWidth() / count);
        stripContainer.setBounds (0, 0, stripWidth * count, area.getHeight());
        viewport.setViewedComponent (&stripContainer, false);

        auto row = stripContainer.getLocalBounds();
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
        updateCountInIndicator();
        updateLinkIndicator();
    }

    void updateTransportIndicators()
    {
        const auto playing = transportController.isPlaying()
                             || transportController.getStageMedia().isPlaying();
        const auto counting = transportController.isCountingIn();
        playButton.setActive (playing || counting);
        pauseButton.setActive (! playing && ! counting && transportController.getPosition() > 0.0);
        stopButton.setActive (! playing && ! counting && transportController.getPosition() <= 0.001);
    }

    void updateCountInIndicator()
    {
        const auto enabled = transportController.isCountInEnabled();
        countInButton.setToggleState (enabled, juce::dontSendNotification);

        auto blink = false;
        if (transportController.isCountingIn())
        {
            const auto bpm = transportController.getMetronome().getBpm();
            const auto phase = std::fmod (juce::Time::getMillisecondCounterHiRes() * bpm / 60000.0, 1.0);
            blink = phase < 0.35;
        }

        countInButton.setIndicatorActive (enabled, blink);
    }

    void updateLinkIndicator()
    {
        const auto linked = transportController.isStageMediaLinked();
        linkVideoButton.setToggleState (linked, juce::dontSendNotification);
        linkVideoButton.setIndicatorActive (linked, false);
    }

    jamstudio::audio::TransportController& transportController;
    juce::TextButton saveSetMixButton;
    juce::Label setTrackLabel;
    juce::Label remoteLabel;
    TapeDeckButton skipBackButton { "mixerSkipBack", TapeDeckButton::Icon::skipBack };
    TapeDeckButton playButton { "mixerPlay", TapeDeckButton::Icon::play };
    TapeDeckButton pauseButton { "mixerPause", TapeDeckButton::Icon::pause };
    TapeDeckButton stopButton { "mixerStop", TapeDeckButton::Icon::stop };
    TapeDeckButton skipForwardButton { "mixerSkipFwd", TapeDeckButton::Icon::skipForward };
    IndicatorButton countInButton { "mixerCountIn", "4-IN" };
    IndicatorButton linkVideoButton { "mixerLinkVideo", "LINK" };
    std::function<void()> saveSetlistMix;

    juce::Viewport viewport;
    juce::Component stripContainer;
    juce::OwnedArray<MixerChannelStrip> strips;
    juce::Label emptyLabel;
    VideoSoundStrip videoStrip;
    BusMasterStrip busStrip;
};

MixerWindow::MixerWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("Mixer",
                      JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::minimiseButton
                          | DocumentWindow::maximiseButton
                          | DocumentWindow::closeButton),
      transportController (transport)
{
    juce::ignoreUnused (transportController);

    // Keep off-screen until showMixer(true). Default visible=true would flash a peer.
    windowOpen = false;
    setVisible (false);
    setWantsKeyboardFocus (false);

    // Non-native chrome so min / max / close always hit our button handlers (Linux WMs
    // often swallow native title-bar clicks on secondary DocumentWindows).
    setUsingNativeTitleBar (false);
    content = std::make_unique<Content> (transport);
    setContentNonOwned (content.get(), false);
    setResizable (true, true);
    setResizeLimits (520, 360, 2800, 1200);
    setSize (920, 520);
    restoredBounds = getBounds();

    attachButton.setTooltip ("Stick Stage FX Controller to mixer (<>)");
    detachButton.setTooltip ("Unstick floating windows (><)");
    attachButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    detachButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    attachButton.onClick = [this]
    {
        if (dockAttach)
            dockAttach();
    };
    detachButton.onClick = [this]
    {
        if (dockDetach)
            dockDetach();
    };
    // ResizableWindow shadows Component& overload — pass pointers.
    addAndMakeVisible (&attachButton);
    addAndMakeVisible (&detachButton);
    setDockStickyState (true);

    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

MixerWindow::~MixerWindow()
{
    hideMixer();
    setContentNonOwned (nullptr, false);
    content.reset();
}

void MixerWindow::rebuild (jamstudio::audio::StemMixer& mixer, StemChangedCallback onChanged)
{
    if (content != nullptr)
        content->rebuild (mixer, std::move (onChanged));
}

void MixerWindow::syncFromMixer (const jamstudio::audio::StemMixer& mixer)
{
    if (content != nullptr)
        content->syncFromMixer (mixer);
}

void MixerWindow::syncVideoSoundSlider()
{
    if (content != nullptr)
        content->syncVideoSoundSlider();
}

void MixerWindow::setSaveSetlistMixCallback (std::function<void()> onSave)
{
    if (content != nullptr)
        content->setSaveSetlistMixCallback (std::move (onSave));
}

void MixerWindow::setPerformanceMixContext (const bool performanceActive,
                                            const int songIndex,
                                            const int songCount,
                                            const juce::String& songName)
{
    if (content != nullptr)
        content->setPerformanceMixContext (performanceActive, songIndex, songCount, songName);
}

void MixerWindow::closeButtonPressed()
{
    hideMixer();
}

void MixerWindow::userTriedToCloseWindow()
{
    hideMixer();
}

void MixerWindow::minimiseButtonPressed()
{
    // Avoid WM minimise which often strands secondary DocumentWindows on Linux.
    // Collapse to a compact floating bar instead of iconifying.
    if (! maximised && getWidth() > 0 && getHeight() > 0)
        restoredBounds = getBounds();

    maximised = false;
    setFullScreen (false);
    setMinimised (false);
    setBounds (restoredBounds.withHeight (56).withWidth (juce::jmax (360, restoredBounds.getWidth())));
}

juce::Rectangle<int> MixerWindow::getMaximiseBounds() const
{
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getBounds()))
        return display->userBounds.getSmallestIntegerContainer().reduced (8);

    return { 40, 40, 1200, 800 };
}

void MixerWindow::maximiseButtonPressed()
{
    // Manual max/restore — setFullScreen on non-native DocumentWindow is unreliable on Linux.
    if (maximised)
    {
        maximised = false;
        setFullScreen (false);
        setMinimised (false);
        if (! restoredBounds.isEmpty())
            setBounds (restoredBounds);
        else
            centreWithSize (920, 520);
        return;
    }

    if (getWidth() > 0 && getHeight() > 0)
        restoredBounds = getBounds();

    maximised = true;
    setMinimised (false);
    setFullScreen (false);
    setBounds (getMaximiseBounds());
    toFront (true);

    if (dockMaximised)
        dockMaximised();
}

void MixerWindow::setDockCallbacks (std::function<void()> onAttach,
                                    std::function<void()> onDetach,
                                    std::function<void()> onMaximised)
{
    dockAttach = std::move (onAttach);
    dockDetach = std::move (onDetach);
    dockMaximised = std::move (onMaximised);
}

void MixerWindow::setDockStickyState (const bool sticky)
{
    // Sticky: show detach >< ; Detached: show attach <>
    attachButton.setVisible (! sticky);
    detachButton.setVisible (sticky);
    attachButton.setEnabled (! sticky);
    detachButton.setEnabled (sticky);
    layoutDockButtons();
}

void MixerWindow::layoutDockButtons()
{
    const auto titleH = getTitleBarHeight();
    // Leave room for min/max/close (~3 * titleH) on the right.
    const auto reserve = titleH * 3 + 8;
    auto r = juce::Rectangle<int> (getWidth() - reserve - 72, 2, 68, juce::jmax (18, titleH - 4));
    if (detachButton.isVisible())
        detachButton.setBounds (r.removeFromRight (32));
    if (attachButton.isVisible())
    {
        if (detachButton.isVisible())
            r.removeFromRight (4);
        attachButton.setBounds (r.removeFromRight (32));
    }
}

void MixerWindow::resized()
{
    DocumentWindow::resized();
    layoutDockButtons();
}

void MixerWindow::hideMixer()
{
    const auto wasOpen = windowOpen || isVisible() || isOnDesktop();

    if (! maximised && ! isMinimised() && getWidth() > 0 && getHeight() > 0)
        restoredBounds = getBounds();

    windowOpen = false;
    maximised = false;
    setMinimised (false);
    setFullScreen (false);
    setVisible (false);

    if (isOnDesktop())
        removeFromDesktop();

    if (wasOpen && visibilityChanged != nullptr)
        visibilityChanged (false);
}

void MixerWindow::showMixer (const bool shouldShow)
{
    if (! shouldShow)
    {
        hideMixer();
        return;
    }

    maximised = false;
    setFullScreen (false);
    setMinimised (false);

    if (! restoredBounds.isEmpty())
        setBounds (restoredBounds);
    else
        centreWithSize (920, 520);

    if (! isOnDesktop())
        addToDesktop (getDesktopWindowStyleFlags());

    setVisible (true);
    toFront (true);
    windowOpen = true;

    if (visibilityChanged != nullptr)
        visibilityChanged (true);
}

void MixerWindow::setVisibilityChangedCallback (std::function<void (bool visible)> callback)
{
    visibilityChanged = std::move (callback);
}

} // namespace jamstudio::ui
