#include "LyricsView.h"

namespace jamstudio::notation
{

LyricsView::LyricsView (jamstudio::audio::TransportController& transport)
    : transportController (transport)
{
    startTimerHz (15);
}

void LyricsView::setLyrics (const LyricsTrack& newLyrics)
{
    lyrics = newLyrics;
    lastActiveLine = -1;
    repaint();
}

void LyricsView::clear()
{
    lyrics.clear();
    lastActiveLine = -1;
    repaint();
}

void LyricsView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));

    if (lyrics.isEmpty())
    {
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Import an LRC file to display synced lyrics with any song",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto position = transportController.getPosition();
    const auto activeIndex = lyrics.getActiveLineIndex (position);
    auto bounds = getLocalBounds().reduced (12, 8);

    if (lyrics.getTitle().isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (lyrics.getTitle(), bounds.removeFromTop (16), juce::Justification::centredLeft);
    }

    auto content = bounds;

    for (int offset = -1; offset <= 1; ++offset)
    {
        const auto lineIndex = activeIndex + offset;

        if (! juce::isPositiveAndBelow (lineIndex, lyrics.getNumLines()))
            continue;

        if (const auto* line = lyrics.getLine (lineIndex))
        {
            const auto isActive = lineIndex == activeIndex;
            auto lineBounds = content.removeFromTop (isActive ? 34 : 22);

            if (isActive)
            {
                g.setColour (juce::Colour (0xffffcc00).withAlpha (0.2f));
                g.fillRoundedRectangle (lineBounds.toFloat().reduced (2.0f), 4.0f);
            }

            g.setColour (isActive ? juce::Colours::white : juce::Colours::white.withAlpha (0.45f));
            g.setFont (juce::FontOptions (isActive ? 18.0f : 13.0f, isActive ? juce::Font::bold : juce::Font::plain));
            g.drawText (line->text, lineBounds, juce::Justification::centredLeft);
        }
    }
}

void LyricsView::timerCallback()
{
    if (lyrics.isEmpty())
        return;

    const auto activeIndex = lyrics.getActiveLineIndex (transportController.getPosition());

    if (activeIndex != lastActiveLine)
    {
        lastActiveLine = activeIndex;
        repaint();
    }
}

} // namespace jamstudio::notation