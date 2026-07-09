#pragma once

#include "../audio/StemMixer.h"
#include "../audio/TransportController.h"
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

    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    void minimiseButtonPressed() override;
    void maximiseButtonPressed() override;

    void showMixer (bool shouldShow);
    void setVisibilityChangedCallback (std::function<void (bool visible)> callback);
    [[nodiscard]] bool isMixerVisible() const noexcept { return windowOpen; }

private:
    class Content;

    void hideMixer();

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    std::function<void (bool)> visibilityChanged;
    bool windowOpen = false;
    juce::Rectangle<int> restoredBounds { 100, 100, 680, 460 };
};

} // namespace jamstudio::ui
