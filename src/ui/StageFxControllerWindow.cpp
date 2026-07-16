#include "StageFxControllerWindow.h"

#include "JamStudioTheme.h"
#include "VideoOutputWindow.h"

#include <cmath>

namespace jamstudio::ui
{

namespace
{
/** Mini karaoke monitor (same layout language as full KaraokeOutputWindow). */
class KaraokePreviewPanel : public juce::Component,
                            private juce::Timer
{
public:
    explicit KaraokePreviewPanel (jamstudio::audio::TransportController& t)
        : transport (t)
    {
        startTimerHz (20);
    }

    void setLyrics (const jamstudio::notation::LyricsTrack& l)
    {
        lyrics = l;
        lyrics.sanitizeAll();
        activeLine = lyrics.isEmpty() ? -1 : lyrics.getActiveLineIndex (transport.getPosition());
        repaint();
    }

    void setSongTitle (const juce::String& t)
    {
        songTitle = jamstudio::notation::LyricsTrack::sanitizeDisplayText (t);
        repaint();
    }

    void setOutputLive (const bool live)
    {
        if (outputLive != live)
        {
            outputLive = live;
            repaint();
        }
    }

    void timerCallback() override
    {
        if (lyrics.isEmpty())
            return;
        const auto line = lyrics.getActiveLineIndex (transport.getPosition());
        if (line != activeLine)
        {
            activeLine = line;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
        g.setColour (JamStudioTheme::getColours().border);
        g.drawRect (getLocalBounds(), 1);

        auto area = getLocalBounds().reduced (8, 6);

        // Header
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        auto head = area.removeFromTop (16);
        g.drawText ("KARAOKE PREVIEW", head.removeFromLeft (head.getWidth() / 2),
                    juce::Justification::centredLeft);
        g.setColour (outputLive ? juce::Colour (0xff33cc66) : juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (outputLive ? "LIVE ON DISPLAY" : "preview only", head,
                    juce::Justification::centredRight);

        if (songTitle.isNotEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (songTitle, area.removeFromTop (14), juce::Justification::centred);
        }

        area.removeFromTop (4);

        if (lyrics.isEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("No lyrics loaded", area, juce::Justification::centred);
            return;
        }

        const int prev = activeLine > 0 ? activeLine - 1 : -1;
        const int next = (activeLine >= 0 && activeLine + 1 < lyrics.getNumLines())
                             ? activeLine + 1
                             : (activeLine < 0 && lyrics.getNumLines() > 0 ? 0 : -1);

        const auto rowH = area.getHeight() / 3;
        auto top = area.removeFromTop (rowH);
        auto mid = area.removeFromTop (rowH);
        auto bot = area;

        auto drawLine = [&] (juce::Rectangle<int> r, int idx, bool current)
        {
            if (idx < 0)
                return;
            if (const auto* line = lyrics.getLine (idx))
            {
                if (current)
                {
                    g.setColour (juce::Colour (0xff1a3a5c).withAlpha (0.9f));
                    g.fillRoundedRectangle (r.reduced (2).toFloat(), 6.0f);
                }
                g.setColour (current ? juce::Colour (0xffffdd44) : juce::Colours::white.withAlpha (0.4f));
                g.setFont (juce::FontOptions (current ? 13.0f : 11.0f,
                                              current ? juce::Font::bold : juce::Font::plain));
                g.drawFittedText (line->text, r.reduced (6, 2), juce::Justification::centred, 2);
            }
        };

        if (activeLine < 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawText ("...", mid, juce::Justification::centred);
            drawLine (bot, 0, false);
        }
        else
        {
            drawLine (top, prev, false);
            drawLine (mid, activeLine, true);
            drawLine (bot, next, false);
        }
    }

private:
    jamstudio::audio::TransportController& transport;
    jamstudio::notation::LyricsTrack lyrics;
    juce::String songTitle;
    int activeLine = -1;
    bool outputLive = false;
};

/** Mini stage / video monitor (video frame or reactive FX). */
class StagePreviewPanel : public juce::Component,
                          private juce::Timer
{
public:
    explicit StagePreviewPanel (jamstudio::audio::TransportController& t)
        : transport (t)
    {
        startTimerHz (18);
    }

    void setSongTitle (const juce::String& t)
    {
        songTitle = jamstudio::notation::LyricsTrack::sanitizeDisplayText (t);
        repaint();
    }

    void setOutputLive (const bool live)
    {
        if (outputLive != live)
        {
            outputLive = live;
            repaint();
        }
    }

    void timerCallback() override
    {
        auto& stage = transport.getStageMedia();
        energy = juce::jmax (energy * 0.88f, stage.getMeterLevel());
        phase += (stage.isPlaying() || transport.isPlaying()) ? 0.05f : 0.015f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        if (stage.hasVideo() || stage.isSlideshow())
        {
            const auto serial = stage.getVideoFrameSerial();
            auto next = stage.getVideoFrame();
            if (next.isValid() && (serial != lastSerial || ! frame.isValid()))
            {
                lastSerial = serial;
                frame = std::move (next);
            }
        }
        else if (frame.isValid())
        {
            frame = {};
            lastSerial = 0;
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff050508));
        g.setColour (JamStudioTheme::getColours().border);
        g.drawRect (getLocalBounds(), 1);

        auto area = getLocalBounds().reduced (8, 6);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        auto head = area.removeFromTop (16);
        g.drawText ("STAGE FX PREVIEW", head.removeFromLeft (head.getWidth() / 2),
                    juce::Justification::centredLeft);
        g.setColour (outputLive ? juce::Colour (0xff33cc66) : juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (outputLive ? "LIVE ON DISPLAY" : "preview only", head,
                    juce::Justification::centredRight);
        area.removeFromTop (2);

        const auto w = static_cast<float> (area.getWidth());
        const auto h = static_cast<float> (area.getHeight());
        const auto cx = area.getX() + w * 0.5f;
        const auto cy = area.getY() + h * 0.5f;

        if (frame.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
            g.drawImageWithin (frame, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                               juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                               false);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (area.removeFromBottom (22));
        }
        else
        {
            const auto glowR = juce::jmin (w, h) * (0.22f + 0.12f * energy);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a6cff).withAlpha (0.35f * juce::jmax (0.15f, energy)),
                                                     cx, cy,
                                                     juce::Colours::transparentBlack, cx + glowR, cy, true));
            g.fillEllipse (cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);

            const int bars = 28;
            auto barArea = area.removeFromBottom (juce::jmax (24, area.getHeight() / 3)).reduced (6, 4).toFloat();
            const auto barW = barArea.getWidth() / static_cast<float> (bars);
            for (int i = 0; i < bars; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (bars);
                const auto wave = 0.5f + 0.5f * std::sin (phase * 2.0f + t * 10.0f);
                const auto barH = barArea.getHeight() * wave * juce::jmax (0.2f, energy);
                g.setColour (juce::Colour::fromHSV (std::fmod (t + phase * 0.04f, 1.0f), 0.8f, 0.95f, 0.9f));
                g.fillRoundedRectangle (barArea.getX() + t * barArea.getWidth() + 1.0f,
                                        barArea.getBottom() - barH, barW - 2.0f, barH, 1.5f);
            }
        }

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (songTitle.isNotEmpty() ? songTitle : "Stage board",
                    getLocalBounds().reduced (10).removeFromBottom (20),
                    juce::Justification::centred);
    }

private:
    jamstudio::audio::TransportController& transport;
    juce::Image frame;
    juce::String songTitle;
    uint32_t lastSerial = 0;
    float phase = 0.0f;
    float energy = 0.1f;
    bool outputLive = false;
};
} // namespace

//==============================================================================
class StageFxControllerWindow::Content : public juce::Component,
                                         private juce::Timer,
                                         private juce::ChangeListener
{
public:
    Content (jamstudio::audio::TransportController& transport,
             VideoRouting& routing)
        : transportController (transport),
          stageMedia (transport.getStageMedia()),
          videoRouting (routing),
          karaokePreview (transport),
          stagePreview (transport)
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

        previewsHeading.setText ("Output previews", juce::dontSendNotification);
        previewsHeading.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (previewsHeading);

        addAndMakeVisible (karaokePreview);
        addAndMakeVisible (stagePreview);

        refreshDisplayLists();

        hint.setText ("Previews mirror the full-screen outputs. Open Karaoke / Stage FX to send "
                      "them to the chosen displays (does not auto-open in Performance mode).",
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

    void setLyrics (const jamstudio::notation::LyricsTrack& lyrics)
    {
        karaokePreview.setLyrics (lyrics);
    }

    void setSongTitle (const juce::String& songTitle)
    {
        karaokePreview.setSongTitle (songTitle);
        stagePreview.setSongTitle (songTitle);
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

        if (! outputPanelBounds.isEmpty())
        {
            g.setColour (JamStudioTheme::getColours().panelBackground.brighter (0.04f));
            g.fillRoundedRectangle (outputPanelBounds.toFloat(), 8.0f);
            g.setColour (JamStudioTheme::getColours().border);
            g.drawRoundedRectangle (outputPanelBounds.toFloat(), 8.0f, 1.0f);
        }

        if (! previewPanelBounds.isEmpty())
        {
            g.setColour (JamStudioTheme::getColours().panelBackground.brighter (0.03f));
            g.fillRoundedRectangle (previewPanelBounds.toFloat(), 8.0f);
            g.setColour (JamStudioTheme::getColours().border);
            g.drawRoundedRectangle (previewPanelBounds.toFloat(), 8.0f, 1.0f);
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (12);
        title.setBounds (a.removeFromTop (22));
        a.removeFromTop (4);

        // Previews pinned to the bottom of the controller
        const auto previewH = juce::jlimit (140, 220, getHeight() / 3);
        previewPanelBounds = a.removeFromBottom (previewH);
        auto prev = previewPanelBounds.reduced (10, 8);
        previewsHeading.setBounds (prev.removeFromTop (16));
        prev.removeFromTop (4);
        const auto gap = 8;
        auto left = prev.removeFromLeft ((prev.getWidth() - gap) / 2);
        prev.removeFromLeft (gap);
        karaokePreview.setBounds (left);
        stagePreview.setBounds (prev);

        a.removeFromBottom (6);
        hint.setBounds (a.removeFromBottom (36));
        a.removeFromBottom (6);

        auto fileRow = a.removeFromTop (28);
        openButton.setBounds (fileRow.removeFromRight (120).reduced (2));
        fileLabel.setBounds (fileRow);
        a.removeFromTop (8);

        auto deck = a.removeFromTop (44);
        const auto b = juce::jmin (40, deck.getHeight());
        playButton.setBounds (deck.removeFromLeft (b + 6).withSizeKeepingCentre (b, b));
        pauseButton.setBounds (deck.removeFromLeft (b + 6).withSizeKeepingCentre (b, b));
        stopButton.setBounds (deck.removeFromLeft (b + 6).withSizeKeepingCentre (b, b));
        deck.removeFromLeft (6);
        loopButton.setBounds (deck.removeFromLeft (64).reduced (1));
        positionLabel.setBounds (deck.reduced (8, 4));
        a.removeFromTop (8);

        volumeLabel.setBounds (a.removeFromTop (16));
        auto volRow = a.removeFromTop (26);
        volumeReadout.setBounds (volRow.removeFromRight (48));
        volumeSlider.setBounds (volRow);
        a.removeFromTop (6);
        meterBounds = a.removeFromTop (12);
        a.removeFromTop (8);

        outputPanelBounds = a;
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
        karaokePreview.setOutputLive (karaokeOpen);
        stagePreview.setOutputLive (stageOpen);
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

    juce::Label previewsHeading;
    KaraokePreviewPanel karaokePreview;
    StagePreviewPanel stagePreview;
    juce::Rectangle<int> previewPanelBounds;

    juce::Label hint;
    juce::Rectangle<int> meterBounds;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

//==============================================================================
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
    setResizeLimits (460, 520, 1100, 900);
    setSize (560, 640);
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

void StageFxControllerWindow::setLyrics (const jamstudio::notation::LyricsTrack& lyrics)
{
    if (content != nullptr)
        content->setLyrics (lyrics);
}

void StageFxControllerWindow::setSongTitle (const juce::String& title)
{
    if (content != nullptr)
        content->setSongTitle (title);
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
    const auto reserve = titleH + 8;
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
