#include "MixerChannelStrip.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

MixerChannelStrip::MixerChannelStrip (const int stemIndex,
                                      jamstudio::audio::StemMixer& mixer,
                                      StemChangedCallback onChanged)
    : index (stemIndex),
      stemMixer (mixer),
      onStemChanged (std::move (onChanged))
{
    const auto* track = stemMixer.getStem (index);

    if (track != nullptr)
    {
        type = track->getType();
        nameLabel.setText (track->getName().isNotEmpty()
                               ? track->getName()
                               : jamstudio::audio::stemTypeToString (track->getType()),
                           juce::dontSendNotification);
        muteButton.setToggleState (track->isMuted(), juce::dontSendNotification);
        soloButton.setToggleState (track->isSolo(), juce::dontSendNotification);
        volumeSlider.setValue (track->getVolume(), juce::dontSendNotification);
        levelLabel.setText (juce::String (static_cast<int> (track->getVolume() * 100)),
                            juce::dontSendNotification);
    }

    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (nameLabel);

    muteButton.setClickingTogglesState (true);
    muteButton.setIndicatorColour (JamStudioTheme::getColours().indicatorMute);
    muteButton.onClick = [this] { notifyChanged(); updateIndicators(); };
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
    soloButton.onClick = [this] { notifyChanged(); updateIndicators(); };
    addAndMakeVisible (soloButton);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setSliderSnapsToMousePosition (true);
    volumeSlider.setMouseDragSensitivity (180);
    volumeSlider.setVelocityBasedMode (false);
    volumeSlider.setPopupDisplayEnabled (true, true, this);
    volumeSlider.setTextValueSuffix (" %");
    volumeSlider.setNumDecimalPlacesToDisplay (0);
    volumeSlider.setColour (juce::Slider::textBoxTextColourId,
                            JamStudioTheme::getColours().text);
    volumeSlider.onValueChange = [this]
    {
        levelLabel.setText (juce::String (static_cast<int> (volumeSlider.getValue() * 100)),
                            juce::dontSendNotification);
        notifyChanged();
    };
    addAndMakeVisible (volumeSlider);

    levelLabel.setJustificationType (juce::Justification::centred);
    levelLabel.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (levelLabel);

    updateIndicators();
    startTimerHz (30);
}

void MixerChannelStrip::syncFromTrack (const jamstudio::audio::StemTrack& track)
{
    type = track.getType();
    nameLabel.setText (track.getName().isNotEmpty()
                           ? track.getName()
                           : jamstudio::audio::stemTypeToString (track.getType()),
                       juce::dontSendNotification);
    muteButton.setToggleState (track.isMuted(), juce::dontSendNotification);
    soloButton.setToggleState (track.isSolo(), juce::dontSendNotification);
    volumeSlider.setValue (track.getVolume(), juce::dontSendNotification);
    levelLabel.setText (juce::String (static_cast<int> (track.getVolume() * 100)),
                        juce::dontSendNotification);
    updateIndicators();
    repaint();
}

void MixerChannelStrip::notifyChanged()
{
    if (onStemChanged != nullptr)
        onStemChanged (index, muteButton.getToggleState(), soloButton.getToggleState(),
                       static_cast<float> (volumeSlider.getValue()));
}

void MixerChannelStrip::updateIndicators()
{
    muteButton.setIndicatorActive (muteButton.getToggleState());
    soloButton.setIndicatorActive (soloButton.getToggleState());
}

void MixerChannelStrip::timerCallback()
{
    if (auto* stem = stemMixer.getStem (index))
    {
        stem->tickMeterPeakHold (1.0f / 30.0f);
        displayLevel = stem->getMeterLevel();
        displayPeakHold = stem->getMeterPeakHold();
        repaint (meterBounds.expanded (1));
    }
}

void MixerChannelStrip::paintLevelMeter (juce::Graphics& g,
                                         juce::Rectangle<float> area,
                                         const float level,
                                         const float peakHold) const
{
    // Dark well
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (area, 2.0f);
    g.setColour (JamStudioTheme::getColours().border.withAlpha (0.7f));
    g.drawRoundedRectangle (area, 2.0f, 1.0f);

    auto inner = area.reduced (1.5f);
    const auto h = inner.getHeight();

    // Segment thresholds (linear amplitude): green < 0.55, yellow < 0.82, red above
    const float greenTop = 0.55f;
    const float yellowTop = 0.82f;

    auto fillSegment = [&] (const float fromNorm, const float toNorm, const juce::Colour colour)
    {
        const auto y1 = inner.getBottom() - toNorm * h;
        const auto y0 = inner.getBottom() - fromNorm * h;
        const auto segment = juce::Rectangle<float> (inner.getX(), y1, inner.getWidth(), juce::jmax (0.0f, y0 - y1));

        if (segment.getHeight() > 0.5f)
        {
            g.setColour (colour);
            g.fillRect (segment);
        }
    };

    const auto lit = juce::jlimit (0.0f, 1.0f, level);

    if (lit > 0.001f)
    {
        // Bottom green zone
        fillSegment (0.0f, juce::jmin (lit, greenTop), juce::Colour (0xff22cc55));

        if (lit > greenTop)
            fillSegment (greenTop, juce::jmin (lit, yellowTop), juce::Colour (0xffffcc22));

        if (lit > yellowTop)
            fillSegment (yellowTop, lit, juce::Colour (0xffff3344));
    }

    // Sticky peak hold marker (classic stereo “peak LED”)
    const auto hold = juce::jlimit (0.0f, 1.0f, peakHold);

    if (hold > 0.02f)
    {
        juce::Colour holdColour = juce::Colour (0xff22cc55);

        if (hold > yellowTop)
            holdColour = juce::Colour (0xffff2233);
        else if (hold > greenTop)
            holdColour = juce::Colour (0xffffcc22);

        const auto peakY = inner.getBottom() - hold * h;
        g.setColour (holdColour.brighter (0.35f));
        g.fillRect (inner.getX() - 1.0f, peakY - 1.5f, inner.getWidth() + 2.0f, 3.0f);
        g.setColour (holdColour.withAlpha (0.9f));
        g.fillRect (inner.getX(), peakY - 1.0f, inner.getWidth(), 2.0f);
    }

    // Segment tick marks
    g.setColour (juce::Colours::black.withAlpha (0.35f));

    for (float t : { greenTop, yellowTop })
    {
        const auto y = inner.getBottom() - t * h;
        g.drawHorizontalLine (juce::roundToInt (y), inner.getX(), inner.getRight());
    }
}

void MixerChannelStrip::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (colours.panelBackground.brighter (0.04f));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto accent = jamstudio::audio::stemTypeColour (type);
    g.setColour (accent);
    g.fillRoundedRectangle (bounds.removeFromTop (4.0f).reduced (4.0f, 0.0f), 2.0f);

    paintLevelMeter (g, meterBounds.toFloat(), displayLevel, displayPeakHold);
}

void MixerChannelStrip::resized()
{
    // Scale controls with the strip so faders always fill the mixer window height.
    auto bounds = getLocalBounds().reduced (juce::jmax (3, getWidth() / 12),
                                            juce::jmax (4, getHeight() / 40));

    const auto nameH = juce::jlimit (18, 32, getHeight() / 12);
    const auto btnH = juce::jlimit (22, 36, getHeight() / 11);
    const auto levelH = juce::jlimit (14, 20, getHeight() / 18);

    nameLabel.setBounds (bounds.removeFromTop (nameH));
    bounds.removeFromTop (juce::jmax (2, getHeight() / 80));

    auto buttonRow = bounds.removeFromTop (btnH);
    muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (2));
    soloButton.setBounds (buttonRow.reduced (2));
    bounds.removeFromTop (juce::jmax (4, getHeight() / 60));

    levelLabel.setBounds (bounds.removeFromBottom (levelH));
    bounds.removeFromBottom (juce::jmax (2, getHeight() / 80));

    // Fader + meter side-by-side — leave room for realistic cap width.
    const auto meterW = juce::jlimit (8, 14, bounds.getWidth() / 4);
    meterBounds = bounds.removeFromRight (meterW).reduced (1, 2);
    bounds.removeFromRight (juce::jmax (2, getWidth() / 20));

    const auto sidePad = juce::jmax (1, getWidth() / 18);
    volumeSlider.setBounds (bounds.reduced (sidePad, 2));
}

} // namespace jamstudio::ui
