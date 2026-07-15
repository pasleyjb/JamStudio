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

    /** Console-style faders (mixer) + clean horizontal tracks (transport). */
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override;

    /** Realistic metal/plastic rotary pot for click and utility knobs. */
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    int getSliderThumbRadius (juce::Slider& slider) override;

private:
    void applyPalette();
    void drawVerticalFader (juce::Graphics& g, juce::Rectangle<float> bounds,
                            float sliderPos, juce::Slider& slider);
    void drawHorizontalFader (juce::Graphics& g, juce::Rectangle<float> bounds,
                              float sliderPos, juce::Slider& slider);
};

} // namespace jamstudio::ui