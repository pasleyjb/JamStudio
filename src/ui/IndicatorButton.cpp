#include "IndicatorButton.h"

#include "JamStudioLookAndFeel.h"

namespace jamstudio::ui
{

IndicatorButton::IndicatorButton (const juce::String& name, const juce::String& text)
    : juce::Button (name)
{
    setButtonText (text);
}

void IndicatorButton::setIndicatorActive (const bool active, const bool blink)
{
    indicatorActive = active;
    indicatorBlink = blink;
    repaint();
}

void IndicatorButton::paintButton (juce::Graphics& g,
                                   const bool shouldDrawButtonAsHighlighted,
                                   const bool shouldDrawButtonAsDown)
{
    const auto colours = JamStudioTheme::getColours();
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    if (auto* laf = dynamic_cast<JamStudioLookAndFeel*> (&getLookAndFeel()))
        laf->drawButtonBackground (g, *this, colours.buttonFace, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    else
        g.fillAll (colours.buttonFace);

    const auto ledSize = 7.0f;
    const auto ledBounds = juce::Rectangle<float> (bounds.getX() + 5.0f,
                                                   bounds.getCentreY() - ledSize * 0.5f,
                                                   ledSize, ledSize);

    auto ledColour = colours.indicatorOff;

    if (indicatorActive)
        ledColour = indicatorBlink ? indicatorColour.brighter (0.35f) : indicatorColour;

    g.setColour (ledColour.darker (0.5f));
    g.fillEllipse (ledBounds.expanded (0.5f));
    g.setColour (ledColour);
    g.fillEllipse (ledBounds);

    if (indicatorActive)
    {
        g.setColour (ledColour.brighter (0.6f).withAlpha (0.7f));
        g.fillEllipse (ledBounds.reduced (2.0f));
    }

    g.setColour (colours.text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (getButtonText(), bounds.withTrimmedLeft (16.0f), juce::Justification::centred);
}

} // namespace jamstudio::ui