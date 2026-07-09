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
    void closeButtonPressed() override;
    void showMixer (bool shouldShow);
    void setVisibilityChangedCallback (std::function<void (bool visible)> callback);
    [[nodiscard]] bool isMixerVisible() const noexcept { return isVisible(); }

private:
    class Content;

    jamstudio::audio::TransportController& transportController;
    std::unique_ptr<Content> content;
    std::function<void (bool)> visibilityChanged;
};

} // namespace jamstudio::ui
