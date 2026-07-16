#include "MixerWindow.h"

#include "JamStudioTheme.h"

#include <array>

namespace jamstudio::ui
{

/** Permanent mixer strip for stage media - transport + VIDEO sound fader. */
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


/** Masters for FOH + Mon1-5 + per-bus click knobs + PC listen. */
class BusMasterStrip : public juce::Component,
                       private juce::Timer
{
public:
    explicit BusMasterStrip (jamstudio::audio::TransportController& transport)
        : transportController (transport)
    {
        for (auto& s : busSliders)
        {
            s.setSliderStyle (juce::Slider::LinearVertical);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        }

        for (auto& k : clickKnobs)
        {
            k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        }

        title.setText ("BUSES", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centred);
        title.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (title);

        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        {
            const auto bus = static_cast<jamstudio::audio::MixBus> (b);
            const auto accent = jamstudio::audio::mixBusColour (bus);
            auto& lab = busLabels[static_cast<size_t> (b)];
            auto& s = busSliders[static_cast<size_t> (b)];
            auto& clk = clickKnobs[static_cast<size_t> (b)];
            auto& clkLab = clickLabels[static_cast<size_t> (b)];

            lab.setJustificationType (juce::Justification::centred);
            lab.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            lab.setColour (juce::Label::textColourId, accent);
            addAndMakeVisible (lab);

            clkLab.setText ("clk", juce::dontSendNotification);
            clkLab.setJustificationType (juce::Justification::centred);
            clkLab.setFont (juce::FontOptions (8.0f));
            clkLab.setColour (juce::Label::textColourId, accent.withAlpha (0.85f));
            addAndMakeVisible (clkLab);

            clk.setRange (0.0, 1.0, 0.01);
            clk.setValue (transportController.getMultiBusMaster().getClickBusSend (bus),
                          juce::dontSendNotification);
            clk.setSliderSnapsToMousePosition (false);
            clk.setMouseDragSensitivity (180);
            // ~270° pot travel (classic console feel)
            clk.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                     juce::MathConstants<float>::pi * 2.8f,
                                     true);
            clk.setColour (juce::Slider::rotarySliderFillColourId, accent);
            clk.setColour (juce::Slider::rotarySliderOutlineColourId, accent.withAlpha (0.4f));
            clk.setColour (juce::Slider::thumbColourId, juce::Colours::white);
            clk.setPopupDisplayEnabled (true, true, this);
            clk.setTextValueSuffix (" %");
            clk.setNumDecimalPlacesToDisplay (0);
            clk.onValueChange = [this, bus, &clk]
            {
                transportController.getMultiBusMaster().setClickBusSend (
                    bus, static_cast<float> (clk.getValue()));
            };
            addAndMakeVisible (clk);

            s.setRange (0.0, 1.25, 0.01);
            s.setValue (transportController.getStemMixer().getBusMaster (bus), juce::dontSendNotification);
            s.setSliderSnapsToMousePosition (true);
            s.setColour (juce::Slider::thumbColourId, accent);
            s.onValueChange = [this, bus, &s]
            {
                transportController.getStemMixer().setBusMaster (
                    bus, static_cast<float> (s.getValue()));
            };
            addAndMakeVisible (s);
        }
        refreshBusLabels();

        routeHint.setText ("1-2 FOH - 3-4 M1 - 5-6 M2 - 7-8 M3 - 9-10 M4 - 11-12 M5",
                           juce::dontSendNotification);
        routeHint.setJustificationType (juce::Justification::centred);
        routeHint.setFont (juce::FontOptions (8.0f));
        routeHint.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (routeHint);

        listenLabel.setText ("PC LISTEN", juce::dontSendNotification);
        listenLabel.setJustificationType (juce::Justification::centred);
        listenLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        listenLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().accent);
        addAndMakeVisible (listenLabel);

        rebuildListenBox();
        listenBox.setTooltip ("Which bus is fed to PC speakers / virtual interface fold-down");
        {
            const auto sel = transportController.getMultiBusMaster().getOutputMonitorSelect();
            listenBox.setSelectedId (static_cast<int> (sel) + 1, juce::dontSendNotification);
        }
        listenBox.onChange = [this]
        {
            const auto id = listenBox.getSelectedId();
            auto s = jamstudio::audio::OutputMonitorSelect::foh;
            if (id == jamstudio::audio::kNumMixBuses + 1)
                s = jamstudio::audio::OutputMonitorSelect::sumAll;
            else if (id >= 1 && id <= jamstudio::audio::kNumMixBuses)
                s = static_cast<jamstudio::audio::OutputMonitorSelect> (id - 1);
            transportController.getMultiBusMaster().setOutputMonitorSelect (s);
            transportController.getMultiBusMaster().saveSettings();
        };
        addAndMakeVisible (listenBox);

        foldToggle.setButtonText ("Fold to PC stereo");
        foldToggle.setTooltip ("On: only the selected bus reaches speakers (laptop / virtual). "
                               "Off: full FOH+M1-M5 matrix when the device has enough outs.");
        foldToggle.setToggleState (transportController.getMultiBusMaster().isStereoFoldListen(),
                                   juce::dontSendNotification);
        foldToggle.onClick = [this]
        {
            transportController.getMultiBusMaster().setStereoFoldListen (foldToggle.getToggleState());
            transportController.getMultiBusMaster().saveSettings();
        };
        addAndMakeVisible (foldToggle);

        startTimerHz (12);
    }

    void syncFromMixer()
    {
        auto& m = transportController.getStemMixer();
        auto& mb = transportController.getMultiBusMaster();
        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        {
            const auto bus = static_cast<jamstudio::audio::MixBus> (b);
            busSliders[static_cast<size_t> (b)].setValue (m.getBusMaster (bus),
                                                          juce::dontSendNotification);
            clickKnobs[static_cast<size_t> (b)].setValue (mb.getClickBusSend (bus),
                                                          juce::dontSendNotification);
        }

        refreshBusLabels();
        rebuildListenBox();
        foldToggle.setToggleState (mb.isStereoFoldListen(), juce::dontSendNotification);
    }

    void refreshBusLabels()
    {
        auto& mb = transportController.getMultiBusMaster();
        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        {
            const auto bus = static_cast<jamstudio::audio::MixBus> (b);
            const auto name = mb.getBusDisplayName (bus);
            const auto longName = mb.getBusLongDisplayName (bus);
            busLabels[static_cast<size_t> (b)].setText (name, juce::dontSendNotification);
            busLabels[static_cast<size_t> (b)].setTooltip (longName + " — outs "
                                                           + jamstudio::audio::mixBusHardwareOuts (bus));
            clickKnobs[static_cast<size_t> (b)].setTooltip ("Click volume -> " + longName);
            busSliders[static_cast<size_t> (b)].setTooltip (
                longName + " master - outs " + jamstudio::audio::mixBusHardwareOuts (bus));
        }
    }

    void rebuildListenBox()
    {
        auto& mb = transportController.getMultiBusMaster();
        const auto keep = listenBox.getSelectedId();
        listenBox.clear (juce::dontSendNotification);
        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
            listenBox.addItem (mb.getOutputMonitorSelectDisplayName (
                                   static_cast<jamstudio::audio::OutputMonitorSelect> (b)),
                               b + 1);
        listenBox.addItem ("Sum all buses", jamstudio::audio::kNumMixBuses + 1);
        const auto sel = static_cast<int> (mb.getOutputMonitorSelect()) + 1;
        listenBox.setSelectedId (keep > 0 ? keep : sel, juce::dontSendNotification);
        if (listenBox.getSelectedId() <= 0)
            listenBox.setSelectedId (sel, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = JamStudioTheme::getColours();
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours.panelBackground.brighter (0.05f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (colours.border);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        auto& mb = transportController.getMultiBusMaster();
        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        {
            const auto bus = static_cast<jamstudio::audio::MixBus> (b);
            auto r = meterBounds[static_cast<size_t> (b)];
            const float lvl = juce::jlimit (0.0f, 1.0f, mb.getBusMeterLevel (bus));
            g.setColour (colours.border.withAlpha (0.5f));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (jamstudio::audio::mixBusColour (bus));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * lvl), 2.0f);
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (3);
        title.setBounds (a.removeFromTop (16));
        a.removeFromTop (2);

        foldToggle.setBounds (a.removeFromBottom (22).reduced (1));
        listenBox.setBounds (a.removeFromBottom (24).reduced (1));
        listenLabel.setBounds (a.removeFromBottom (14));
        a.removeFromBottom (2);
        routeHint.setBounds (a.removeFromBottom (22));
        a.removeFromBottom (2);

        auto meters = a.removeFromBottom (7);
        {
            const auto w = juce::jmax (1, meters.getWidth() / jamstudio::audio::kNumMixBuses);
            for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
            {
                auto m = (b + 1 < jamstudio::audio::kNumMixBuses)
                             ? meters.removeFromLeft (w)
                             : meters;
                meterBounds[static_cast<size_t> (b)] = m.reduced (1, 1).toFloat();
            }
        }
        a.removeFromBottom (2);

        // Column layout: bus name | clk label | larger rotary pot | master fader
        const auto labH = 12;
        const auto clkLabH = 11;
        // Bigger pots: ~36-52 px depending on strip height / column width
        const auto colW = juce::jmax (1, a.getWidth() / jamstudio::audio::kNumMixBuses);
        const auto knobH = juce::jlimit (36, 52, juce::jmin (colW - 2, a.getHeight() / 4));
        const auto w = colW;

        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        {
            auto c = (b + 1 < jamstudio::audio::kNumMixBuses) ? a.removeFromLeft (w) : a;
            busLabels[static_cast<size_t> (b)].setBounds (c.removeFromTop (labH));
            clickLabels[static_cast<size_t> (b)].setBounds (c.removeFromTop (clkLabH));

            auto knobArea = c.removeFromTop (knobH);
            // Prefer a solid pot size; allow slight padding
            const auto side = juce::jlimit (34, 50, juce::jmin (knobArea.getWidth() - 2,
                                                                knobArea.getHeight() - 2));
            clickKnobs[static_cast<size_t> (b)].setBounds (
                knobArea.withSizeKeepingCentre (side, side));

            c.removeFromTop (3);
            busSliders[static_cast<size_t> (b)].setBounds (c.reduced (0, 1));
        }

    }

    void timerCallback() override
    {
        repaint();
    }

private:
    jamstudio::audio::TransportController& transportController;
    juce::Label title;
    std::array<juce::Label, jamstudio::audio::kNumMixBuses> busLabels;
    std::array<juce::Label, jamstudio::audio::kNumMixBuses> clickLabels;
    std::array<juce::Slider, jamstudio::audio::kNumMixBuses> clickKnobs;
    std::array<juce::Slider, jamstudio::audio::kNumMixBuses> busSliders;
    std::array<juce::Rectangle<float>, jamstudio::audio::kNumMixBuses> meterBounds {};
    juce::Label routeHint;
    juce::Label listenLabel;
    juce::ComboBox listenBox;
    juce::ToggleButton foldToggle;
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

        startTimerHz (10);
    }

    void rebuild (jamstudio::audio::StemMixer& mixer, MixerChannelStrip::StemChangedCallback onChanged)
    {
        strips.clear();
        stripContainer.removeAllChildren();

        for (int i = 0; i < mixer.getNumStems(); ++i)
        {
            if (mixer.getStem (i) != nullptr)
            {
                auto strip = std::make_unique<MixerChannelStrip> (
                    i, mixer, transportController.getMultiBusMaster(), onChanged);
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
            setTrackLabel.setText ("Set ready - start a song to save mix", juce::dontSendNotification);
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
        // Wide enough for six bus columns with larger click pots
        const auto busW = juce::jlimit (170, 260, bounds.getWidth() / 3);
        videoStrip.setBounds (bounds.removeFromRight (videoW));
        bounds.removeFromRight (4);
        busStrip.setBounds (bounds.removeFromRight (busW));
        bounds.removeFromRight (6);

        emptyLabel.setBounds (bounds);
        viewport.setBounds (bounds);

        const auto area = viewport.getLocalBounds();
        const int count = juce::jmax (1, strips.size());
        // Wider strips for FOH + Mon 1-5 send columns.
        const int minStripW = 132;
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
    setSize (1100, 540);
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
    // ResizableWindow shadows Component& overload - pass pointers.
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
    // Manual max/restore - setFullScreen on non-native DocumentWindow is unreliable on Linux.
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
