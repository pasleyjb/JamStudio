#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

struct ThemeColours
{
    juce::Colour windowBackground;
    juce::Colour panelBackground;
    juce::Colour trackBackground;
    juce::Colour toolbarBackground;
    juce::Colour border;
    juce::Colour borderLight;
    juce::Colour text;
    juce::Colour textSecondary;
    juce::Colour accent;
    juce::Colour waveform;
    juce::Colour waveformBackground;
    juce::Colour buttonHighlight;
    juce::Colour buttonShadow;
    juce::Colour buttonFace;
    juce::Colour indicatorOn;
    juce::Colour indicatorOff;
    juce::Colour indicatorMute;
    juce::Colour indicatorSolo;
    juce::Colour lyricsHighlight;
    juce::Colour lyricsBackground;
    juce::Colour notationBackground;
    juce::Colour statusBackground;
};

/** System-aware light/dark palette shared across the app. */
class JamStudioTheme
{
public:
    [[nodiscard]] static bool isDarkModeActive();
    [[nodiscard]] static ThemeColours getColours();
    static void applyToComponent (juce::Component& component);
    static void refreshAll (juce::Component& root);

private:
    [[nodiscard]] static ThemeColours getDarkColours();
    [[nodiscard]] static ThemeColours getLightColours();
};

} // namespace jamstudio::ui