#include "WaveformDisplay.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

WaveformDisplay::WaveformDisplay (juce::AudioFormatManager& formatManager,
                                  juce::AudioThumbnailCache& cache,
                                  jamstudio::audio::TransportController& transport)
    : transportController (transport),
      thumbnail (512, formatManager, cache)
{
    transportController.addChangeListener (this);
    thumbnail.addChangeListener (this);
    // Cursor-only updates while playing; full redraws are cached.
    startTimerHz (15);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
    thumbnail.removeChangeListener (this);
    transportController.removeChangeListener (this);
}

void WaveformDisplay::setSourceFile (const juce::File& file)
{
    thumbnail.clear();
    waveCache = {};
    waveCacheDirty = true;
    lastPlayheadX = -1;

    if (file.existsAsFile())
        thumbnail.setSource (new juce::FileInputSource (file));

    repaint();
}

void WaveformDisplay::clear()
{
    thumbnail.clear();
    waveCache = {};
    waveCacheDirty = true;
    lastPlayheadX = -1;
    repaint();
}

juce::Rectangle<int> WaveformDisplay::waveBounds() const
{
    return getLocalBounds().reduced (2).reduced (6, 8);
}

int WaveformDisplay::playheadX() const
{
    const auto bounds = waveBounds();
    if (thumbnail.getTotalLength() <= 0.0 || bounds.getWidth() <= 0)
        return bounds.getX();

    const auto progress = juce::jlimit (0.0, 1.0,
                                        transportController.getPosition() / thumbnail.getTotalLength());
    return bounds.getX() + static_cast<int> (progress * static_cast<double> (bounds.getWidth()));
}

void WaveformDisplay::invalidateWaveCache()
{
    waveCacheDirty = true;
    waveCache = {};
}

void WaveformDisplay::rebuildWaveCacheIfNeeded()
{
    const auto bounds = waveBounds();
    if (bounds.isEmpty())
        return;

    if (! waveCacheDirty
        && waveCache.isValid()
        && waveCache.getWidth() == bounds.getWidth()
        && waveCache.getHeight() == bounds.getHeight())
        return;

    waveCache = juce::Image (juce::Image::ARGB, juce::jmax (1, bounds.getWidth()),
                             juce::jmax (1, bounds.getHeight()), true);
    juce::Graphics g (waveCache);
    const auto colours = JamStudioTheme::getColours();

    g.fillAll (juce::Colours::transparentBlack);

    // Centre line
    g.setColour (colours.borderLight.withAlpha (0.25f));
    g.drawHorizontalLine (bounds.getHeight() / 2, 0.0f, static_cast<float> (bounds.getWidth()));

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (colours.waveform.withAlpha (0.9f));
        thumbnail.drawChannels (g,
                                { 0, 0, bounds.getWidth(), bounds.getHeight() },
                                0.0,
                                thumbnail.getTotalLength(),
                                1.0f);
    }

    waveCacheDirty = false;
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.waveformBackground);

    auto bounds = getLocalBounds().reduced (2);

    g.setColour (colours.panelBackground.brighter (0.03f));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);

    auto wave = waveBounds();

    if (thumbnail.getTotalLength() > 0.0)
    {
        rebuildWaveCacheIfNeeded();
        if (waveCache.isValid())
            g.drawImageAt (waveCache, wave.getX(), wave.getY());

        const auto cursorX = playheadX();
        g.setColour (colours.lyricsHighlight.withAlpha (0.2f));
        g.fillRect (cursorX - 3, wave.getY(), 6, wave.getHeight());
        g.setColour (colours.lyricsHighlight);
        g.drawLine (static_cast<float> (cursorX), static_cast<float> (wave.getY()),
                    static_cast<float> (cursorX), static_cast<float> (wave.getBottom()), 2.0f);
    }
    else
    {
        g.setColour (colours.textSecondary);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Main waveform - open a song to begin",
                    wave, juce::Justification::centred);
    }
}

void WaveformDisplay::resized()
{
    invalidateWaveCache();
    lastPlayheadX = -1;
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    const auto bounds = waveBounds();
    const auto proportion = juce::jlimit (0.0f, 1.0f,
                                          (event.position.x - static_cast<float> (bounds.getX()))
                                              / static_cast<float> (juce::jmax (1, bounds.getWidth())));

    transportController.setPosition (proportion * thumbnail.getTotalLength());
    lastPlayheadX = -1;
    repaint();
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
    {
        invalidateWaveCache();
        repaint();
        return;
    }

    // Transport start/stop — don't redraw full waveform, just playhead soon.
    lastPlayheadX = -1;
}

void WaveformDisplay::timerCallback()
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    const bool playing = transportController.isPlaying();
    if (! playing && lastPlayheadX >= 0)
    {
        // Idle: occasional check in case seek happened without us noticing.
        const auto x = playheadX();
        if (x == lastPlayheadX)
            return;
    }

    const auto x = playheadX();
    if (x == lastPlayheadX)
        return;

    const auto wave = waveBounds();
    // Dirty only the old and new playhead strips (avoids full thumbnail redraw).
    if (lastPlayheadX >= 0)
        repaint (lastPlayheadX - 4, wave.getY(), 8, wave.getHeight());
    repaint (x - 4, wave.getY(), 8, wave.getHeight());
    lastPlayheadX = x;
}

} // namespace jamstudio::ui
