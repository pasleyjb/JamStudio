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
    NotationView (jamstudio::audio::TransportController& transport);

    void setScore (const Score& newScore);
    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void scrollToMeasure (int measureIndex);
    void drawMeasure (juce::Graphics& g, const Measure& measure, juce::Rectangle<int> bounds,
                      bool isActive, const NoteEvent* activeLyricNote) const;
    void drawLyric (juce::Graphics& g, const NoteEvent& note, juce::Rectangle<int> bounds,
                    int x, bool isActive) const;
    void drawStandardNote (juce::Graphics& g, const NoteEvent& note, juce::Rectangle<int> bounds, int x) const;
    void drawTabNote (juce::Graphics& g, const NoteEvent& note, juce::Rectangle<int> bounds, int x) const;
    [[nodiscard]] int getContentHeight() const noexcept;

    jamstudio::audio::TransportController& transportController;
    Score score;
    int lastHighlightedMeasure = -1;
    double lastHighlightedBeat = -1.0;
    static constexpr int measureWidth = 180;
    static constexpr int measureHeight = 160;
    static constexpr int lyricRowHeight = 22;
};

} // namespace jamstudio::notation