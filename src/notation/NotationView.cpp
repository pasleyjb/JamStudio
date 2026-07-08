#include "NotationView.h"

#include "../ui/JamStudioTheme.h"

namespace jamstudio::notation
{

NotationView::NotationView (jamstudio::audio::TransportController& transport)
    : transportController (transport)
{
    startTimerHz (15);
}

void NotationView::setScore (const Score& newScore)
{
    score = newScore;
    lastHighlightedMeasure = -1;
    lastHighlightedBeat = -1.0;
    setSize (juce::jmax (getWidth(), score.getNumMeasures() * measureWidth + 40), getContentHeight());
    repaint();
}

void NotationView::clear()
{
    score.clear();
    lastHighlightedMeasure = -1;
    lastHighlightedBeat = -1.0;
    repaint();
}

int NotationView::getContentHeight() const noexcept
{
    return measureHeight + (score.hasLyrics() ? lyricRowHeight : 0) + 20;
}

void NotationView::paint (juce::Graphics& g)
{
    const auto colours = jamstudio::ui::JamStudioTheme::getColours();
    g.fillAll (colours.notationBackground);

    if (score.isEmpty())
    {
        g.setColour (colours.textSecondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Import a MusicXML file to display synced notation and lyrics",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour (colours.text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (score.getTitle(), 12, 4, getWidth() - 24, 20, juce::Justification::centredLeft);

    const auto position = transportController.getPosition();
    const auto activeMeasure = score.getMeasureIndexAtTime (position);
    const auto* activeLyricNote = score.getActiveLyricNoteAtTime (position);

    auto x = 12;

    for (int i = 0; i < score.getNumMeasures(); ++i)
    {
        if (const auto* measure = score.getMeasure (i))
        {
            const auto bounds = juce::Rectangle<int> (x, 24, measureWidth - 8, measureHeight);
            drawMeasure (g, *measure, bounds, i == activeMeasure, activeLyricNote);
            x += measureWidth;
        }
    }
}

void NotationView::resized()
{
    if (! score.isEmpty())
        setSize (juce::jmax (getWidth(), score.getNumMeasures() * measureWidth + 40), getContentHeight());
}

void NotationView::timerCallback()
{
    if (score.isEmpty())
        return;

    const auto position = transportController.getPosition();
    const auto activeMeasure = score.getMeasureIndexAtTime (position);
    const auto currentBeat = score.secondsToBeats (position);
    const auto beatChanged = std::abs (currentBeat - lastHighlightedBeat) > 0.01;

    if (activeMeasure != lastHighlightedMeasure)
    {
        lastHighlightedMeasure = activeMeasure;
        scrollToMeasure (activeMeasure);
    }

    if (beatChanged || activeMeasure != lastHighlightedMeasure)
    {
        lastHighlightedBeat = currentBeat;
        repaint();
    }
}

void NotationView::scrollToMeasure (const int measureIndex)
{
    if (auto* parent = getParentComponent())
    {
        if (auto* viewport = dynamic_cast<juce::Viewport*> (parent))
        {
            const auto targetX = juce::jmax (0, measureIndex * measureWidth - viewport->getWidth() / 3);
            viewport->setViewPosition (targetX, 0);
        }
    }
}

void NotationView::drawMeasure (juce::Graphics& g,
                                const Measure& measure,
                                const juce::Rectangle<int> bounds,
                                const bool isActive,
                                const NoteEvent* activeLyricNote) const
{
    g.setColour (isActive ? juce::Colour (0xff2f4f78) : juce::Colour (0xff242424));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    g.setColour (juce::Colour (0xff505050));
    g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);

    auto measureBounds = bounds;

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (juce::String (measure.number), measureBounds.removeFromTop (18), juce::Justification::centred);

    auto noteArea = measureBounds;

    if (score.hasLyrics())
        noteArea.removeFromBottom (lyricRowHeight);

    noteArea = noteArea.reduced (6, 4);
    auto x = noteArea.getX() + 8;

    for (const auto& note : measure.notes)
    {
        if (score.getNotationMode() == NotationMode::tab)
            drawTabNote (g, note, noteArea, x);
        else
            drawStandardNote (g, note, noteArea, x);

        if (note.isTuplet)
        {
            g.setColour (juce::Colours::orange.withAlpha (0.8f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("3", x - 4, noteArea.getY() + 2, 12, 10, juce::Justification::centred);
        }

        if (note.lyricText.isNotEmpty())
        {
            const auto isLyricActive = activeLyricNote != nullptr && activeLyricNote->startBeat == note.startBeat;
            drawLyric (g, note, bounds, x, isLyricActive);
        }

        const auto spacing = juce::jlimit (14, 40, static_cast<int> (note.durationBeats * 8.0));
        x += spacing;
    }
}

void NotationView::drawLyric (juce::Graphics& g,
                              const NoteEvent& note,
                              const juce::Rectangle<int> bounds,
                              const int x,
                              const bool isActive) const
{
    auto lyricArea = bounds.withTrimmedTop (bounds.getHeight() - lyricRowHeight).reduced (4, 0);

    if (isActive)
    {
        g.setColour (juce::Colour (0xffffcc00).withAlpha (0.35f));
        g.fillRoundedRectangle (static_cast<float> (x - 8), static_cast<float> (lyricArea.getY()),
                                28.0f, static_cast<float> (lyricArea.getHeight()), 3.0f);
    }

    g.setColour (isActive ? juce::Colours::white : juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::FontOptions (isActive ? 13.0f : 11.0f, isActive ? juce::Font::bold : juce::Font::plain));
    g.drawText (note.lyricText, x - 10, lyricArea.getY(), 36, lyricArea.getHeight(), juce::Justification::centred);
}

void NotationView::drawStandardNote (juce::Graphics& g,
                                     const NoteEvent& note,
                                     const juce::Rectangle<int> bounds,
                                     const int x) const
{
    const auto staffTop = bounds.getY() + 20;
    const auto lineSpacing = 8;

    for (int line = 0; line < 5; ++line)
    {
        g.setColour (juce::Colour (0xff606060));
        g.drawHorizontalLine (staffTop + line * lineSpacing, static_cast<float> (bounds.getX()),
                              static_cast<float> (bounds.getRight()));
    }

    if (note.isRest)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText ("r", x - 6, staffTop + 8, 20, 16, juce::Justification::centred);
        return;
    }

    const auto pitchOffset = note.midiPitch >= 0 ? (note.midiPitch % 12) : 0;
    const auto y = staffTop + (4 * lineSpacing) - (pitchOffset * lineSpacing / 3);

    g.setColour (juce::Colours::white);
    g.fillEllipse (static_cast<float> (x - 5), static_cast<float> (y - 5), 10.0f, 10.0f);

    g.setFont (juce::FontOptions (10.0f));
    g.drawText (note.label, x - 12, y - 24, 30, 14, juce::Justification::centred);
}

void NotationView::drawTabNote (juce::Graphics& g,
                                const NoteEvent& note,
                                const juce::Rectangle<int> bounds,
                                const int x) const
{
    const auto tabTop = bounds.getY() + 18;
    const auto lineSpacing = 12;

    for (int line = 0; line < 6; ++line)
    {
        g.setColour (juce::Colour (0xff606060));
        g.drawHorizontalLine (tabTop + line * lineSpacing, static_cast<float> (bounds.getX()),
                              static_cast<float> (bounds.getRight()));
    }

    if (note.isRest)
        return;

    const auto stringIndex = juce::jlimit (1, 6, note.stringNumber);
    const auto y = tabTop + (stringIndex - 1) * lineSpacing;

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (note.label, x - 8, y - 8, 20, 16, juce::Justification::centred);
}

} // namespace jamstudio::notation