#pragma once

#include "../audio/TransportController.h"
#include "../notation/NotationView.h"
#include "../notation/Score.h"

namespace jamstudio::ui
{

/** Full-page notation/tab window with print and PNG export.
    Stays hidden until the user opens it from the menu (or notation header). */
class FullPageTabsWindow : public juce::DocumentWindow
{
public:
    explicit FullPageTabsWindow (jamstudio::audio::TransportController& transport);
    ~FullPageTabsWindow() override;

    void setScore (const jamstudio::notation::Score& score);
    void showWindow (bool shouldShow);
    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    void setVisibilityChangedCallback (std::function<void (bool visible)> callback);
    [[nodiscard]] bool isTabsWindowVisible() const noexcept { return windowOpen; }

private:
    class Content;

    void hideWindow();

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    std::function<void (bool)> visibilityChanged;
    bool windowOpen = false;
};

} // namespace jamstudio::ui
