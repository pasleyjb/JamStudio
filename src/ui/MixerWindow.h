#pragma once

#include "../audio/StemMixer.h"
#include "../audio/TransportController.h"
#include "IndicatorButton.h"
#include "MixerChannelStrip.h"
#include "TapeDeckButton.h"

namespace jamstudio::ui
{

/** Floating mixer board with vertical channel strips + tape-deck transport remote. */
class MixerWindow : public juce::DocumentWindow
{
public:
    using StemChangedCallback = MixerChannelStrip::StemChangedCallback;

    explicit MixerWindow (jamstudio::audio::TransportController& transport);
    ~MixerWindow() override;

    void rebuild (jamstudio::audio::StemMixer& mixer, StemChangedCallback onChanged);
    /** Lightweight UI update after MIDI/fader changes (no strip recreate). */
    void syncFromMixer (const jamstudio::audio::StemMixer& mixer);
    /** Sync dedicated VIDEO sound fader from stage media player. */
    void syncVideoSoundSlider();

    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    void minimiseButtonPressed() override;
    void maximiseButtonPressed() override;

    void showMixer (bool shouldShow);
    void setVisibilityChangedCallback (std::function<void (bool visible)> callback);
    [[nodiscard]] bool isMixerVisible() const noexcept { return windowOpen; }
    [[nodiscard]] bool isMaximisedState() const noexcept { return maximised; }

    /**
     * Performance: save current mixer levels to the setlist song that is playing.
     * Button enables when a set is active and a song index is valid.
     */
    void setSaveSetlistMixCallback (std::function<void()> onSave);
    void setPerformanceMixContext (bool performanceActive,
                                   int songIndex,
                                   int songCount,
                                   const juce::String& songName);

    /** Title-bar stick controls: <> attach, >< detach. */
    void setDockCallbacks (std::function<void()> onAttach,
                           std::function<void()> onDetach,
                           std::function<void()> onMaximised);
    void setDockStickyState (bool sticky);

    void resized() override;

private:
    class Content;

    void hideMixer();
    void layoutDockButtons();
    [[nodiscard]] juce::Rectangle<int> getMaximiseBounds() const;

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    std::function<void (bool)> visibilityChanged;
    std::function<void()> dockAttach;
    std::function<void()> dockDetach;
    std::function<void()> dockMaximised;
    juce::TextButton attachButton { "<>" };
    juce::TextButton detachButton { "><" };
    bool windowOpen = false;
    bool maximised = false;
    juce::Rectangle<int> restoredBounds { 80, 60, 920, 520 };
};

} // namespace jamstudio::ui
