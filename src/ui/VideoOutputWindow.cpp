#include "VideoOutputWindow.h"

#include "JamStudioTheme.h"
#include "../notation/LyricsTrack.h"

#include <cmath>

namespace jamstudio::ui
{

//==============================================================================
int VideoOutputHelpers::getNumDisplays()
{
    return juce::jmax (1, juce::Desktop::getInstance().getDisplays().displays.size());
}

juce::Rectangle<int> VideoOutputHelpers::getDisplayBounds (const int displayIndex, const bool fullArea)
{
    const auto& displays = juce::Desktop::getInstance().getDisplays().displays;

    if (displays.isEmpty())
        return { 0, 0, 1280, 720 };

    const auto idx = juce::jlimit (0, displays.size() - 1, displayIndex);
    const auto& d = displays.getReference (idx);

    // Prefer full logical panel for stage outputs; fall back to usable area.
    if (fullArea)
        return d.logicalBounds.getSmallestIntegerContainer();

    return d.userBounds.getSmallestIntegerContainer();
}

juce::String VideoOutputHelpers::getDisplayLabel (const int displayIndex)
{
    const auto bounds = getDisplayBounds (displayIndex, true);
    return "Display " + juce::String (displayIndex + 1)
           + " (" + juce::String (bounds.getWidth()) + "x" + juce::String (bounds.getHeight()) + ")";
}

//==============================================================================
class KaraokeOutputWindow::Content : public juce::Component,
                                     private juce::Timer
{
public:
    explicit Content (jamstudio::audio::TransportController& t)
        : transport (t)
    {
        startTimerHz (30);
    }

    void setLyrics (const jamstudio::notation::LyricsTrack& l)
    {
        lyrics = l;
        lyrics.sanitizeAll();
        repaint();
    }

    void setSongTitle (const juce::String& t)
    {
        songTitle = jamstudio::notation::LyricsTrack::sanitizeDisplayText (t);
        repaint();
    }

    void timerCallback()
    {
        if (lyrics.isEmpty())
            return;

        const auto pos = transport.getPosition();
        const auto line = lyrics.getActiveLineIndex (pos);

        if (line != activeLine)
        {
            activeLine = line;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        // Karaoke black stage background
        g.fillAll (juce::Colours::black);

        auto area = getLocalBounds().reduced (juce::jmax (24, getWidth() / 20),
                                              juce::jmax (24, getHeight() / 16));

        // Song title strip
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (juce::jlimit (16.0f, 28.0f, getHeight() * 0.035f)));
        g.drawText (songTitle.isNotEmpty() ? songTitle : "JamStudio Karaoke",
                    area.removeFromTop (juce::jlimit (28, 48, getHeight() / 14)),
                    juce::Justification::centred);

        area.removeFromTop (12);

        if (lyrics.isEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.setFont (juce::FontOptions (24.0f));
            g.drawText ("Waiting for lyrics...", area, juce::Justification::centred);
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
                    g.setColour (juce::Colour (0xff1a3a5c).withAlpha (0.85f));
                    g.fillRoundedRectangle (r.reduced (8).toFloat(), 16.0f);
                }

                g.setColour (current ? juce::Colour (0xffffdd44) : juce::Colours::white.withAlpha (0.45f));
                g.setFont (juce::FontOptions (current ? juce::jlimit (28.0f, 64.0f, getHeight() * 0.07f)
                                                      : juce::jlimit (18.0f, 36.0f, getHeight() * 0.04f),
                                              current ? juce::Font::bold : juce::Font::plain));
                g.drawFittedText (line->text, r.reduced (24, 8), juce::Justification::centred, 3);
            }
        };

        if (activeLine < 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (juce::FontOptions (22.0f));
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
};

KaraokeOutputWindow::KaraokeOutputWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("JamStudio - Karaoke Output",
                      juce::Colours::black,
                      DocumentWindow::closeButton),
      transportController (transport)
{
    setUsingNativeTitleBar (false);
    content = std::make_unique<Content> (transport);
    setContentNonOwned (content.get(), false);
    setResizable (true, true);
    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

KaraokeOutputWindow::~KaraokeOutputWindow()
{
    hideInternal();
    setContentNonOwned (nullptr, false);
    content.reset();
}

void KaraokeOutputWindow::setLyrics (const jamstudio::notation::LyricsTrack& lyrics)
{
    if (content != nullptr)
        content->setLyrics (lyrics);
}

void KaraokeOutputWindow::setSongTitle (const juce::String& title)
{
    if (content != nullptr)
        content->setSongTitle (title);
}

void KaraokeOutputWindow::showOnDisplay (const int index)
{
    displayIndex = juce::jmax (0, index);
    const auto bounds = VideoOutputHelpers::getDisplayBounds (displayIndex, true);

    setFullScreen (false);
    setBounds (bounds);

    if (! isOnDesktop())
        addToDesktop (getDesktopWindowStyleFlags());

    setVisible (true);
    toFront (true);
    // Fullscreen after show for reliable multi-monitor placement on Linux WMs
    setFullScreen (true);
    outputOpen = true;
}

void KaraokeOutputWindow::hideOutput()
{
    hideInternal();
}

void KaraokeOutputWindow::hideInternal()
{
    outputOpen = false;
    setFullScreen (false);
    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

void KaraokeOutputWindow::closeButtonPressed()
{
    hideInternal();
}

void KaraokeOutputWindow::userTriedToCloseWindow()
{
    hideInternal();
}

//==============================================================================
class StageFxOutputWindow::Content : public juce::Component,
                                     private juce::Timer
{
public:
    explicit Content (jamstudio::audio::TransportController& t)
        : transport (t)
    {
        // Keep UI light while stage video decodes on a worker thread.
        startTimerHz (20);
    }

    void setSongTitle (const juce::String& t)
    {
        songTitle = jamstudio::notation::LyricsTrack::sanitizeDisplayText (t);
        repaint();
    }

    void setSetInfo (const juce::String& name, const int idx, const int count)
    {
        setName = name;
        songIndex = idx;
        songCount = count;
        repaint();
    }

    void setWaitingBetweenSongs (const bool w)
    {
        waiting = w;
        repaint();
    }

    void setExternalEnergy (const float e)
    {
        externalEnergy = juce::jlimit (0.0f, 1.0f, e);
    }

    void timerCallback()
    {
        auto& stage = transport.getStageMedia();

        // Prefer live stage-media meter when a board clip is playing.
        const auto mediaLevel = stage.getMeterLevel();
        externalEnergy = juce::jmax (externalEnergy * 0.92f, mediaLevel);

        bool needRepaint = false;

        // Non-blocking grab of latest worker-decoded frame (never decode on UI thread).
        if (stage.hasVideo() && (stage.isPlaying() || stage.getPosition() > 0.0))
        {
            const auto serial = stage.getVideoFrameSerial();
            auto next = stage.getVideoFrame();
            if (next.isValid() && (serial != lastVideoSerial || ! videoFrame.isValid()))
            {
                lastVideoSerial = serial;
                videoFrame = std::move (next);
                needRepaint = true;
            }
        }
        else if (videoFrame.isValid() && ! stage.hasVideo())
        {
            videoFrame = {};
            lastVideoSerial = 0;
            needRepaint = true;
        }

        phase += (transport.isPlaying() || stage.isPlaying() || externalEnergy > 0.05f) ? 0.045f : 0.012f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        // Approximate level from transport motion + stage media meter
        const auto length = transport.getLengthInSeconds();
        const auto pos = transport.getPosition();
        playNorm = (length > 0.0) ? static_cast<float> (pos / length) : 0.0f;
        const auto transportEnergy = transport.isPlaying()
                                         ? (0.35f + 0.65f * (0.5f + 0.5f * std::sin (phase * 3.0f + playNorm * 20.0f)))
                                         : (waiting ? 0.15f : 0.05f);
        energy = juce::jmax (transportEnergy, externalEnergy);

        // Always repaint reactive FX; for video only when frame/energy changes much.
        if (! videoFrame.isValid() || needRepaint || stage.isPlaying() || transport.isPlaying())
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // Deep stage black
        g.fillAll (juce::Colour (0xff050508));

        const auto w = static_cast<float> (getWidth());
        const auto h = static_cast<float> (getHeight());
        const auto cx = w * 0.5f;
        const auto cy = h * 0.5f;
        const bool showingVideo = videoFrame.isValid();

        if (showingVideo)
        {
            // Letterbox stage media video (MPEG / MP4 / MOV / etc.)
            g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
            g.drawImageWithin (videoFrame,
                               0,
                               0,
                               getWidth(),
                               getHeight(),
                               juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                               false);

            // Light bottom shade for title card (cheaper than full vignette fill)
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack,
                                                     cx,
                                                     h * 0.55f,
                                                     juce::Colours::black.withAlpha (0.65f),
                                                     cx,
                                                     h,
                                                     false));
            g.fillRect (0.0f, h * 0.55f, w, h * 0.45f);
        }
        else
        {
            // Reactive board visuals when no video track is loaded
            const auto glowR = juce::jmin (w, h) * (0.25f + 0.15f * energy);
            const auto glowCol = waiting ? juce::Colour (0xff22aa55) : juce::Colour (0xff2a6cff);
            g.setGradientFill (juce::ColourGradient (glowCol.withAlpha (0.35f * energy), cx, cy,
                                                     juce::Colours::transparentBlack, cx + glowR, cy, true));
            g.fillEllipse (cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);

            const int bars = 48;
            const auto barArea = getLocalBounds().removeFromBottom (getHeight() / 3).reduced (20, 10).toFloat();
            const auto barW = barArea.getWidth() / static_cast<float> (bars);

            for (int i = 0; i < bars; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (bars);
                const auto wave = 0.5f + 0.5f * std::sin (phase * 2.0f + t * 12.0f + playNorm * 30.0f);
                const auto barH = barArea.getHeight() * wave * energy * (0.4f + 0.6f * t);
                const auto x = barArea.getX() + t * barArea.getWidth();
                auto col = juce::Colour::fromHSV (std::fmod (t + phase * 0.05f, 1.0f), 0.85f, 0.95f, 0.9f);
                if (waiting)
                    col = juce::Colour (0xff33cc66).withMultipliedBrightness (0.5f + 0.5f * wave);

                g.setColour (col);
                g.fillRoundedRectangle (x + 1.0f, barArea.getBottom() - barH, barW - 2.0f, barH, 2.0f);
            }

            g.setColour (juce::Colours::white.withAlpha (0.03f));
            for (int y = 0; y < getHeight(); y += 4)
                g.drawHorizontalLine (y, 0.0f, w);
        }

        // Center title card (overlay on video or FX)
        auto titleArea = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 40, 900),
                                                                 juce::jlimit (80, 160, getHeight() / 5));
        g.setColour (juce::Colours::black.withAlpha (showingVideo ? 0.55f : 0.45f));
        g.fillRoundedRectangle (titleArea.toFloat(), 12.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (juce::jlimit (22.0f, 48.0f, getHeight() * 0.055f), juce::Font::bold));
        g.drawText (songTitle.isNotEmpty() ? songTitle : "JamStudio Live",
                    titleArea.removeFromTop (titleArea.getHeight() * 2 / 3),
                    juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.setFont (juce::FontOptions (juce::jlimit (14.0f, 24.0f, getHeight() * 0.028f)));
        juce::String sub;
        if (songCount > 0 && songIndex >= 0)
            sub = setName + "  |  Song " + juce::String (songIndex + 1) + " / " + juce::String (songCount);
        else if (setName.isNotEmpty())
            sub = setName;
        if (waiting)
            sub = (sub.isNotEmpty() ? sub + "  |  " : juce::String()) + "WAITING FOR NEXT";
        if (showingVideo)
            sub = (sub.isNotEmpty() ? sub + "  |  " : juce::String()) + "VIDEO";
        g.drawText (sub, titleArea, juce::Justification::centred);

        // Progress arc for song transport (not stage media)
        if (playNorm > 0.0f && transport.isPlaying() && ! showingVideo)
        {
            juce::Path arc;
            const auto r = juce::jmin (w, h) * 0.18f;
            arc.addCentredArc (cx, cy, r, r, 0.0f,
                               -juce::MathConstants<float>::halfPi,
                               -juce::MathConstants<float>::halfPi + playNorm * juce::MathConstants<float>::twoPi,
                               true);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.strokePath (arc, juce::PathStrokeType (4.0f));
        }
    }

private:
    jamstudio::audio::TransportController& transport;
    juce::String songTitle;
    juce::String setName;
    int songIndex = -1;
    int songCount = 0;
    bool waiting = false;
    float phase = 0.0f;
    float energy = 0.0f;
    float playNorm = 0.0f;
    float externalEnergy = 0.0f;
    juce::Image videoFrame;
    uint32_t lastVideoSerial = 0;
};

StageFxOutputWindow::StageFxOutputWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("JamStudio - Stage FX Output",
                      juce::Colours::black,
                      DocumentWindow::closeButton),
      transportController (transport)
{
    setUsingNativeTitleBar (false);
    content = std::make_unique<Content> (transport);
    setContentNonOwned (content.get(), false);
    setResizable (true, true);
    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

StageFxOutputWindow::~StageFxOutputWindow()
{
    hideInternal();
    setContentNonOwned (nullptr, false);
    content.reset();
}

void StageFxOutputWindow::setSongTitle (const juce::String& title)
{
    if (content != nullptr)
        content->setSongTitle (title);
}

void StageFxOutputWindow::setSetInfo (const juce::String& setName, const int songIndex, const int songCount)
{
    if (content != nullptr)
        content->setSetInfo (setName, songIndex, songCount);
}

void StageFxOutputWindow::setWaitingBetweenSongs (const bool waiting)
{
    if (content != nullptr)
        content->setWaitingBetweenSongs (waiting);
}

void StageFxOutputWindow::setExternalEnergy (const float energy01)
{
    if (content != nullptr)
        content->setExternalEnergy (energy01);
}

void StageFxOutputWindow::showOnDisplay (const int index)
{
    displayIndex = juce::jmax (0, index);
    const auto bounds = VideoOutputHelpers::getDisplayBounds (displayIndex, true);

    setFullScreen (false);
    setBounds (bounds);

    if (! isOnDesktop())
        addToDesktop (getDesktopWindowStyleFlags());

    setVisible (true);
    toFront (true);
    setFullScreen (true);
    outputOpen = true;
}

void StageFxOutputWindow::hideOutput()
{
    hideInternal();
}

void StageFxOutputWindow::hideInternal()
{
    outputOpen = false;
    setFullScreen (false);
    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

void StageFxOutputWindow::closeButtonPressed()
{
    hideInternal();
}

void StageFxOutputWindow::userTriedToCloseWindow()
{
    hideInternal();
}

} // namespace jamstudio::ui
