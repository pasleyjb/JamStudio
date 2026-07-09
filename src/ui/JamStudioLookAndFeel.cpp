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

int JamStudioLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
    if (slider.isVertical())
        return juce::jlimit (10, 16, slider.getWidth() / 2);

    return juce::LookAndFeel_V4::getSliderThumbRadius (slider);
}

void JamStudioLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                            const int x, const int y,
                                            const int width, const int height,
                                            const float sliderPos,
                                            const float /*minSliderPos*/,
                                            const float /*maxSliderPos*/,
                                            const juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

    if (style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical)
    {
        drawVerticalFader (g, bounds, sliderPos, slider);
        return;
    }

    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        drawHorizontalFader (g, bounds, sliderPos, slider);
        return;
    }

    juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                            0.0f, 0.0f, style, slider);
}

void JamStudioLookAndFeel::drawVerticalFader (juce::Graphics& g,
                                             juce::Rectangle<float> bounds,
                                             const float sliderPos,
                                             juce::Slider& slider)
{
    const auto theme = JamStudioTheme::getColours();
    const auto isOver = slider.isMouseOverOrDragging();
    const auto isDown = slider.isMouseButtonDown();

    // --- Chassis / slot background ---
    auto chassis = bounds.reduced (juce::jmax (1.0f, bounds.getWidth() * 0.08f), 2.0f);
    g.setGradientFill (juce::ColourGradient (theme.panelBackground.darker (0.25f),
                                             chassis.getTopLeft(),
                                             theme.panelBackground.brighter (0.05f),
                                             chassis.getBottomRight(), false));
    g.fillRoundedRectangle (chassis, 4.0f);
    g.setColour (theme.border.darker (0.2f));
    g.drawRoundedRectangle (chassis, 4.0f, 1.0f);

    // --- Metal slot (recessed track) ---
    const auto slotW = juce::jlimit (6.0f, 12.0f, chassis.getWidth() * 0.28f);
    auto slot = juce::Rectangle<float> (slotW, chassis.getHeight() - 18.0f)
                    .withCentre ({ chassis.getCentreX(), chassis.getCentreY() });

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (slot.expanded (3.0f, 2.0f), 3.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a1a),
                                             slot.getTopLeft(),
                                             juce::Colour (0xff3a3a3a),
                                             slot.getBottomRight(), false));
    g.fillRoundedRectangle (slot, 2.0f);

    // Inner rail highlight
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (slot.getCentreX() - 0.5f, slot.getY() + 2.0f,
                slot.getCentreX() - 0.5f, slot.getBottom() - 2.0f, 1.0f);

    // --- Tick marks (0 / 25 / 50 / 75 / 100 style) ---
    g.setColour (theme.textSecondary.withAlpha (0.55f));
    for (int i = 0; i <= 4; ++i)
    {
        const auto t = static_cast<float> (i) / 4.0f;
        const auto y = slot.getBottom() - t * slot.getHeight();
        const auto tickLen = (i == 0 || i == 4 || i == 2) ? 7.0f : 4.0f;
        g.drawLine (slot.getRight() + 3.0f, y, slot.getRight() + 3.0f + tickLen, y, 1.0f);
        g.drawLine (slot.getX() - 3.0f - tickLen, y, slot.getX() - 3.0f, y, 1.0f);
    }

    // Unity / 0 dB style marker near top third
    {
        const auto y = slot.getBottom() - 0.78f * slot.getHeight();
        g.setColour (theme.accent.withAlpha (0.55f));
        g.drawLine (slot.getX() - 5.0f, y, slot.getRight() + 5.0f, y, 1.2f);
    }

    // --- Fader cap (plastic console knob) ---
    const auto capH = juce::jlimit (22.0f, 34.0f, bounds.getHeight() * 0.12f);
    const auto capW = juce::jlimit (18.0f, 28.0f, chassis.getWidth() * 0.78f);
    auto cap = juce::Rectangle<float> (capW, capH)
                   .withCentre ({ chassis.getCentreX(), sliderPos });

    // Clamp cap inside chassis
    if (cap.getY() < chassis.getY() + 2.0f)
        cap.setY (chassis.getY() + 2.0f);
    if (cap.getBottom() > chassis.getBottom() - 2.0f)
        cap.setY (chassis.getBottom() - 2.0f - cap.getHeight());

    // Soft shadow under cap
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);

    // Cap body gradient (beige/grey plastic like real mixers)
    auto faceTop = juce::Colour (0xffd8d4cc);
    auto faceBot = juce::Colour (0xff9a968e);

    if (isDown)
    {
        faceTop = faceTop.darker (0.12f);
        faceBot = faceBot.darker (0.08f);
    }
    else if (isOver)
    {
        faceTop = faceTop.brighter (0.08f);
    }

    g.setGradientFill (juce::ColourGradient (faceTop, cap.getTopLeft(),
                                             faceBot, cap.getBottomLeft(), false));
    g.fillRoundedRectangle (cap, 3.0f);

    // Edge bevel
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawLine (cap.getX() + 1.0f, cap.getY() + 1.0f, cap.getRight() - 1.0f, cap.getY() + 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);

    // Grip ridges
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    const auto ridgeCount = 5;
    const auto ridgePad = 4.0f;
    const auto ridgeSpan = cap.getHeight() - ridgePad * 2.0f;

    for (int i = 0; i < ridgeCount; ++i)
    {
        const auto t = (static_cast<float> (i) + 0.5f) / static_cast<float> (ridgeCount);
        const auto y = cap.getY() + ridgePad + t * ridgeSpan;
        g.drawLine (cap.getX() + 3.5f, y, cap.getRight() - 3.5f, y, 1.0f);
    }

    // Center indicator line (position marker)
    g.setColour (theme.accent.brighter (isOver ? 0.15f : 0.0f));
    g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 4.0f, 2.0f);
}

void JamStudioLookAndFeel::drawHorizontalFader (juce::Graphics& g,
                                               juce::Rectangle<float> bounds,
                                               const float sliderPos,
                                               juce::Slider& slider)
{
    const auto theme = JamStudioTheme::getColours();
    const auto isOver = slider.isMouseOverOrDragging();

    auto track = bounds.reduced (2.0f, juce::jmax (2.0f, bounds.getHeight() * 0.28f));
    track = track.withHeight (juce::jlimit (4.0f, 10.0f, track.getHeight()))
                 .withCentre (bounds.getCentre());

    // Recessed trough
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (track.expanded (1.0f, 1.5f), 3.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222222),
                                             track.getTopLeft(),
                                             juce::Colour (0xff3a3a3a),
                                             track.getBottomLeft(), false));
    g.fillRoundedRectangle (track, 3.0f);

    // Fill to thumb
    auto filled = track.withWidth (juce::jmax (0.0f, sliderPos - track.getX()));
    if (filled.getWidth() > 1.0f)
    {
        g.setGradientFill (juce::ColourGradient (theme.accent.brighter (0.15f),
                                                 filled.getTopLeft(),
                                                 theme.accent.darker (0.25f),
                                                 filled.getBottomLeft(), false));
        g.fillRoundedRectangle (filled, 3.0f);
    }

    // Thumb
    const auto thumbR = juce::jlimit (6.0f, 11.0f, bounds.getHeight() * 0.42f);
    auto thumb = juce::Rectangle<float> (thumbR * 2.0f, thumbR * 2.0f)
                     .withCentre ({ sliderPos, bounds.getCentreY() });

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (thumb.translated (0.0f, 1.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e4dc),
                                             thumb.getTopLeft(),
                                             juce::Colour (0xffa8a49c),
                                             thumb.getBottomLeft(), false));
    g.fillEllipse (thumb);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (thumb, 1.0f);

    if (isOver)
    {
        g.setColour (theme.accent.withAlpha (0.35f));
        g.drawEllipse (thumb.expanded (1.0f), 1.5f);
    }

    // Center notch
    g.setColour (theme.accent);
    g.fillRect (thumb.getCentreX() - 1.0f, thumb.getY() + 3.0f, 2.0f, thumb.getHeight() - 6.0f);
}

} // namespace jamstudio::ui