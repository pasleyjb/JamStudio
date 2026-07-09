#pragma once

#include "../audio/TransportController.h"
#include "../notation/LyricsTrack.h"

namespace jamstudio::ui
{

/** Full-page printable lyrics sheet (mirrors Full Page Tabs).
    Hidden until opened from Lyrics menu. */
class FullPageLyricsWindow : public juce::DocumentWindow
{
public:
    explicit FullPageLyricsWindow (jamstudio::audio::TransportController& transport);
    ~FullPageLyricsWindow() override;

    void setLyrics (const jamstudio::notation::LyricsTrack& lyrics);
    void showWindow (bool shouldShow);
    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    void setVisibilityChangedCallback (std::function<void (bool visible)> callback);
    [[nodiscard]] bool isLyricsWindowVisible() const noexcept { return windowOpen; }

private:
    class Content;

    void hideWindow();

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    std::function<void (bool)> visibilityChanged;
    bool windowOpen = false;
};

} // namespace jamstudio::ui
