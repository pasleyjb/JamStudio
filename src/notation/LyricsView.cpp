#include "LyricsView.h"

#include <cmath>

namespace jamstudio::notation
{

LyricsView::LyricsView (jamstudio::audio::TransportController& transport)
    : transportController (transport)
{
    earlierButton.setTooltip ("Lyrics are late - shift timing earlier");
    laterButton.setTooltip ("Lyrics are early - shift timing later");
    earlierButton.onClick = [this] { nudgeSyncOffset (-0.5); };
    laterButton.onClick = [this] { nudgeSyncOffset (0.5); };

    offsetLabel.setJustificationType (juce::Justification::centred);
    offsetLabel.setColour (juce::Label::textColourId,
                           jamstudio::ui::JamStudioTheme::getColours().textSecondary);
    offsetLabel.setFont (juce::FontOptions (12.0f));

    hintLabel.setJustificationType (juce::Justification::centred);
    hintLabel.setColour (juce::Label::textColourId,
                         jamstudio::ui::JamStudioTheme::getColours().textSecondary);
    hintLabel.setFont (juce::FontOptions (11.0f));
    hintLabel.setText ("Sync: use - / + if lyrics lead or lag the song",
                       juce::dontSendNotification);

    addAndMakeVisible (earlierButton);
    addAndMakeVisible (laterButton);
    addAndMakeVisible (offsetLabel);
    addAndMakeVisible (hintLabel);

    startTimerHz (30);
    rebuildFromBase();
}

LyricsView::~LyricsView() = default;

void LyricsView::setLyrics (const LyricsTrack& newLyrics)
{
    baseLyrics = newLyrics;
    baseLyrics.sanitizeAll();
    baseLyrics.finalizeTiming();
    syncOffsetSeconds = 0.0;
    rebuildFromBase();
    lastActiveLine = -1;
    lastActiveWord = -1;
    updateActiveFromTransport();
    repaint();
}

void LyricsView::clear()
{
    baseLyrics.clear();
    lyrics.clear();
    syncOffsetSeconds = 0.0;
    lastActiveLine = -1;
    lastActiveWord = -1;
    rebuildFromBase();
    repaint();
}

void LyricsView::setSyncOffset (const double offsetSeconds)
{
    syncOffsetSeconds = juce::jlimit (-30.0, 30.0, offsetSeconds);
    rebuildFromBase();
    updateActiveFromTransport();
    repaint();
}

void LyricsView::nudgeSyncOffset (const double deltaSeconds)
{
    setSyncOffset (syncOffsetSeconds + deltaSeconds);
}

void LyricsView::rebuildFromBase()
{
    lyrics = baseLyrics;

    if (std::abs (syncOffsetSeconds) > 1.0e-9)
        lyrics.applyTimeOffset (syncOffsetSeconds);

    if (std::abs (syncOffsetSeconds) < 0.05)
        offsetLabel.setText ("in sync", juce::dontSendNotification);
    else if (syncOffsetSeconds > 0.0)
        offsetLabel.setText ("+" + juce::String (syncOffsetSeconds, 1) + "s",
                             juce::dontSendNotification);
    else
        offsetLabel.setText (juce::String (syncOffsetSeconds, 1) + "s",
                             juce::dontSendNotification);

    const auto hasLyrics = ! lyrics.isEmpty();
    earlierButton.setEnabled (hasLyrics);
    laterButton.setEnabled (hasLyrics);
    earlierButton.setVisible (hasLyrics);
    laterButton.setVisible (hasLyrics);
    offsetLabel.setVisible (hasLyrics);
    hintLabel.setVisible (hasLyrics);
}

void LyricsView::paint (juce::Graphics& g)
{
    const auto colours = jamstudio::ui::JamStudioTheme::getColours();
    g.fillAll (colours.lyricsBackground);
    g.setColour (colours.border.withAlpha (0.5f));
    g.drawRect (getLocalBounds(), 1);

    if (lyrics.isEmpty())
    {
        g.setColour (colours.textSecondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Lyrics ready - import LRC, online lyrics, or AI Lyrics",
                    getLocalBounds().reduced (16), juce::Justification::centred);
        return;
    }

    // Compact karaoke: previous / current / next (2-3 lines only).
    auto area = getLocalBounds().reduced (12, 8);
    area.removeFromBottom (28); // room for sync controls

    const auto active = lastActiveLine;
    const int prev = active > 0 ? active - 1 : -1;
    const int next = (active >= 0 && active + 1 < lyrics.getNumLines()) ? active + 1
                     : (active < 0 && lyrics.getNumLines() > 0 ? 0 : -1);

    const auto rowH = juce::jmax (28, area.getHeight() / 3);
    auto top = area.removeFromTop (rowH);
    auto mid = area.removeFromTop (rowH);
    auto bot = area.removeFromTop (rowH);

    if (const auto* line = lyrics.getLine (prev))
        paintKaraokeLine (g, top, line, false, -1);

    if (active >= 0)
    {
        // Glow behind current line
        g.setColour (colours.lyricsHighlight.withAlpha (0.14f));
        g.fillRoundedRectangle (mid.toFloat().expanded (4.0f, 2.0f), 8.0f);
        paintKaraokeLine (g, mid, lyrics.getLine (active), true, lastActiveWord);
    }
    else
    {
        g.setColour (colours.textSecondary.withAlpha (0.7f));
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("... waiting for vocals ...", mid, juce::Justification::centred);
    }

    if (const auto* line = lyrics.getLine (next >= 0 ? next : (active < 0 ? 0 : -1)))
    {
        if (active >= 0 || next >= 0)
            paintKaraokeLine (g, bot, line, false, -1);
    }
}

void LyricsView::paintKaraokeLine (juce::Graphics& g,
                                   juce::Rectangle<int> bounds,
                                   const LyricLine* line,
                                   const bool isActive,
                                   const int activeWord) const
{
    if (line == nullptr)
        return;

    const auto colours = jamstudio::ui::JamStudioTheme::getColours();
    g.setFont (juce::FontOptions (isActive ? 22.0f : 14.0f,
                                  isActive ? juce::Font::bold : juce::Font::plain));

    if (isActive && lyrics.hasWordTimings() && ! line->words.empty())
    {
        float totalWidth = 0.0f;
        juce::Array<float> wordWidths;

        for (const auto& word : line->words)
        {
            const auto w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), word.text);
            wordWidths.add (w);
            totalWidth += w + 10.0f;
        }

        auto x = static_cast<float> (bounds.getCentreX()) - totalWidth * 0.5f;

        for (int wordIndex = 0; wordIndex < static_cast<int> (line->words.size()); ++wordIndex)
        {
            const auto& word = line->words[static_cast<size_t> (wordIndex)];
            const auto isActiveWord = wordIndex == activeWord;
            const auto wordWidth = wordWidths[wordIndex];

            if (isActiveWord)
            {
                g.setColour (colours.lyricsHighlight.withAlpha (0.35f));
                g.fillRoundedRectangle (x - 4.0f,
                                        static_cast<float> (bounds.getY() + 4),
                                        wordWidth + 10.0f,
                                        static_cast<float> (bounds.getHeight() - 8),
                                        4.0f);
                g.setColour (colours.lyricsHighlight);
            }
            else
            {
                g.setColour (colours.text.withAlpha (0.88f));
            }

            g.drawText (word.text, static_cast<int> (x), bounds.getY(),
                        juce::roundToInt (wordWidth) + 8, bounds.getHeight(),
                        juce::Justification::centredLeft);
            x += wordWidth + 10.0f;
        }
    }
    else
    {
        g.setColour (isActive ? colours.lyricsHighlight
                              : colours.textSecondary.withAlpha (isActive ? 1.0f : 0.75f));
        g.drawText (line->text, bounds, juce::Justification::centred);
    }
}

void LyricsView::resized()
{
    auto bar = getLocalBounds().removeFromBottom (28).reduced (8, 2);
    earlierButton.setBounds (bar.removeFromLeft (64).reduced (2));
    laterButton.setBounds (bar.removeFromRight (64).reduced (2));
    offsetLabel.setBounds (bar.removeFromLeft (72));
    hintLabel.setBounds (bar);
}

void LyricsView::timerCallback()
{
    updateActiveFromTransport();
}

void LyricsView::updateActiveFromTransport()
{
    if (lyrics.isEmpty())
        return;

    const auto position = transportController.getPosition();
    const auto activeLine = lyrics.getActiveLineIndex (position);
    const auto activeWord = lyrics.hasWordTimings()
        ? lyrics.getActiveWordIndex (activeLine, position)
        : -1;

    if (activeLine != lastActiveLine || activeWord != lastActiveWord)
    {
        lastActiveLine = activeLine;
        lastActiveWord = activeWord;
        repaint();
    }
}

} // namespace jamstudio::notation
