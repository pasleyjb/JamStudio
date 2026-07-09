#include "NotationView.h"

#include "../ui/JamStudioTheme.h"

#include <cmath>

namespace jamstudio::notation
{

NotationView::NotationView (jamstudio::audio::TransportController& transport)
    : transportController (transport)
{
    startTimerHz (30);
}

void NotationView::setScore (const Score& newScore)
{
    score = newScore;
    lastHighlightedMeasure = -1;
    lastHighlightedBeat = -1.0;
    lastScrolledBeat = -1.0;
    updateContentSize();
    repaint();
}

void NotationView::clear()
{
    score.clear();
    lastHighlightedMeasure = -1;
    lastHighlightedBeat = -1.0;
    lastScrolledBeat = -1.0;
    updateContentSize();
    repaint();
}

void NotationView::setLayoutMode (const LayoutMode mode)
{
    layoutMode = mode;
    updateContentSize();
    repaint();
}

void NotationView::setPrintFriendly (const bool shouldBePrintFriendly)
{
    printFriendly = shouldBePrintFriendly;
    repaint();
}

void NotationView::setFollowPlayback (const bool shouldFollow)
{
    followPlayback = shouldFollow;
}

int NotationView::getMeasuresPerRow() const noexcept
{
    if (layoutMode == LayoutMode::horizontalStrip)
        return juce::jmax (1, score.getNumMeasures());

    // ~ letter page width when full-page (~4 measures across at 180px)
    const auto available = juce::jmax (measureWidth, getWidth() > 0 ? getWidth() - pageMargin * 2 : 720);
    return juce::jmax (1, available / measureWidth);
}

int NotationView::getContentWidth() const noexcept
{
    if (score.isEmpty())
        return 400;

    if (layoutMode == LayoutMode::horizontalStrip)
        return score.getNumMeasures() * measureWidth + 40;

    const auto cols = getMeasuresPerRow();
    return cols * measureWidth + pageMargin * 2;
}

int NotationView::getContentHeight() const noexcept
{
    if (score.isEmpty())
        return 200;

    const auto rowH = measureHeight + (score.hasLyrics() ? lyricRowHeight : 0) + rowGap;

    if (layoutMode == LayoutMode::horizontalStrip)
        return rowH + 28;

    const auto cols = getMeasuresPerRow();
    const auto rows = juce::jmax (1, (score.getNumMeasures() + cols - 1) / cols);
    return pageMargin + 28 + rows * rowH + pageMargin;
}

juce::Rectangle<int> NotationView::getMeasureBounds (const int measureIndex) const noexcept
{
    if (score.isEmpty() || ! juce::isPositiveAndBelow (measureIndex, score.getNumMeasures()))
        return {};

    const auto rowH = measureHeight + (score.hasLyrics() ? lyricRowHeight : 0) + rowGap;

    if (layoutMode == LayoutMode::horizontalStrip)
        return { 12 + measureIndex * measureWidth, 28, measureWidth - 8, measureHeight };

    const auto cols = getMeasuresPerRow();
    const auto row = measureIndex / cols;
    const auto col = measureIndex % cols;
    return { pageMargin + col * measureWidth,
             pageMargin + 28 + row * rowH,
             measureWidth - 8,
             measureHeight };
}

void NotationView::updateContentSize()
{
    setSize (juce::jmax (getWidth(), getContentWidth()),
             juce::jmax (getHeight(), getContentHeight()));
}

void NotationView::paint (juce::Graphics& g)
{
    paintScore (g, printFriendly);
}

void NotationView::paintScore (juce::Graphics& g, const bool forPrint) const
{
    const auto colours = jamstudio::ui::JamStudioTheme::getColours();

    if (forPrint)
        g.fillAll (juce::Colours::white);
    else
        g.fillAll (colours.notationBackground);

    if (score.isEmpty())
    {
        g.setColour (forPrint ? juce::Colours::black : colours.textSecondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Notation ready — import MusicXML, browse the library, or run AI Tab.\n"
                    "Use the Part menu and Tab/Sheet buttons to choose what to view.",
                    getLocalBounds().reduced (16), juce::Justification::centred);
        return;
    }

    g.setColour (forPrint ? juce::Colours::black : colours.text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    const auto titleY = layoutMode == LayoutMode::fullPageRows ? pageMargin : 4;
    g.drawText (score.getTitle() + "  —  " + score.getActivePart().name,
                pageMargin, titleY, getWidth() - pageMargin * 2, 20,
                juce::Justification::centredLeft);

    const auto position = transportController.getPosition();
    const auto activeMeasure = forPrint ? -1 : score.getMeasureIndexAtTime (position);
    const auto* activeLyricNote = forPrint ? nullptr : score.getActiveLyricNoteAtTime (position);

    for (int i = 0; i < score.getNumMeasures(); ++i)
    {
        if (const auto* measure = score.getMeasure (i))
        {
            const auto bounds = getMeasureBounds (i);
            drawMeasure (g, *measure, bounds, i == activeMeasure, activeLyricNote, forPrint);
        }
    }
}

juce::Image NotationView::renderToImage (const float scale) const
{
    const auto w = juce::jmax (1, getContentWidth());
    const auto h = juce::jmax (1, getContentHeight());
    const auto imgW = juce::roundToInt (static_cast<float> (w) * scale);
    const auto imgH = juce::roundToInt (static_cast<float> (h) * scale);

    juce::Image image (juce::Image::RGB, imgW, imgH, true);
    juce::Graphics g (image);
    g.fillAll (juce::Colours::white);
    g.addTransform (juce::AffineTransform::scale (scale));
    paintScore (g, true);
    return image;
}

void NotationView::resized()
{
    if (! score.isEmpty())
        updateContentSize();
}

void NotationView::timerCallback()
{
    if (score.isEmpty() || ! followPlayback)
        return;

    const auto position = transportController.getPosition();
    const auto activeMeasure = score.getMeasureIndexAtTime (position);
    const auto currentBeat = score.secondsToBeats (position);
    const auto beatChanged = std::abs (currentBeat - lastHighlightedBeat) > 0.02;
    const auto scrollChanged = std::abs (currentBeat - lastScrolledBeat) > 0.03;

    if (transportController.isPlaying() || scrollChanged)
    {
        lastScrolledBeat = currentBeat;
        scrollToBeat (currentBeat);
    }

    if (beatChanged || activeMeasure != lastHighlightedMeasure)
    {
        lastHighlightedMeasure = activeMeasure;
        lastHighlightedBeat = currentBeat;
        repaint();
    }
}

void NotationView::scrollToBeat (const double beat)
{
    auto* viewport = findParentComponentOfClass<juce::Viewport>();

    if (viewport == nullptr)
        return;

    const auto measureIndex = score.getMeasureIndexAtTime (transportController.getPosition());
    const auto bounds = getMeasureBounds (measureIndex >= 0 ? measureIndex : 0);

    if (layoutMode == LayoutMode::horizontalStrip)
    {
        const auto playheadX = static_cast<int> (score.getXPositionForBeat (beat, measureWidth));
        const auto targetX = juce::jmax (0, playheadX - viewport->getViewWidth() / 3);
        viewport->setViewPosition (targetX, viewport->getViewPositionY());
        return;
    }

    // Full-page rows: keep the active row near the top third of the viewport.
    const auto targetY = juce::jmax (0, bounds.getY() - viewport->getViewHeight() / 4);
    viewport->setViewPosition (0, targetY);
}

void NotationView::drawMeasure (juce::Graphics& g,
                                const Measure& measure,
                                const juce::Rectangle<int> bounds,
                                const bool isActive,
                                const NoteEvent* activeLyricNote,
                                const bool forPrint) const
{
    if (forPrint)
    {
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (bounds.toFloat(), 2.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (bounds.toFloat(), 2.0f, 1.0f);
    }
    else
    {
        g.setColour (isActive ? juce::Colour (0xff2f4f78) : juce::Colour (0xff242424));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xff505050));
        g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);
    }

    auto measureBounds = bounds;

    g.setColour (forPrint ? juce::Colours::black : juce::Colours::white.withAlpha (0.8f));
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
            drawTabNote (g, note, noteArea, x, forPrint);
        else
            drawStandardNote (g, note, noteArea, x, forPrint);

        if (note.isTuplet)
        {
            g.setColour (forPrint ? juce::Colours::darkgrey : juce::Colours::orange.withAlpha (0.8f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("3", x - 4, noteArea.getY() + 2, 12, 10, juce::Justification::centred);
        }

        if (note.lyricText.isNotEmpty())
        {
            const auto isLyricActive = ! forPrint && activeLyricNote != nullptr
                                       && std::abs (activeLyricNote->startBeat - note.startBeat) < 0.001;
            drawLyric (g, note, bounds, x, isLyricActive, forPrint);
        }

        const auto spacing = juce::jlimit (14, 40, static_cast<int> (note.durationBeats * 8.0));
        x += spacing;
    }
}

void NotationView::drawLyric (juce::Graphics& g,
                              const NoteEvent& note,
                              const juce::Rectangle<int> bounds,
                              const int x,
                              const bool isActive,
                              const bool forPrint) const
{
    auto lyricArea = bounds.withTrimmedTop (bounds.getHeight() - lyricRowHeight).reduced (4, 0);

    if (isActive && ! forPrint)
    {
        g.setColour (juce::Colour (0xffffcc00).withAlpha (0.35f));
        g.fillRoundedRectangle (static_cast<float> (x - 8), static_cast<float> (lyricArea.getY()),
                                28.0f, static_cast<float> (lyricArea.getHeight()), 3.0f);
    }

    g.setColour (forPrint ? juce::Colours::black
                          : (isActive ? juce::Colours::white : juce::Colours::white.withAlpha (0.75f)));
    g.setFont (juce::FontOptions (isActive ? 13.0f : 11.0f, isActive ? juce::Font::bold : juce::Font::plain));
    g.drawText (note.lyricText, x - 10, lyricArea.getY(), 36, lyricArea.getHeight(), juce::Justification::centred);
}

void NotationView::drawStandardNote (juce::Graphics& g,
                                     const NoteEvent& note,
                                     const juce::Rectangle<int> bounds,
                                     const int x,
                                     const bool forPrint) const
{
    const auto staffTop = bounds.getY() + 20;
    const auto lineSpacing = 8;
    const auto lineColour = forPrint ? juce::Colours::black : juce::Colour (0xff606060);

    for (int line = 0; line < 5; ++line)
    {
        g.setColour (lineColour);
        g.drawHorizontalLine (staffTop + line * lineSpacing, static_cast<float> (bounds.getX()),
                              static_cast<float> (bounds.getRight()));
    }

    if (note.isRest)
    {
        g.setColour (forPrint ? juce::Colours::black : juce::Colours::white.withAlpha (0.7f));
        g.drawText ("r", x - 6, staffTop + 8, 20, 16, juce::Justification::centred);
        return;
    }

    const auto pitchOffset = note.midiPitch >= 0 ? (note.midiPitch % 12) : 0;
    const auto y = staffTop + (4 * lineSpacing) - (pitchOffset * lineSpacing / 3);

    g.setColour (forPrint ? juce::Colours::black : juce::Colours::white);
    g.fillEllipse (static_cast<float> (x - 5), static_cast<float> (y - 5), 10.0f, 10.0f);

    g.setFont (juce::FontOptions (10.0f));
    g.drawText (note.label, x - 12, y - 24, 30, 14, juce::Justification::centred);
}

void NotationView::drawTabNote (juce::Graphics& g,
                                const NoteEvent& note,
                                const juce::Rectangle<int> bounds,
                                const int x,
                                const bool forPrint) const
{
    const auto tabTop = bounds.getY() + 18;
    const auto lineSpacing = 12;
    const auto lineColour = forPrint ? juce::Colours::black : juce::Colour (0xff606060);

    for (int line = 0; line < 6; ++line)
    {
        g.setColour (lineColour);
        g.drawHorizontalLine (tabTop + line * lineSpacing, static_cast<float> (bounds.getX()),
                              static_cast<float> (bounds.getRight()));
    }

    if (note.isRest)
        return;

    const auto stringIndex = juce::jlimit (1, 6, note.stringNumber);
    const auto y = tabTop + (stringIndex - 1) * lineSpacing;

    g.setColour (forPrint ? juce::Colours::black : juce::Colours::white);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (note.label, x - 8, y - 8, 20, 16, juce::Justification::centred);
}

} // namespace jamstudio::notation
