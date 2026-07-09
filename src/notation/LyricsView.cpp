#include "LyricsView.h"

namespace jamstudio::notation
{

class LyricsView::LyricsContent : public juce::Component
{
public:
    explicit LyricsContent (LyricsTrack& trackToDisplay) : lyrics (trackToDisplay) {}

    void setActiveIndices (const int lineIndex, const int wordIndex)
    {
        activeLine = lineIndex;
        activeWord = wordIndex;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = jamstudio::ui::JamStudioTheme::getColours();
        g.fillAll (colours.lyricsBackground);

        if (lyrics.isEmpty())
        {
            g.setColour (colours.textSecondary);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("Lyrics ready — import LRC or run AI Lyrics after loading a song",
                        getLocalBounds().reduced (16), juce::Justification::centred);
            return;
        }

        auto y = 12;

        if (lyrics.getTitle().isNotEmpty())
        {
            g.setColour (colours.textSecondary);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (lyrics.getTitle(), 16, y, getWidth() - 32, 18, juce::Justification::centred);
            y += 26;
        }

        for (int lineIndex = 0; lineIndex < lyrics.getNumLines(); ++lineIndex)
        {
            if (const auto* line = lyrics.getLine (lineIndex))
            {
                const auto isActive = lineIndex == activeLine;
                const auto lineHeight = isActive ? 42 : 26;
                auto lineBounds = juce::Rectangle<int> (12, y, getWidth() - 24, lineHeight);

                if (isActive)
                {
                    // Soft glow behind the active line
                    g.setColour (colours.lyricsHighlight.withAlpha (0.12f));
                    g.fillRoundedRectangle (lineBounds.toFloat().expanded (4.0f, 2.0f), 8.0f);
                    g.setColour (colours.lyricsHighlight.withAlpha (0.22f));
                    g.fillRoundedRectangle (lineBounds.toFloat(), 6.0f);
                }

                g.setFont (juce::FontOptions (isActive ? 22.0f : 15.0f,
                                              isActive ? juce::Font::bold : juce::Font::plain));

                if (isActive && lyrics.hasWordTimings() && ! line->words.empty())
                {
                    // Centre the karaoke line as a group
                    float totalWidth = 0.0f;
                    juce::Array<float> wordWidths;

                    for (const auto& word : line->words)
                    {
                        const auto w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), word.text);
                        wordWidths.add (w);
                        totalWidth += w + 10.0f;
                    }

                    auto x = static_cast<float> (lineBounds.getCentreX()) - totalWidth * 0.5f;

                    for (int wordIndex = 0; wordIndex < static_cast<int> (line->words.size()); ++wordIndex)
                    {
                        const auto& word = line->words[static_cast<size_t> (wordIndex)];
                        const auto isActiveWord = wordIndex == activeWord;
                        const auto wordWidth = wordWidths[wordIndex];

                        if (isActiveWord)
                        {
                            g.setColour (colours.lyricsHighlight.withAlpha (0.35f));
                            g.fillRoundedRectangle (x - 4.0f,
                                                    static_cast<float> (lineBounds.getY() + 4),
                                                    wordWidth + 10.0f,
                                                    static_cast<float> (lineBounds.getHeight() - 8),
                                                    4.0f);
                            g.setColour (colours.lyricsHighlight);
                        }
                        else
                        {
                            g.setColour (colours.text.withAlpha (0.88f));
                        }

                        g.drawText (word.text, static_cast<int> (x), lineBounds.getY(),
                                    juce::roundToInt (wordWidth) + 8, lineBounds.getHeight(),
                                    juce::Justification::centredLeft);
                        x += wordWidth + 10.0f;
                    }
                }
                else
                {
                    g.setColour (isActive ? colours.lyricsHighlight : colours.textSecondary);
                    g.drawText (line->text, lineBounds, juce::Justification::centred);
                }

                y += lineHeight + 6;
            }
        }
    }

    void resized() override
    {
        const auto lineCount = juce::jmax (1, lyrics.getNumLines());
        setSize (getWidth(), 48 + lineCount * 36 + (lyrics.getTitle().isNotEmpty() ? 26 : 0));
    }

private:
    LyricsTrack& lyrics;
    int activeLine = -1;
    int activeWord = -1;
};

LyricsView::LyricsView (jamstudio::audio::TransportController& transport)
    : transportController (transport),
      content (std::make_unique<LyricsContent> (lyrics))
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    startTimerHz (30);
}

LyricsView::~LyricsView() = default;

void LyricsView::setLyrics (const LyricsTrack& newLyrics)
{
    lyrics = newLyrics;
    lastActiveLine = -1;
    lastActiveWord = -1;
    content->resized();
    content->repaint();
    scrollToActiveLine();
}

void LyricsView::clear()
{
    lyrics.clear();
    lastActiveLine = -1;
    lastActiveWord = -1;
    content->resized();
    content->repaint();
}

void LyricsView::paint (juce::Graphics& g)
{
    const auto colours = jamstudio::ui::JamStudioTheme::getColours();
    g.fillAll (colours.lyricsBackground);
    g.setColour (colours.border.withAlpha (0.5f));
    g.drawRect (getLocalBounds(), 1);
}

void LyricsView::resized()
{
    viewport.setBounds (getLocalBounds().reduced (1));
    content->setSize (viewport.getMaximumVisibleWidth(), content->getHeight());
}

void LyricsView::timerCallback()
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
        content->setActiveIndices (activeLine, activeWord);
        scrollToActiveLine();
    }
}

void LyricsView::scrollToActiveLine()
{
    if (lastActiveLine < 0 || ! content)
        return;

    const auto lineHeight = 36;
    const auto targetY = juce::jmax (0, lastActiveLine * lineHeight - getHeight() / 3);
    viewport.setViewPosition (0, targetY);
}

} // namespace jamstudio::notation
