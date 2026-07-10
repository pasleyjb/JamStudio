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
    transportController.getStemMixer().addChangeListener (this);
    startTimerHz (30);
}

void WaveformDisplay::setSourceFile (const juce::File& file)
{
    thumbnail.clear();

    if (file.existsAsFile())
        thumbnail.setSource (new juce::FileInputSource (file));

    repaint();
}

void WaveformDisplay::clear()
{
    thumbnail.clear();
    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto colours = jamstudio::ui::JamStudioTheme::getColours();
    g.fillAll (colours.waveformBackground);

    auto bounds = getLocalBounds().reduced (2);

    // Panel chrome
    g.setColour (colours.panelBackground.brighter (0.03f));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);

    auto waveBounds = bounds.reduced (6, 8);

    // Subtle centre line
    g.setColour (colours.borderLight.withAlpha (0.25f));
    g.drawHorizontalLine (waveBounds.getCentreY(),
                          static_cast<float> (waveBounds.getX()),
                          static_cast<float> (waveBounds.getRight()));

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (colours.waveform.withAlpha (0.9f));
        thumbnail.drawChannels (g, waveBounds, 0.0, thumbnail.getTotalLength(), 1.0f);

        const auto progress = transportController.getPosition() / thumbnail.getTotalLength();
        const auto cursorX = waveBounds.getX()
            + static_cast<int> (juce::jlimit (0.0, 1.0, progress)
                                * static_cast<double> (waveBounds.getWidth()));

        // Playhead glow
        g.setColour (colours.lyricsHighlight.withAlpha (0.2f));
        g.fillRect (cursorX - 3, waveBounds.getY(), 6, waveBounds.getHeight());
        g.setColour (colours.lyricsHighlight);
        g.drawLine (static_cast<float> (cursorX), static_cast<float> (waveBounds.getY()),
                    static_cast<float> (cursorX), static_cast<float> (waveBounds.getBottom()), 2.0f);
    }
    else
    {
        g.setColour (colours.textSecondary);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Main waveform - open a song to begin",
                    waveBounds, juce::Justification::centred);
    }
}

void WaveformDisplay::resized()
{
    cursorArea = getLocalBounds();
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    const auto bounds = getLocalBounds().reduced (8, 10);
    const auto proportion = juce::jlimit (0.0f, 1.0f,
                                          (event.position.x - static_cast<float> (bounds.getX()))
                                              / static_cast<float> (bounds.getWidth()));

    transportController.setPosition (proportion * thumbnail.getTotalLength());
    repaint();
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    repaint();
}

void WaveformDisplay::timerCallback()
{
    repaint();
}

} // namespace jamstudio::ui
