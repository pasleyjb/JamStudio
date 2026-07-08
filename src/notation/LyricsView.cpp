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
            g.setFont (juce::FontOptions (13.0f));
            g.drawText ("Import LRC lyrics or use AI Lyrics from the Lyrics menu / toolbar",
                        getLocalBounds(), juce::Justification::centred);
            return;
        }

        auto y = 8;

        if (lyrics.getTitle().isNotEmpty())
        {
            g.setColour (colours.textSecondary);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (lyrics.getTitle(), 12, y, getWidth() - 24, 18, juce::Justification::centredLeft);
            y += 22;
        }

        for (int lineIndex = 0; lineIndex < lyrics.getNumLines(); ++lineIndex)
        {
            if (const auto* line = lyrics.getLine (lineIndex))
            {
                const auto isActive = lineIndex == activeLine;
                const auto lineHeight = isActive ? 34 : 24;
                auto lineBounds = juce::Rectangle<int> (8, y, getWidth() - 16, lineHeight);

                if (isActive)
                {
                    g.setColour (colours.lyricsHighlight.withAlpha (0.18f));
                    g.fillRoundedRectangle (lineBounds.toFloat().reduced (2.0f), 4.0f);
                }

                g.setFont (juce::FontOptions (isActive ? 18.0f : 14.0f,
                                              isActive ? juce::Font::bold : juce::Font::plain));
                g.setColour (isActive ? colours.text : colours.textSecondary);

                if (isActive && lyrics.hasWordTimings() && ! line->words.empty())
                {
                    auto x = static_cast<float> (lineBounds.getX() + 8);

                    for (int wordIndex = 0; wordIndex < static_cast<int> (line->words.size()); ++wordIndex)
                    {
                        const auto& word = line->words[static_cast<size_t> (wordIndex)];
                        const auto isActiveWord = wordIndex == activeWord;
                        g.setColour (isActiveWord ? colours.lyricsHighlight : colours.text);
                        const auto wordWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), word.text);
                        g.drawText (word.text, static_cast<int> (x), lineBounds.getY(),
                                    juce::roundToInt (wordWidth) + 6, lineBounds.getHeight(),
                                    juce::Justification::centredLeft);
                        x += wordWidth + 8.0f;
                    }
                }
                else
                {
                    g.drawText (line->text, lineBounds, juce::Justification::centredLeft);
                }

                y += lineHeight + 4;
            }
        }
    }

    void resized() override
    {
        const auto lineCount = juce::jmax (1, lyrics.getNumLines());
        setSize (getWidth(), 40 + lineCount * 30 + (lyrics.getTitle().isNotEmpty() ? 22 : 0));
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
    startTimerHz (20);
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
    g.fillAll (jamstudio::ui::JamStudioTheme::getColours().lyricsBackground);
}

void LyricsView::resized()
{
    viewport.setBounds (getLocalBounds());
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

    const auto lineHeight = 30;
    const auto targetY = juce::jmax (0, lastActiveLine * lineHeight - getHeight() / 3);
    viewport.setViewPosition (0, targetY);
}

} // namespace jamstudio::notation