#pragma once

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** Audacity-inspired 3D controls that follow system light/dark mode. */
class JamStudioLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JamStudioLookAndFeel();

    void refreshTheme();
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override;

private:
    void applyPalette();
};

} // namespace jamstudio::ui