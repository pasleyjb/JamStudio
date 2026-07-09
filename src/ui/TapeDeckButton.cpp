#include "TapeDeckButton.h"

namespace jamstudio::ui
{

TapeDeckButton::TapeDeckButton (const juce::String& name, const Icon icon)
    : juce::Button (name),
      iconType (icon)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TapeDeckButton::setActive (const bool shouldBeActive)
{
    if (active == shouldBeActive)
        return;

    active = shouldBeActive;
    repaint();
}

void TapeDeckButton::paintButton (juce::Graphics& g,
                                  const bool shouldDrawButtonAsHighlighted,
                                  const bool shouldDrawButtonAsDown)
{
    const auto colours = JamStudioTheme::getColours();
    auto bounds = getLocalBounds().toFloat().reduced (1.5f);

    // Outer chassis
    const auto face = shouldDrawButtonAsDown ? colours.buttonFace.darker (0.18f)
                                             : colours.buttonFace.brighter (0.04f);

    g.setGradientFill (juce::ColourGradient (face.brighter (0.12f), bounds.getTopLeft(),
                                             face.darker (0.22f), bounds.getBottomRight(), false));
    g.fillRoundedRectangle (bounds, 7.0f);

    g.setColour (colours.border.darker (0.2f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.2f);

    // Inner well (recessed deck button look)
    auto well = bounds.reduced (3.5f);
    g.setColour (colours.windowBackground.darker (0.15f).withAlpha (0.9f));
    g.fillRoundedRectangle (well, 5.0f);

    if (shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
    {
        g.setColour (colours.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (well, 5.0f);
    }

    // Active play glow ring
    if (active)
    {
        g.setColour (colours.indicatorOn.withAlpha (0.55f));
        g.drawRoundedRectangle (well.reduced (1.0f), 4.0f, 1.5f);
    }

    auto iconColour = active ? colours.indicatorOn : colours.text;

    if (shouldDrawButtonAsDown)
        iconColour = iconColour.darker (0.15f);

    drawIcon (g, well.reduced (well.getWidth() * 0.22f, well.getHeight() * 0.22f), iconColour);
}

void TapeDeckButton::drawIcon (juce::Graphics& g, juce::Rectangle<float> area, const juce::Colour colour) const
{
    g.setColour (colour);

    switch (iconType)
    {
        case Icon::play:
        {
            juce::Path triangle;
            const auto x = area.getX() + area.getWidth() * 0.18f;
            const auto y = area.getY();
            const auto w = area.getWidth() * 0.78f;
            const auto h = area.getHeight();
            triangle.addTriangle (x, y, x, y + h, x + w, y + h * 0.5f);
            g.fillPath (triangle);
            break;
        }

        case Icon::pause:
        {
            const auto gap = area.getWidth() * 0.18f;
            const auto barW = (area.getWidth() - gap) * 0.42f;
            g.fillRoundedRectangle (area.getX(), area.getY(), barW, area.getHeight(), 2.0f);
            g.fillRoundedRectangle (area.getRight() - barW, area.getY(), barW, area.getHeight(), 2.0f);
            break;
        }

        case Icon::stop:
        {
            const auto size = juce::jmin (area.getWidth(), area.getHeight()) * 0.86f;
            auto square = juce::Rectangle<float> (size, size).withCentre (area.getCentre());
            g.fillRoundedRectangle (square, 2.0f);
            break;
        }
    }
}

} // namespace jamstudio::ui
