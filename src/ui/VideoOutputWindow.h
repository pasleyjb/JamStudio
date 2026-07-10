#pragma once

#include "../audio/TransportController.h"
#include "../notation/LyricsTrack.h"
#include <JuceHeader.h>

namespace jamstudio::ui
{

/** Shared helpers for multi-monitor stage video outputs. */
struct VideoOutputHelpers
{
    /** Number of attached displays (main + extras). */
    [[nodiscard]] static int getNumDisplays();

    /** Bounds for display index (0 = primary). Falls back to primary if out of range. */
    [[nodiscard]] static juce::Rectangle<int> getDisplayBounds (int displayIndex, bool fullArea = true);

    /** Short label for a display, e.g. "Display 2 (1920x1080)". */
    [[nodiscard]] static juce::String getDisplayLabel (int displayIndex);
};

/**
 * Output A - Karaoke lyrics for house / singer screen.
 * Large current line + previous/next, synced to transport.
 */
class KaraokeOutputWindow : public juce::DocumentWindow
{
public:
    explicit KaraokeOutputWindow (jamstudio::audio::TransportController& transport);
    ~KaraokeOutputWindow() override;

    void setLyrics (const jamstudio::notation::LyricsTrack& lyrics);
    void setSongTitle (const juce::String& title);
    void showOnDisplay (int displayIndex);
    void hideOutput();
    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    [[nodiscard]] bool isOutputVisible() const noexcept { return outputOpen; }
    [[nodiscard]] int getDisplayIndex() const noexcept { return displayIndex; }

private:
    class Content;
    void hideInternal();

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    bool outputOpen = false;
    int displayIndex = 0;
};

/**
 * Output B - Stage / video-board FX feed.
 * Dark canvas with reactive bars, song title, and beat-ish motion for LED walls.
 * Energy can follow song transport and/or stage media player level.
 */
class StageFxOutputWindow : public juce::DocumentWindow
{
public:
    explicit StageFxOutputWindow (jamstudio::audio::TransportController& transport);
    ~StageFxOutputWindow() override;

    void setSongTitle (const juce::String& title);
    void setSetInfo (const juce::String& setName, int songIndex, int songCount);
    void setWaitingBetweenSongs (bool waiting);
    /** Optional external energy 0..1 (stage media meter) blended into visuals. */
    void setExternalEnergy (float energy01);
    void showOnDisplay (int displayIndex);
    void hideOutput();
    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    [[nodiscard]] bool isOutputVisible() const noexcept { return outputOpen; }
    [[nodiscard]] int getDisplayIndex() const noexcept { return displayIndex; }

private:
    class Content;
    void hideInternal();

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    bool outputOpen = false;
    int displayIndex = 1; // prefer second screen when present
};

} // namespace jamstudio::ui
