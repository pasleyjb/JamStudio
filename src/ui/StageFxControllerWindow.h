#pragma once

#include "../audio/StageMediaPlayer.h"
#include "../audio/TransportController.h"
#include "../notation/LyricsTrack.h"
#include "IndicatorButton.h"
#include "TapeDeckButton.h"

namespace jamstudio::ui
{

/** Floating stage media controller — file, transport, video-output routing + live previews. */
class StageFxControllerWindow : public juce::DocumentWindow
{
public:
    /** Hooks into MainComponent karaoke / stage output windows. */
    struct VideoRouting
    {
        std::function<int()> getKaraokeDisplay;
        std::function<int()> getStageDisplay;
        std::function<void (int displayIndex)> setKaraokeDisplay;
        std::function<void (int displayIndex)> setStageDisplay;
        std::function<bool()> isKaraokeVisible;
        std::function<bool()> isStageVisible;
        std::function<void()> openKaraoke;
        std::function<void()> closeKaraoke;
        std::function<void()> openStage;
        std::function<void()> closeStage;
    };

    explicit StageFxControllerWindow (jamstudio::audio::TransportController& transport);
    ~StageFxControllerWindow() override;

    void showController (bool shouldShow);
    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;

    void setVideoRouting (VideoRouting routing);
    /** Refresh display lists / open-state buttons (e.g. after menu open/close). */
    void syncVideoRoutingUi();

    /** Keep karaoke preview in sync with the song (also used by full-screen karaoke). */
    void setLyrics (const jamstudio::notation::LyricsTrack& lyrics);
    void setSongTitle (const juce::String& title);

    [[nodiscard]] bool isControllerVisible() const noexcept { return windowOpen; }

    void setDockCallbacks (std::function<void()> onAttach, std::function<void()> onDetach);
    void setDockStickyState (bool sticky);

    void resized() override;

private:
    class Content;
    void hideController();
    void layoutDockButtons();

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    VideoRouting videoRouting;
    std::function<void()> dockAttach;
    std::function<void()> dockDetach;
    juce::TextButton attachButton { "<>" };
    juce::TextButton detachButton { "><" };
    bool windowOpen = false;
    juce::Rectangle<int> restoredBounds { 80, 60, 560, 640 };
};

} // namespace jamstudio::ui
