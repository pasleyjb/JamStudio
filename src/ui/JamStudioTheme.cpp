#include "JamStudioTheme.h"

namespace jamstudio::ui
{

bool JamStudioTheme::isDarkModeActive()
{
    return juce::Desktop::getInstance().isDarkModeActive();
}

ThemeColours JamStudioTheme::getDarkColours()
{
    return {
        juce::Colour (0xff1c1c1c),
        juce::Colour (0xff262626),
        juce::Colour (0xff2f2f2f),
        juce::Colour (0xff303030),
        juce::Colour (0xff111111),
        juce::Colour (0xff4a4a4a),
        juce::Colours::white,
        juce::Colours::white.withAlpha (0.55f),
        juce::Colour (0xff4a9eff),
        juce::Colour (0xff5cb8ff),
        juce::Colour (0xff1a1a1a),
        juce::Colour (0xff3a3a3a),
        juce::Colour (0xff555555),
        juce::Colour (0xff2a2a2a),
        juce::Colour (0xff44dd66),
        juce::Colour (0xff2a2a2a),
        juce::Colour (0xffee4444),
        juce::Colour (0xffffaa22),
        juce::Colour (0xffffcc00),
        juce::Colour (0xff141414),
        juce::Colour (0xff161616),
        juce::Colour (0xff222222)
    };
}

ThemeColours JamStudioTheme::getLightColours()
{
    return {
        juce::Colour (0xffe8e8e8),
        juce::Colour (0xfff3f3f3),
        juce::Colour (0xfffafafa),
        juce::Colour (0xffececec),
        juce::Colour (0xffb0b0b0),
        juce::Colour (0xffd8d8d8),
        juce::Colour (0xff1a1a1a),
        juce::Colour (0xff555555),
        juce::Colour (0xff2060c0),
        juce::Colour (0xff2060c0),
        juce::Colour (0xfff8f8f8),
        juce::Colour (0xffc8c8c8),
        juce::Colour (0xff888888),
        juce::Colour (0xffe0e0e0),
        juce::Colour (0xff22aa44),
        juce::Colour (0xffcccccc),
        juce::Colour (0xffcc3333),
        juce::Colour (0xffdd8800),
        juce::Colour (0xffcc9900),
        juce::Colour (0xfff5f5f5),
        juce::Colour (0xfff0f0f0),
        juce::Colour (0xffe4e4e4)
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
    component.setColour (juce::Slider::trackColourId, colours.border);
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