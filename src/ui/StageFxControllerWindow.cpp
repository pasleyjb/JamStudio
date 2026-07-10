#include "StageFxControllerWindow.h"

#include "JamStudioTheme.h"
#include "VideoOutputWindow.h"

namespace jamstudio::ui
{

class StageFxControllerWindow::Content : public juce::Component,
                                         private juce::Timer,
                                         private juce::ChangeListener
{
public:
    Content (jamstudio::audio::TransportController& transport,
             VideoRouting& routing)
        : transportController (transport),
          stageMedia (transport.getStageMedia()),
          videoRouting (routing)
    {
        title.setText ("Stage Visual Effects", juce::dontSendNotification);
        title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        addAndMakeVisible (title);

        fileLabel.setText ("No media loaded", juce::dontSendNotification);
        fileLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        fileLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (fileLabel);

        openButton.setButtonText ("Open Media...");
        openButton.onClick = [this] { chooseFile(); };
        addAndMakeVisible (openButton);

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

        loopButton.setClickingTogglesState (true);
        loopButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
        loopButton.setToggleState (stageMedia.isLooping(), juce::dontSendNotification);
        loopButton.setTooltip ("Loop stage video / slideshow continuously");
        loopButton.onClick = [this]
        {
            stageMedia.setLooping (loopButton.getToggleState());
            updateLoopIndicator();
        };
        addAndMakeVisible (loopButton);

        volumeLabel.setText ("Video sound", juce::dontSendNotification);
        volumeLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (volumeLabel);

        volumeSlider.setRange (0.0, 1.0, 0.01);
        volumeSlider.setValue (stageMedia.getVolume(), juce::dontSendNotification);
        volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        volumeSlider.onValueChange = [this]
        {
            stageMedia.setVolume (static_cast<float> (volumeSlider.getValue()));
            volumeReadout.setText (juce::String (static_cast<int> (volumeSlider.getValue() * 100)) + " %",
                                   juce::dontSendNotification);
        };
        addAndMakeVisible (volumeSlider);

        volumeReadout.setJustificationType (juce::Justification::centred);
        volumeReadout.setText ("85 %", juce::dontSendNotification);
        addAndMakeVisible (volumeReadout);

        positionLabel.setJustificationType (juce::Justification::centredLeft);
        positionLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (positionLabel);

        // ---- Video outputs ----
        outputsHeading.setText ("Video outputs", juce::dontSendNotification);
        outputsHeading.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (outputsHeading);

        karaokeLabel.setText ("Karaoke", juce::dontSendNotification);
        karaokeLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (karaokeLabel);

        stageLabel.setText ("Stage FX", juce::dontSendNotification);
        stageLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (stageLabel);

        karaokeDisplayBox.onChange = [this]
        {
            const auto idx = karaokeDisplayBox.getSelectedItemIndex();
            if (idx < 0)
                return;

            if (videoRouting.setKaraokeDisplay)
                videoRouting.setKaraokeDisplay (idx);

            // Move live output if already open
            if (videoRouting.isKaraokeVisible && videoRouting.isKaraokeVisible()
                && videoRouting.openKaraoke)
                videoRouting.openKaraoke();

            updateOutputButtons();
        };
        addAndMakeVisible (karaokeDisplayBox);

        stageDisplayBox.onChange = [this]
        {
            const auto idx = stageDisplayBox.getSelectedItemIndex();
            if (idx < 0)
                return;

            if (videoRouting.setStageDisplay)
                videoRouting.setStageDisplay (idx);

            if (videoRouting.isStageVisible && videoRouting.isStageVisible()
                && videoRouting.openStage)
                videoRouting.openStage();

            updateOutputButtons();
        };
        addAndMakeVisible (stageDisplayBox);

        karaokeToggle.onClick = [this]
        {
            const bool open = videoRouting.isKaraokeVisible && videoRouting.isKaraokeVisible();
            if (open)
            {
                if (videoRouting.closeKaraoke)
                    videoRouting.closeKaraoke();
            }
            else
            {
                const auto idx = karaokeDisplayBox.getSelectedItemIndex();
                if (idx >= 0 && videoRouting.setKaraokeDisplay)
                    videoRouting.setKaraokeDisplay (idx);
                if (videoRouting.openKaraoke)
                    videoRouting.openKaraoke();
            }
            updateOutputButtons();
        };
        addAndMakeVisible (karaokeToggle);

        stageToggle.onClick = [this]
        {
            const bool open = videoRouting.isStageVisible && videoRouting.isStageVisible();
            if (open)
            {
                if (videoRouting.closeStage)
                    videoRouting.closeStage();
            }
            else
            {
                const auto idx = stageDisplayBox.getSelectedItemIndex();
                if (idx >= 0 && videoRouting.setStageDisplay)
                    videoRouting.setStageDisplay (idx);
                if (videoRouting.openStage)
                    videoRouting.openStage();
            }
            updateOutputButtons();
        };
        addAndMakeVisible (stageToggle);

        refreshDisplayLists();

        hint.setText ("Choose displays for Karaoke and Stage FX, then Open. "
                      "Screens do not auto-open in Performance mode. "
                      "MPEG/video media plays on Stage FX output.",
                      juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (hint);

        stageMedia.addChangeListener (this);
        startTimerHz (20);
        updateTransport();
        updateOutputButtons();
    }

    ~Content() override
    {
        stageMedia.removeChangeListener (this);
    }

    void syncVideoRoutingUi()
    {
        refreshDisplayLists();
        updateOutputButtons();
    }

    void syncVolumeFromPlayer()
    {
        volumeSlider.setValue (stageMedia.getVolume(), juce::dontSendNotification);
        volumeReadout.setText (juce::String (static_cast<int> (stageMedia.getVolume() * 100)) + " %",
                               juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (JamStudioTheme::getColours().windowBackground);

        auto meter = meterBounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (meter, 3.0f);
        const auto level = stageMedia.getMeterLevel();
        g.setColour (level > 0.8f ? juce::Colours::red
                                  : (level > 0.5f ? juce::Colours::yellow : juce::Colours::limegreen));
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * level), 3.0f);
        g.setColour (JamStudioTheme::getColours().border);
        g.drawRoundedRectangle (meter, 3.0f, 1.0f);

        // Subtle panel behind output controls
        if (! outputPanelBounds.isEmpty())
        {
            g.setColour (JamStudioTheme::getColours().panelBackground.brighter (0.04f));
            g.fillRoundedRectangle (outputPanelBounds.toFloat(), 8.0f);
            g.setColour (JamStudioTheme::getColours().border);
            g.drawRoundedRectangle (outputPanelBounds.toFloat(), 8.0f, 1.0f);
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (12);
        title.setBounds (a.removeFromTop (24));
        a.removeFromTop (6);
        auto fileRow = a.removeFromTop (30);
        openButton.setBounds (fileRow.removeFromRight (120).reduced (2));
        fileLabel.setBounds (fileRow);
        a.removeFromTop (10);

        auto deck = a.removeFromTop (48);
        const auto b = juce::jmin (44, deck.getHeight());
        playButton.setBounds (deck.removeFromLeft (b + 6).withSizeKeepingCentre (b, b));
        pauseButton.setBounds (deck.removeFromLeft (b + 6).withSizeKeepingCentre (b, b));
        stopButton.setBounds (deck.removeFromLeft (b + 6).withSizeKeepingCentre (b, b));
        deck.removeFromLeft (6);
        loopButton.setBounds (deck.removeFromLeft (64).reduced (1));
        positionLabel.setBounds (deck.reduced (8, 4));
        a.removeFromTop (10);

        volumeLabel.setBounds (a.removeFromTop (18));
        auto volRow = a.removeFromTop (28);
        volumeReadout.setBounds (volRow.removeFromRight (48));
        volumeSlider.setBounds (volRow);
        a.removeFromTop (8);
        meterBounds = a.removeFromTop (14);
        a.removeFromTop (10);

        // Outputs block
        outputPanelBounds = a.removeFromTop (118);
        auto out = outputPanelBounds.reduced (10, 8);
        outputsHeading.setBounds (out.removeFromTop (18));
        out.removeFromTop (4);

        auto karaokeRow = out.removeFromTop (28);
        karaokeLabel.setBounds (karaokeRow.removeFromLeft (70));
        karaokeToggle.setBounds (karaokeRow.removeFromRight (110).reduced (2));
        karaokeDisplayBox.setBounds (karaokeRow.reduced (2));
        out.removeFromTop (6);

        auto stageRow = out.removeFromTop (28);
        stageLabel.setBounds (stageRow.removeFromLeft (70));
        stageToggle.setBounds (stageRow.removeFromRight (110).reduced (2));
        stageDisplayBox.setBounds (stageRow.reduced (2));

        a.removeFromTop (8);
        hint.setBounds (a.removeFromTop (52));
    }

private:
    void refreshDisplayLists()
    {
        const auto n = VideoOutputHelpers::getNumDisplays();
        const auto karaokeSel = videoRouting.getKaraokeDisplay ? videoRouting.getKaraokeDisplay() : 0;
        const auto stageSel = videoRouting.getStageDisplay ? videoRouting.getStageDisplay() : 0;

        karaokeDisplayBox.clear (juce::dontSendNotification);
        stageDisplayBox.clear (juce::dontSendNotification);

        for (int i = 0; i < n; ++i)
        {
            const auto label = VideoOutputHelpers::getDisplayLabel (i);
            karaokeDisplayBox.addItem (label, i + 1);
            stageDisplayBox.addItem (label, i + 1);
        }

        karaokeDisplayBox.setSelectedItemIndex (juce::jlimit (0, n - 1, karaokeSel),
                                                juce::dontSendNotification);
        stageDisplayBox.setSelectedItemIndex (juce::jlimit (0, n - 1, stageSel),
                                              juce::dontSendNotification);
    }

    void updateOutputButtons()
    {
        const bool karaokeOpen = videoRouting.isKaraokeVisible && videoRouting.isKaraokeVisible();
        const bool stageOpen = videoRouting.isStageVisible && videoRouting.isStageVisible();

        karaokeToggle.setButtonText (karaokeOpen ? "Close Karaoke" : "Open Karaoke");
        stageToggle.setButtonText (stageOpen ? "Close Stage" : "Open Stage");
    }

    void chooseFile()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Open stage media (MPEG video / audio)",
            juce::File {},
            "*.wav;*.mp3;*.mp2;*.flac;*.ogg;*.aiff;*.m4a;*.aac;"
            "*.mp4;*.m4v;*.mov;*.mkv;*.webm;*.mpeg;*.mpg;*.m2ts;*.ts;*.avi;*.wmv;*.flv;*.vob");

        constexpr auto browserFlags = juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (browserFlags, [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            juce::String error;

            if (! stageMedia.loadFile (file, error))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Stage media",
                                                        error);
                return;
            }

            updateTransport();
        });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        updateTransport();
    }

    void timerCallback() override
    {
        updateTransport();
        updateLoopIndicator();
        updateOutputButtons();
        repaint (meterBounds.expanded (1));
    }

    void updateLoopIndicator()
    {
        const auto on = stageMedia.isLooping();
        loopButton.setToggleState (on, juce::dontSendNotification);
        loopButton.setIndicatorActive (on, false);
    }

    void updateTransport()
    {
        const auto playing = stageMedia.isPlaying();
        playButton.setActive (playing);
        pauseButton.setActive (! playing && stageMedia.getPosition() > 0.05);
        stopButton.setActive (! playing && stageMedia.getPosition() <= 0.05);

        if (stageMedia.hasMedia())
        {
            const auto pos = stageMedia.getPosition();
            const auto len = stageMedia.getLengthInSeconds();
            auto fmt = [] (double s)
            {
                const auto t = juce::jmax (0, juce::roundToInt (s));
                return juce::String::formatted ("%d:%02d", t / 60, t % 60);
            };
            positionLabel.setText (fmt (pos) + " / " + fmt (len), juce::dontSendNotification);
            const auto info = stageMedia.getCodecInfo();
            fileLabel.setText (stageMedia.getDisplayName()
                                   + (info.isNotEmpty() ? "  [" + info + "]" : juce::String())
                                   + (stageMedia.hasVideo() ? "  (video)" : juce::String()),
                               juce::dontSendNotification);
        }
        else
        {
            positionLabel.setText ("--:-- / --:--", juce::dontSendNotification);
        }

        syncVolumeFromPlayer();
    }

    jamstudio::audio::TransportController& transportController;
    jamstudio::audio::StageMediaPlayer& stageMedia;
    VideoRouting& videoRouting;

    juce::Label title;
    juce::Label fileLabel;
    juce::TextButton openButton;
    TapeDeckButton playButton { "stagePlay", TapeDeckButton::Icon::play };
    TapeDeckButton pauseButton { "stagePause", TapeDeckButton::Icon::pause };
    TapeDeckButton stopButton { "stageStop", TapeDeckButton::Icon::stop };
    IndicatorButton loopButton { "stageLoop", "LOOP" };
    juce::Label volumeLabel;
    juce::Slider volumeSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label volumeReadout;
    juce::Label positionLabel;

    juce::Label outputsHeading;
    juce::Label karaokeLabel;
    juce::Label stageLabel;
    juce::ComboBox karaokeDisplayBox;
    juce::ComboBox stageDisplayBox;
    juce::TextButton karaokeToggle;
    juce::TextButton stageToggle;
    juce::Rectangle<int> outputPanelBounds;

    juce::Label hint;
    juce::Rectangle<int> meterBounds;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

StageFxControllerWindow::StageFxControllerWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("Stage FX Controller",
                      JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::closeButton),
      transportController (transport)
{
    juce::ignoreUnused (transportController);
    windowOpen = false;
    setVisible (false);
    setUsingNativeTitleBar (false);
    content = std::make_unique<Content> (transport, videoRouting);
    setContentNonOwned (content.get(), false);
    setResizable (true, true);
    setResizeLimits (400, 360, 900, 700);
    setSize (500, 440);
    restoredBounds = getBounds();

    attachButton.setTooltip ("Stick to mixer (<>)");
    detachButton.setTooltip ("Unstick from mixer (><)");
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

StageFxControllerWindow::~StageFxControllerWindow()
{
    hideController();
    setContentNonOwned (nullptr, false);
    content.reset();
}

void StageFxControllerWindow::setVideoRouting (VideoRouting routing)
{
    videoRouting = std::move (routing);
    if (content != nullptr)
        content->syncVideoRoutingUi();
}

void StageFxControllerWindow::syncVideoRoutingUi()
{
    if (content != nullptr)
        content->syncVideoRoutingUi();
}

void StageFxControllerWindow::showController (const bool shouldShow)
{
    if (! shouldShow)
    {
        hideController();
        return;
    }

    if (content != nullptr)
        content->syncVideoRoutingUi();

    setBounds (restoredBounds);
    if (! isOnDesktop())
        addToDesktop (getDesktopWindowStyleFlags());
    setVisible (true);
    toFront (true);
    windowOpen = true;
}

void StageFxControllerWindow::hideController()
{
    if (getWidth() > 0 && getHeight() > 0)
        restoredBounds = getBounds();
    windowOpen = false;
    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

void StageFxControllerWindow::closeButtonPressed()
{
    hideController();
}

void StageFxControllerWindow::userTriedToCloseWindow()
{
    hideController();
}

void StageFxControllerWindow::setDockCallbacks (std::function<void()> onAttach,
                                                std::function<void()> onDetach)
{
    dockAttach = std::move (onAttach);
    dockDetach = std::move (onDetach);
}

void StageFxControllerWindow::setDockStickyState (const bool sticky)
{
    attachButton.setVisible (! sticky);
    detachButton.setVisible (sticky);
    attachButton.setEnabled (! sticky);
    detachButton.setEnabled (sticky);
    layoutDockButtons();
}

void StageFxControllerWindow::layoutDockButtons()
{
    const auto titleH = getTitleBarHeight();
    const auto reserve = titleH + 8; // close only
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

void StageFxControllerWindow::resized()
{
    DocumentWindow::resized();
    layoutDockButtons();
}

} // namespace jamstudio::ui
