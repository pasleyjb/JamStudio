#pragma once

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** Classic cassette / tape-deck style transport control (icon only). */
class TapeDeckButton : public juce::Button
{
public:
    enum class Icon
    {
        play,
        pause,
        stop
    };

    TapeDeckButton (const juce::String& name, Icon icon);

    void setActive (bool shouldBeActive);
    [[nodiscard]] bool isActive() const noexcept { return active; }

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour) const;

    Icon iconType;
    bool active = false;
};

} // namespace jamstudio::ui
