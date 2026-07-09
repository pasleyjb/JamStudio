#pragma once

#include "../audio/TransportController.h"
#include "Score.h"

namespace jamstudio::notation
{

/** Renders sheet music or guitar tab with synced lyrics and scrolls with playback. */
class NotationView : public juce::Component,
                     public juce::Timer
{
public:
    enum class LayoutMode
    {
        horizontalStrip, // DAW panel: measures in one horizontal row
        fullPageRows     // Full-page / print: wrapped measure rows
    };

    NotationView (jamstudio::audio::TransportController& transport);

    void setScore (const Score& newScore);
    void clear();
    void setLayoutMode (LayoutMode mode);
    void setPrintFriendly (bool shouldBePrintFriendly);
    void setFollowPlayback (bool shouldFollow);

    /**
     * Tell the horizontal strip how tall the parent viewport is so measures
     * scale up (bigger frets / wider measures) instead of leaving empty space.
     */
    void setStripViewportHeight (int heightPixels);

    [[nodiscard]] LayoutMode getLayoutMode() const noexcept { return layoutMode; }
    [[nodiscard]] int getContentHeight() const noexcept;
    [[nodiscard]] int getContentWidth() const noexcept;
    [[nodiscard]] int getMeasuresPerRow() const noexcept;
    [[nodiscard]] juce::Rectangle<int> getMeasureBounds (int measureIndex) const noexcept;

    /** Renders the full content into an image (for print / export). */
    [[nodiscard]] juce::Image renderToImage (float scale = 2.0f) const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void updateContentSize();
    void scrollToBeat (double beat);
    void paintScore (juce::Graphics& g, bool forPrint) const;
    void drawMeasure (juce::Graphics& g, const Measure& measure, juce::Rectangle<int> bounds,
                      bool isActive, const NoteEvent* activeLyricNote, bool forPrint) const;
    void drawLyric (juce::Graphics& g, const NoteEvent& note, juce::Rectangle<int> bounds,
                    int x, bool isActive, bool forPrint) const;
    void drawStandardNote (juce::Graphics& g, const NoteEvent& note, juce::Rectangle<int> bounds,
                           int x, bool forPrint) const;
    void drawTabNote (juce::Graphics& g, const NoteEvent& note, juce::Rectangle<int> bounds,
                      int x, bool forPrint) const;

    [[nodiscard]] int measureWidthPx() const noexcept;
    [[nodiscard]] int measureHeightPx() const noexcept;
    [[nodiscard]] int lyricRowHeightPx() const noexcept;
    [[nodiscard]] float displayScale() const noexcept;

    jamstudio::audio::TransportController& transportController;
    Score score;
    LayoutMode layoutMode = LayoutMode::horizontalStrip;
    bool printFriendly = false;
    bool followPlayback = true;
    int stripViewportHeight = 0;
    int lastHighlightedMeasure = -1;
    double lastHighlightedBeat = -1.0;
    double lastScrolledBeat = -1.0;

    static constexpr int baseMeasureWidth = 180;
    static constexpr int baseMeasureHeight = 160;
    static constexpr int baseLyricRowHeight = 22;
    static constexpr int pageMargin = 24;
    static constexpr int rowGap = 16;
};

} // namespace jamstudio::notation
