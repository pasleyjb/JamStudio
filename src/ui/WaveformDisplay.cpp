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
    g.setColour (colours.border);
    g.drawRect (bounds);

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour (colours.waveform);
        thumbnail.drawChannels (g, bounds, 0.0, thumbnail.getTotalLength(), 1.0f);

        const auto progress = thumbnail.getTotalLength() > 0.0
            ? transportController.getPosition() / thumbnail.getTotalLength()
            : 0.0;

        const auto cursorX = bounds.getX() + static_cast<int> (progress * static_cast<double> (bounds.getWidth()));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (static_cast<float> (cursorX), static_cast<float> (bounds.getY()),
                    static_cast<float> (cursorX), static_cast<float> (bounds.getBottom()), 2.0f);
    }
    else
    {
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Waveform will appear when a song is loaded",
                    bounds, juce::Justification::centred);
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

    const auto bounds = getLocalBounds().reduced (2);
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