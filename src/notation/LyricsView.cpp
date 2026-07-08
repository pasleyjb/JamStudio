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
    lastActiveWord = -1;
    repaint();
}

void LyricsView::clear()
{
    lyrics.clear();
    lastActiveLine = -1;
    lastActiveWord = -1;
    repaint();
}

void LyricsView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));

    if (lyrics.isEmpty())
    {
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Import LRC lyrics or use AI Lyrics to transcribe vocals",
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

            const auto fontSize = isActive ? 18.0f : 13.0f;
            const auto fontStyle = isActive ? juce::Font::bold : juce::Font::plain;
            g.setFont (juce::FontOptions (fontSize, fontStyle));

            if (isActive && lyrics.hasWordTimings() && ! line->words.empty())
            {
                const auto activeWordIndex = lyrics.getActiveWordIndex (lineIndex, position);
                auto x = static_cast<float> (lineBounds.getX() + 8);

                for (int wordIndex = 0; wordIndex < static_cast<int> (line->words.size()); ++wordIndex)
                {
                    const auto& word = line->words[static_cast<size_t> (wordIndex)];
                    const auto isActiveWord = wordIndex == activeWordIndex;

                    g.setColour (isActiveWord ? juce::Colour (0xffffcc00) : juce::Colours::white);
                    const auto wordWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), word.text);
                    g.drawText (word.text, static_cast<int> (x), lineBounds.getY(),
                                juce::roundToInt (wordWidth) + 6,
                                lineBounds.getHeight(), juce::Justification::centredLeft);

                    x += wordWidth + 8.0f;
                }
            }
            else
            {
                g.setColour (isActive ? juce::Colours::white : juce::Colours::white.withAlpha (0.45f));
                g.drawText (line->text, lineBounds, juce::Justification::centredLeft);
            }
        }
    }
}

void LyricsView::timerCallback()
{
    if (lyrics.isEmpty())
        return;

    const auto position = transportController.getPosition();
    const auto activeIndex = lyrics.getActiveLineIndex (position);
    const auto activeWord = lyrics.hasWordTimings()
        ? lyrics.getActiveWordIndex (activeIndex, position)
        : -1;

    if (activeIndex != lastActiveLine || activeWord != lastActiveWord)
    {
        lastActiveLine = activeIndex;
        lastActiveWord = activeWord;
        repaint();
    }
}

} // namespace jamstudio::notation