#pragma once

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** 3D push button with a status LED indicator. */
class IndicatorButton : public juce::Button
{
public:
    IndicatorButton (const juce::String& name, const juce::String& buttonText = {});

    void setIndicatorColour (juce::Colour colour) { indicatorColour = colour; repaint(); }
    void setIndicatorActive (bool active, bool blink = false);

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    juce::Colour indicatorColour { JamStudioTheme::getColours().indicatorOn };
    bool indicatorActive = false;
    bool indicatorBlink = false;
};

} // namespace jamstudio::ui