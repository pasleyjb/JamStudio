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

void NotationView::setStripViewportHeight (const int heightPixels)
{
    const auto h = juce::jmax (0, heightPixels);

    if (stripViewportHeight == h)
        return;

    stripViewportHeight = h;
    updateContentSize();
    repaint();
}

float NotationView::displayScale() const noexcept
{
    if (layoutMode == LayoutMode::fullPageRows || printFriendly)
        return 1.0f;

    // Scale tab strip into the space *below* the score title (never over it).
    const auto target = stripViewportHeight > 0 ? stripViewportHeight : getHeight();

    if (target <= 0)
        return 1.0f;

    const auto lyrics = score.hasLyrics() ? baseLyricRowHeight : 0;
    // Leave a clear band for "Song - Part" title at the top of the strip.
    const auto chrome = 40; // title row + padding under it
    const auto idealBody = juce::jmax (baseMeasureHeight, target - chrome - lyrics);
    // Cap so frets stay large but don't eat the title (was up to 2.4x).
    return juce::jlimit (1.0f, 1.75f, static_cast<float> (idealBody) / static_cast<float> (baseMeasureHeight));
}

int NotationView::measureWidthPx() const noexcept
{
    return juce::roundToInt (static_cast<float> (baseMeasureWidth) * displayScale());
}

int NotationView::measureHeightPx() const noexcept
{
    return juce::roundToInt (static_cast<float> (baseMeasureHeight) * displayScale());
}

int NotationView::lyricRowHeightPx() const noexcept
{
    return juce::roundToInt (static_cast<float> (baseLyricRowHeight) * juce::jmax (1.0f, displayScale() * 0.85f));
}

int NotationView::getMeasuresPerRow() const noexcept
{
    if (layoutMode == LayoutMode::horizontalStrip)
        return juce::jmax (1, score.getNumMeasures());

    const auto mw = measureWidthPx();
    const auto available = juce::jmax (mw, getWidth() > 0 ? getWidth() - pageMargin * 2 : 720);
    return juce::jmax (1, available / mw);
}

int NotationView::getContentWidth() const noexcept
{
    if (score.isEmpty())
        return 400;

    const auto mw = measureWidthPx();

    if (layoutMode == LayoutMode::horizontalStrip)
        return score.getNumMeasures() * mw + 40;

    const auto cols = getMeasuresPerRow();
    return cols * mw + pageMargin * 2;
}

int NotationView::getContentHeight() const noexcept
{
    if (score.isEmpty())
        return juce::jmax (200, stripViewportHeight);

    const auto rowH = measureHeightPx() + (score.hasLyrics() ? lyricRowHeightPx() : 0) + rowGap;

    if (layoutMode == LayoutMode::horizontalStrip)
    {
        // Fill the viewport so the strip doesn't sit as a thin band in a tall panel.
        const auto natural = rowH + 28;
        return juce::jmax (natural, stripViewportHeight);
    }

    const auto cols = getMeasuresPerRow();
    const auto rows = juce::jmax (1, (score.getNumMeasures() + cols - 1) / cols);
    return pageMargin + 28 + rows * rowH + pageMargin;
}

juce::Rectangle<int> NotationView::getMeasureBounds (const int measureIndex) const noexcept
{
    if (score.isEmpty() || ! juce::isPositiveAndBelow (measureIndex, score.getNumMeasures()))
        return {};

    const auto mw = measureWidthPx();
    const auto mh = measureHeightPx();
    const auto rowH = mh + (score.hasLyrics() ? lyricRowHeightPx() : 0) + rowGap;

    if (layoutMode == LayoutMode::horizontalStrip)
    {
        // Always sit below the title band so frets never cover "Song - Part".
        constexpr int titleBand = 34;
        const auto contentH = mh + (score.hasLyrics() ? lyricRowHeightPx() : 0);
        const auto availBelow = juce::jmax (0, getHeight() - titleBand);
        const auto y = titleBand + juce::jmax (0, (availBelow - contentH) / 2);
        return { 12 + measureIndex * mw, y, mw - 8, mh };
    }

    const auto cols = getMeasuresPerRow();
    const auto row = measureIndex / cols;
    const auto col = measureIndex % cols;
    return { pageMargin + col * mw,
             pageMargin + 28 + row * rowH,
             mw - 8,
             mh };
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
    const auto scale = forPrint ? 1.0f : displayScale();

    if (forPrint)
        g.fillAll (juce::Colours::white);
    else
        g.fillAll (colours.notationBackground);

    if (score.isEmpty())
    {
        g.setColour (forPrint ? juce::Colours::black : colours.textSecondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Notation ready - import MusicXML, browse the library, or run AI Tab.\n"
                    "Use the Part menu and Tab/Sheet buttons to choose what to view.",
                    getLocalBounds().reduced (16), juce::Justification::centred);
        return;
    }

    g.setColour (forPrint ? juce::Colours::black : colours.text);
    // Keep the strip title modest - don't scale it with the frets.
    const auto titleScale = layoutMode == LayoutMode::horizontalStrip
                                ? 1.0f
                                : juce::jmax (1.0f, scale * 0.9f);
    g.setFont (juce::FontOptions (15.0f * titleScale, juce::Font::bold));
    const auto titleY = layoutMode == LayoutMode::fullPageRows ? pageMargin : 6;
    const auto titleH = layoutMode == LayoutMode::horizontalStrip
                            ? 24
                            : juce::roundToInt (22.0f * juce::jmax (1.0f, scale * 0.85f));
    g.drawText (score.getTitle() + " - " + score.getActivePart().name,
                pageMargin, titleY, getWidth() - pageMargin * 2, titleH,
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
        const auto playheadX = static_cast<int> (score.getXPositionForBeat (beat, measureWidthPx()));
        const auto targetX = juce::jmax (0, playheadX - viewport->getViewWidth() / 3);
        viewport->setViewPosition (targetX, viewport->getViewPositionY());
        return;
    }

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
    const auto scale = forPrint ? 1.0f : displayScale();
    const auto lyricH = lyricRowHeightPx();
    const auto headerH = juce::roundToInt (18.0f * juce::jmax (1.0f, scale * 0.85f));

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
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff505050));
        g.drawRoundedRectangle (bounds.toFloat(), 6.0f, 1.2f);
    }

    auto measureBounds = bounds;

    g.setColour (forPrint ? juce::Colours::black : juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (12.0f * juce::jmax (1.0f, scale * 0.9f), juce::Font::bold));
    g.drawText (juce::String (measure.number), measureBounds.removeFromTop (headerH),
                juce::Justification::centred);

    auto noteArea = measureBounds;

    if (score.hasLyrics())
        noteArea.removeFromBottom (lyricH);

    noteArea = noteArea.reduced (juce::roundToInt (6.0f * scale), juce::roundToInt (4.0f * scale));
    auto x = noteArea.getX() + juce::roundToInt (8.0f * scale);

    for (const auto& note : measure.notes)
    {
        if (score.getNotationMode() == NotationMode::tab)
            drawTabNote (g, note, noteArea, x, forPrint);
        else
            drawStandardNote (g, note, noteArea, x, forPrint);

        if (note.isTuplet)
        {
            g.setColour (forPrint ? juce::Colours::darkgrey : juce::Colours::orange.withAlpha (0.8f));
            g.setFont (juce::FontOptions (9.0f * scale));
            g.drawText ("3", x - 4, noteArea.getY() + 2, juce::roundToInt (14.0f * scale),
                        juce::roundToInt (12.0f * scale), juce::Justification::centred);
        }

        if (note.lyricText.isNotEmpty())
        {
            const auto isLyricActive = ! forPrint && activeLyricNote != nullptr
                                       && std::abs (activeLyricNote->startBeat - note.startBeat) < 0.001;
            drawLyric (g, note, bounds, x, isLyricActive, forPrint);
        }

        const auto spacing = juce::jlimit (juce::roundToInt (14.0f * scale),
                                           juce::roundToInt (48.0f * scale),
                                           static_cast<int> (note.durationBeats * 8.0 * scale));
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
    const auto scale = forPrint ? 1.0f : displayScale();
    const auto lyricH = lyricRowHeightPx();
    auto lyricArea = bounds.withTrimmedTop (bounds.getHeight() - lyricH).reduced (4, 0);

    if (isActive && ! forPrint)
    {
        g.setColour (juce::Colour (0xffffcc00).withAlpha (0.35f));
        g.fillRoundedRectangle (static_cast<float> (x - 8), static_cast<float> (lyricArea.getY()),
                                28.0f * scale, static_cast<float> (lyricArea.getHeight()), 3.0f);
    }

    g.setColour (forPrint ? juce::Colours::black
                          : (isActive ? juce::Colours::white : juce::Colours::white.withAlpha (0.75f)));
    g.setFont (juce::FontOptions ((isActive ? 13.0f : 11.0f) * scale,
                                  isActive ? juce::Font::bold : juce::Font::plain));
    g.drawText (note.lyricText, x - 10, lyricArea.getY(), juce::roundToInt (40.0f * scale),
                lyricArea.getHeight(), juce::Justification::centred);
}

void NotationView::drawStandardNote (juce::Graphics& g,
                                     const NoteEvent& note,
                                     const juce::Rectangle<int> bounds,
                                     const int x,
                                     const bool forPrint) const
{
    const auto scale = forPrint ? 1.0f : displayScale();
    const auto staffTop = bounds.getY() + juce::roundToInt (20.0f * scale);
    const auto lineSpacing = juce::roundToInt (8.0f * scale);
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
        g.setFont (juce::FontOptions (12.0f * scale));
        g.drawText ("r", x - 6, staffTop + 8, juce::roundToInt (22.0f * scale),
                    juce::roundToInt (18.0f * scale), juce::Justification::centred);
        return;
    }

    const auto pitchOffset = note.midiPitch >= 0 ? (note.midiPitch % 12) : 0;
    const auto y = staffTop + (4 * lineSpacing) - (pitchOffset * lineSpacing / 3);
    const auto r = 5.0f * scale;

    g.setColour (forPrint ? juce::Colours::black : juce::Colours::white);
    g.fillEllipse (static_cast<float> (x) - r, static_cast<float> (y) - r, r * 2.0f, r * 2.0f);

    g.setFont (juce::FontOptions (10.0f * scale));
    g.drawText (note.label, x - 12, y - juce::roundToInt (24.0f * scale),
                juce::roundToInt (30.0f * scale), juce::roundToInt (16.0f * scale),
                juce::Justification::centred);
}

void NotationView::drawTabNote (juce::Graphics& g,
                                const NoteEvent& note,
                                const juce::Rectangle<int> bounds,
                                const int x,
                                const bool forPrint) const
{
    const auto scale = forPrint ? 1.0f : displayScale();
    // Fill the measure body with 6 strings - line spacing grows with the panel.
    const auto usable = juce::jmax (48, bounds.getHeight() - juce::roundToInt (8.0f * scale));
    const auto lineSpacing = juce::jmax (10, usable / 6);
    const auto tabTop = bounds.getY() + juce::jmax (4, (bounds.getHeight() - lineSpacing * 5) / 2);
    const auto lineColour = forPrint ? juce::Colours::black : juce::Colour (0xff606060);
    const auto stroke = juce::jmax (1.0f, scale * 1.1f);

    for (int line = 0; line < 6; ++line)
    {
        g.setColour (lineColour);
        const auto y = static_cast<float> (tabTop + line * lineSpacing);
        g.drawLine (static_cast<float> (bounds.getX()), y,
                    static_cast<float> (bounds.getRight()), y, stroke);
    }

    if (note.isRest)
        return;

    const auto stringIndex = juce::jlimit (1, 6, note.stringNumber);
    const auto y = tabTop + (stringIndex - 1) * lineSpacing;
    const auto fontSize = juce::jlimit (12.0f, 28.0f, 12.0f * scale * 1.15f);
    const auto cell = juce::roundToInt (fontSize * 1.6f);

    g.setColour (forPrint ? juce::Colours::black : juce::Colours::white);
    g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
    g.drawText (note.label, x - cell / 2, y - cell / 2, cell, cell, juce::Justification::centred);
}

} // namespace jamstudio::notation
