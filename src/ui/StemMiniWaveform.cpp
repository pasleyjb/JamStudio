#include "StemMiniWaveform.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

StemMiniWaveform::StemMiniWaveform (juce::AudioFormatManager& formatManager,
                                    juce::AudioThumbnailCache& cache,
                                    jamstudio::audio::TransportController& transport,
                                    const juce::File& sourceFile)
    : transportController (transport),
      thumbnail (256, formatManager, cache)
{
    transportController.addChangeListener (this);
    transportController.getStemMixer().addChangeListener (this);
    setSourceFile (sourceFile);
    startTimerHz (20);
}

void StemMiniWaveform::setSourceFile (const juce::File& file)
{
    thumbnail.clear();

    if (file.existsAsFile())
        thumbnail.setSource (new juce::FileInputSource (file));

    repaint();
}

void StemMiniWaveform::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    auto bounds = getLocalBounds().reduced (1);

    g.setColour (colours.waveformBackground);
    g.fillRect (bounds);
    g.setColour (colours.border);
    g.drawRect (bounds);

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (colours.waveform.withAlpha (0.85f));
        thumbnail.drawChannels (g, bounds, 0.0, thumbnail.getTotalLength(), 1.0f);

        const auto progress = thumbnail.getTotalLength() > 0.0
            ? transportController.getPosition() / thumbnail.getTotalLength()
            : 0.0;

        const auto cursorX = bounds.getX() + static_cast<int> (progress * static_cast<double> (bounds.getWidth()));
        g.setColour (colours.lyricsHighlight.withAlpha (0.9f));
        g.drawLine (static_cast<float> (cursorX), static_cast<float> (bounds.getY()),
                    static_cast<float> (cursorX), static_cast<float> (bounds.getBottom()), 1.5f);
    }
}

void StemMiniWaveform::mouseDown (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    const auto bounds = getLocalBounds().reduced (1);
    const auto proportion = juce::jlimit (0.0f, 1.0f,
                                          (event.position.x - static_cast<float> (bounds.getX()))
                                              / static_cast<float> (bounds.getWidth()));

    transportController.setPosition (proportion * thumbnail.getTotalLength());
}

void StemMiniWaveform::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    repaint();
}

void StemMiniWaveform::timerCallback()
{
    repaint();
}

} // namespace jamstudio::ui