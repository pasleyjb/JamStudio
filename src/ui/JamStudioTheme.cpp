#include "JamStudioTheme.h"

namespace jamstudio::ui
{

bool JamStudioTheme::isDarkModeActive()
{
    return juce::Desktop::getInstance().isDarkModeActive();
}

ThemeColours JamStudioTheme::getDarkColours()
{
    // Cool pro-DAW dark palette (closer to Reaper / Studio One than flat grey).
    return {
        juce::Colour (0xff12151a), // windowBackground
        juce::Colour (0xff1a1f27), // panelBackground
        juce::Colour (0xff1e2430), // trackBackground
        juce::Colour (0xff181c24), // toolbarBackground
        juce::Colour (0xff0a0c10), // border
        juce::Colour (0xff3a4455), // borderLight
        juce::Colour (0xffeef2f7), // text
        juce::Colour (0xff9aa6b8), // textSecondary
        juce::Colour (0xff4a9eff), // accent
        juce::Colour (0xff5eb0ff), // waveform
        juce::Colour (0xff0e1218), // waveformBackground
        juce::Colour (0xff2a3140), // buttonHighlight
        juce::Colour (0xff0d1016), // buttonShadow
        juce::Colour (0xff232a36), // buttonFace
        juce::Colour (0xff44dd88), // indicatorOn
        juce::Colour (0xff2a3140), // indicatorOff
        juce::Colour (0xffff4d5e), // indicatorMute
        juce::Colour (0xffffb020), // indicatorSolo
        juce::Colour (0xffffd34d), // lyricsHighlight
        juce::Colour (0xff10141c), // lyricsBackground
        juce::Colour (0xff141922), // notationBackground
        juce::Colour (0xff151a22)  // statusBackground
    };
}

ThemeColours JamStudioTheme::getLightColours()
{
    return {
        juce::Colour (0xffe6e9ef),
        juce::Colour (0xfff4f6fa),
        juce::Colour (0xfffbfcfe),
        juce::Colour (0xffeceff5),
        juce::Colour (0xffb4bcc9),
        juce::Colour (0xffd5dae3),
        juce::Colour (0xff161a22),
        juce::Colour (0xff5a6575),
        juce::Colour (0xff1f6fd6),
        juce::Colour (0xff1f6fd6),
        juce::Colour (0xfff7f8fb),
        juce::Colour (0xffffffff),
        juce::Colour (0xffa0a8b5),
        juce::Colour (0xffe4e8ef),
        juce::Colour (0xff1aaa55),
        juce::Colour (0xffc8ced8),
        juce::Colour (0xffd93a4a),
        juce::Colour (0xffd48a00),
        juce::Colour (0xffc99700),
        juce::Colour (0xfff2f4f8),
        juce::Colour (0xfff0f2f6),
        juce::Colour (0xffe2e6ee)
    };
}

ThemeColours JamStudioTheme::getColours()
{
    return isDarkModeActive() ? getDarkColours() : getLightColours();
}

void JamStudioTheme::applyToComponent (juce::Component& component)
{
    const auto colours = getColours();
    component.setColour (juce::ResizableWindow::backgroundColourId, colours.windowBackground);
    component.setColour (juce::DocumentWindow::backgroundColourId, colours.windowBackground);
    component.setColour (juce::Label::textColourId, colours.text);
    component.setColour (juce::TextButton::buttonColourId, colours.buttonFace);
    component.setColour (juce::TextButton::buttonOnColourId, colours.accent);
    component.setColour (juce::TextButton::textColourOffId, colours.text);
    component.setColour (juce::TextButton::textColourOnId, colours.text);
    component.setColour (juce::Slider::backgroundColourId, colours.panelBackground);
    component.setColour (juce::Slider::trackColourId, colours.accent.withAlpha (0.55f));
    component.setColour (juce::Slider::thumbColourId, colours.accent);
    component.setColour (juce::Slider::textBoxTextColourId, colours.text);
    component.setColour (juce::Slider::textBoxBackgroundColourId, colours.panelBackground);
    component.setColour (juce::Slider::textBoxOutlineColourId, colours.border);
    component.setColour (juce::TabbedComponent::backgroundColourId, colours.toolbarBackground);
    component.setColour (juce::TabbedComponent::outlineColourId, colours.border);
    component.setColour (juce::PopupMenu::backgroundColourId, colours.panelBackground);
    component.setColour (juce::PopupMenu::textColourId, colours.text);
    component.setColour (juce::PopupMenu::highlightedBackgroundColourId, colours.accent.withAlpha (0.25f));
    component.setColour (juce::PopupMenu::highlightedTextColourId, colours.text);
    component.setColour (juce::ProgressBar::backgroundColourId, colours.panelBackground);
    component.setColour (juce::ProgressBar::foregroundColourId, colours.accent);
}

void JamStudioTheme::refreshAll (juce::Component& root)
{
    applyToComponent (root);
    root.repaint();

    for (int i = 0; i < root.getNumChildComponents(); ++i)
    {
        if (auto* child = root.getChildComponent (i))
            refreshAll (*child);
    }
}

} // namespace jamstudio::ui
