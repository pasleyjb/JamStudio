#pragma once

#include "../audio/TransportController.h"
#include "LyricsTrack.h"

namespace jamstudio::notation
{

/** Karaoke-style synced lyrics display for LRC files. */
class LyricsView : public juce::Component,
                     public juce::Timer
{
public:
    LyricsView (jamstudio::audio::TransportController& transport);

    void setLyrics (const LyricsTrack& lyrics);
    void clear();

    void paint (juce::Graphics& g) override;
    void timerCallback() override;

private:
    jamstudio::audio::TransportController& transportController;
    LyricsTrack lyrics;
    int lastActiveLine = -1;
};

} // namespace jamstudio::notation