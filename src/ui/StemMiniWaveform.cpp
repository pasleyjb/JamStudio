#include "StemMiniWaveform.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

StemMiniWaveform::StemMiniWaveform (juce::AudioFormatManager& formatManager,
                                    juce::AudioThumbnailCache& cache,
                                    jamstudio::audio::TransportController& transport,
                                    const juce::File& sourceFile)
    : transportController (transport),
      thumbnail (128, formatManager, cache)
{
    transportController.addChangeListener (this);
    thumbnail.addChangeListener (this);
    setSourceFile (sourceFile);
    // Low rate: only moves the playhead; waveform is cached.
    startTimerHz (12);
}

StemMiniWaveform::~StemMiniWaveform()
{
    stopTimer();
    thumbnail.removeChangeListener (this);
    transportController.removeChangeListener (this);
}

void StemMiniWaveform::setSourceFile (const juce::File& file)
{
    thumbnail.clear();
    invalidateWaveCache();
    lastPlayheadX = -1;

    if (file.existsAsFile())
        thumbnail.setSource (new juce::FileInputSource (file));

    repaint();
}

juce::Rectangle<int> StemMiniWaveform::waveBounds() const
{
    return getLocalBounds().reduced (1);
}

int StemMiniWaveform::playheadX() const
{
    const auto bounds = waveBounds();
    if (thumbnail.getTotalLength() <= 0.0 || bounds.getWidth() <= 0)
        return bounds.getX();

    const auto progress = juce::jlimit (0.0, 1.0,
                                        transportController.getPosition() / thumbnail.getTotalLength());
    return bounds.getX() + static_cast<int> (progress * static_cast<double> (bounds.getWidth()));
}

void StemMiniWaveform::invalidateWaveCache()
{
    waveCacheDirty = true;
    waveCache = {};
}

void StemMiniWaveform::rebuildWaveCacheIfNeeded()
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

    g.setColour (colours.waveformBackground);
    g.fillAll();

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (colours.waveform.withAlpha (0.85f));
        thumbnail.drawChannels (g,
                                { 0, 0, bounds.getWidth(), bounds.getHeight() },
                                0.0,
                                thumbnail.getTotalLength(),
                                1.0f);
    }

    waveCacheDirty = false;
}

void StemMiniWaveform::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    auto bounds = waveBounds();

    g.setColour (colours.border);
    g.drawRect (bounds);

    rebuildWaveCacheIfNeeded();
    if (waveCache.isValid())
        g.drawImageAt (waveCache, bounds.getX(), bounds.getY());
    else
    {
        g.setColour (colours.waveformBackground);
        g.fillRect (bounds);
    }

    if (thumbnail.getTotalLength() > 0.0)
    {
        const auto cursorX = playheadX();
        g.setColour (colours.lyricsHighlight.withAlpha (0.9f));
        g.drawLine (static_cast<float> (cursorX), static_cast<float> (bounds.getY()),
                    static_cast<float> (cursorX), static_cast<float> (bounds.getBottom()), 1.5f);
    }
}

void StemMiniWaveform::resized()
{
    invalidateWaveCache();
    lastPlayheadX = -1;
}

void StemMiniWaveform::mouseDown (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    const auto bounds = waveBounds();
    const auto proportion = juce::jlimit (0.0f, 1.0f,
                                          (event.position.x - static_cast<float> (bounds.getX()))
                                              / static_cast<float> (juce::jmax (1, bounds.getWidth())));

    transportController.setPosition (proportion * thumbnail.getTotalLength());
    lastPlayheadX = -1;
    repaint (bounds);
}

void StemMiniWaveform::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
    {
        invalidateWaveCache();
        repaint();
        return;
    }

    // Transport change — playhead only.
    lastPlayheadX = -1;
}

void StemMiniWaveform::timerCallback()
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    const auto x = playheadX();
    if (x == lastPlayheadX)
        return;

    const auto bounds = waveBounds();
    if (lastPlayheadX >= 0)
        repaint (lastPlayheadX - 2, bounds.getY(), 4, bounds.getHeight());
    repaint (x - 2, bounds.getY(), 4, bounds.getHeight());
    lastPlayheadX = x;
}

} // namespace jamstudio::ui
