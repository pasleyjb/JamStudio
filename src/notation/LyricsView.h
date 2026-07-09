#pragma once

#include "../audio/TransportController.h"
#include "../ui/JamStudioTheme.h"
#include "LyricsTrack.h"

namespace jamstudio::notation
{

/** Compact karaoke lyrics panel: current + neighbors, auto-follows playback. */
class LyricsView : public juce::Component,
                   public juce::Timer
{
public:
    LyricsView (jamstudio::audio::TransportController& transport);
    ~LyricsView() override;

    void setLyrics (const LyricsTrack& lyrics);
    void clear();

    /** Cumulative time offset applied to display/sync (seconds). */
    void setSyncOffset (double offsetSeconds);
    [[nodiscard]] double getSyncOffset() const noexcept { return syncOffsetSeconds; }
    void nudgeSyncOffset (double deltaSeconds);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void rebuildFromBase();
    void updateActiveFromTransport();
    void paintKaraokeLine (juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           const LyricLine* line,
                           bool isActive,
                           int activeWord) const;

    jamstudio::audio::TransportController& transportController;
    LyricsTrack baseLyrics;
    LyricsTrack lyrics;
    double syncOffsetSeconds = 0.0;

    juce::TextButton earlierButton { "−0.5s" };
    juce::TextButton laterButton { "+0.5s" };
    juce::Label offsetLabel;
    juce::Label hintLabel;

    int lastActiveLine = -1;
    int lastActiveWord = -1;
};

} // namespace jamstudio::notation
