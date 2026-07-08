#pragma once

#include "../audio/TransportController.h"
#include "../ui/JamStudioTheme.h"
#include "LyricsTrack.h"

namespace jamstudio::notation
{

/** Scrolling karaoke lyrics panel that auto-follows playback. */
class LyricsView : public juce::Component,
                   public juce::Timer
{
public:
    LyricsView (jamstudio::audio::TransportController& transport);
    ~LyricsView() override;

    void setLyrics (const LyricsTrack& lyrics);
    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    class LyricsContent;

    void scrollToActiveLine();

    jamstudio::audio::TransportController& transportController;
    LyricsTrack lyrics;
    juce::Viewport viewport;
    std::unique_ptr<LyricsContent> content;
    int lastActiveLine = -1;
    int lastActiveWord = -1;
};

} // namespace jamstudio::notation