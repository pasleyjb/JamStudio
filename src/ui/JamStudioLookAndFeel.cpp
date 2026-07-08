#include "JamStudioLookAndFeel.h"

namespace jamstudio::ui
{

JamStudioLookAndFeel::JamStudioLookAndFeel()
{
    applyPalette();
}

void JamStudioLookAndFeel::refreshTheme()
{
    applyPalette();
}

void JamStudioLookAndFeel::applyPalette()
{
    const auto colours = JamStudioTheme::getColours();
    setColour (juce::ResizableWindow::backgroundColourId, colours.windowBackground);
    setColour (juce::TextButton::buttonColourId, colours.buttonFace);
    setColour (juce::TextButton::buttonOnColourId, colours.accent.withAlpha (0.35f));
    setColour (juce::TextButton::textColourOffId, colours.text);
    setColour (juce::TextButton::textColourOnId, colours.text);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, colours.border);
    setColour (juce::TabbedButtonBar::tabTextColourId, colours.text);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, colours.borderLight);
    setColour (juce::TabbedButtonBar::frontTextColourId, colours.text);
}

void JamStudioLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                 juce::Button& button,
                                                 const juce::Colour& /*backgroundColour*/,
                                                 const bool shouldDrawButtonAsHighlighted,
                                                 const bool shouldDrawButtonAsDown)
{
    const auto theme = JamStudioTheme::getColours();
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    const auto face = button.getToggleState()
        ? theme.accent.withAlpha (0.28f)
        : theme.buttonFace;

    const auto topLeft = shouldDrawButtonAsDown ? theme.buttonShadow : theme.buttonHighlight;
    const auto bottomRight = shouldDrawButtonAsDown ? theme.buttonHighlight : theme.buttonShadow;

    g.setGradientFill ({ face, bounds.getTopLeft(), face.darker (0.08f), bounds.getBottomRight(), false });
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (topLeft);
    g.drawLine (bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getY(), 1.0f);
    g.drawLine (bounds.getX(), bounds.getY(), bounds.getX(), bounds.getBottom(), 1.0f);

    g.setColour (bottomRight);
    g.drawLine (bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);
    g.drawLine (bounds.getRight(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 1.0f);

    if (shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
    {
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds, 3.0f);
    }
}

void JamStudioLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                          const bool isMouseOver, const bool isMouseDown)
{
    const auto theme = JamStudioTheme::getColours();
    const auto area = button.getActiveArea().toFloat();

    if (button.isFrontTab())
    {
        g.setColour (theme.panelBackground);
        g.fillRect (area);
        g.setColour (theme.accent);
        g.fillRect (area.getX(), area.getBottom() - 2.0f, area.getWidth(), 2.0f);
    }
    else
    {
        g.setColour (theme.toolbarBackground);
        g.fillRect (area);

        if (isMouseOver || isMouseDown)
        {
            g.setColour (theme.accent.withAlpha (0.12f));
            g.fillRect (area);
        }
    }

    g.setColour (button.isFrontTab() ? theme.text : theme.textSecondary);
    g.setFont (juce::FontOptions (13.0f, button.isFrontTab() ? juce::Font::bold : juce::Font::plain));
    g.drawText (button.getButtonText(), area.reduced (8.0f, 2.0f), juce::Justification::centred);
}

} // namespace jamstudio::ui