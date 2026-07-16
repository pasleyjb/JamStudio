#include "MixerChannelStrip.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

MixerChannelStrip::MixerChannelStrip (const int stemIndex,
                                      jamstudio::audio::StemMixer& mixer,
                                      jamstudio::audio::MultiBusMaster& multiBusMaster,
                                      StemChangedCallback onChanged)
    : index (stemIndex),
      stemMixer (mixer),
      multiBus (multiBusMaster),
      onStemChanged (std::move (onChanged))
{
    for (auto& s : busSliders)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }

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
        for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
            busSliders[static_cast<size_t> (b)].setValue (
                track->getBusSend (static_cast<jamstudio::audio::MixBus> (b)),
                juce::dontSendNotification);
        levelLabel.setText (juce::String (static_cast<int> (track->getVolume() * 100)),
                            juce::dontSendNotification);
    }

    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (nameLabel);

    muteButton.setClickingTogglesState (true);
    muteButton.setIndicatorColour (JamStudioTheme::getColours().indicatorMute);
    muteButton.onClick = [this] { applyControlsToMixer(); updateIndicators(); };
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
    soloButton.onClick = [this] { applyControlsToMixer(); updateIndicators(); };
    addAndMakeVisible (soloButton);

    for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
    {
        const auto bus = static_cast<jamstudio::audio::MixBus> (b);
        const auto accent = jamstudio::audio::mixBusColour (bus);
        auto& lab = busLabels[static_cast<size_t> (b)];
        auto& s = busSliders[static_cast<size_t> (b)];

        lab.setJustificationType (juce::Justification::centred);
        lab.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        lab.setColour (juce::Label::textColourId, accent);
        addAndMakeVisible (lab);

        s.setRange (0.0, 1.0, 0.01);
        s.setSliderSnapsToMousePosition (true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextValueSuffix (" %");
        s.setNumDecimalPlacesToDisplay (0);
        s.setColour (juce::Slider::thumbColourId, accent);
        s.setColour (juce::Slider::trackColourId, accent.withAlpha (0.35f));
        s.onValueChange = [this]
        {
            levelLabel.setText (
                juce::String (static_cast<int> (
                    busSliders[static_cast<size_t> (jamstudio::audio::MixBus::foh)].getValue() * 100)),
                juce::dontSendNotification);
            applyControlsToMixer();
        };
        addAndMakeVisible (s);
    }
    refreshBusLabels();

    levelLabel.setJustificationType (juce::Justification::centred);
    levelLabel.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (levelLabel);

    updateIndicators();
    startTimerHz (15);
}

void MixerChannelStrip::refreshBusLabels()
{
    for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
    {
        const auto bus = static_cast<jamstudio::audio::MixBus> (b);
        const auto name = multiBus.getBusDisplayName (bus);
        const auto longName = multiBus.getBusLongDisplayName (bus);
        busLabels[static_cast<size_t> (b)].setText (name, juce::dontSendNotification);
        busLabels[static_cast<size_t> (b)].setTooltip (longName);
        busSliders[static_cast<size_t> (b)].setTooltip (
            longName + "  (HW outs " + jamstudio::audio::mixBusHardwareOuts (bus) + ")");
    }
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
    for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        busSliders[static_cast<size_t> (b)].setValue (
            track.getBusSend (static_cast<jamstudio::audio::MixBus> (b)),
            juce::dontSendNotification);
    levelLabel.setText (juce::String (static_cast<int> (track.getVolume() * 100)),
                        juce::dontSendNotification);
    refreshBusLabels();
    updateIndicators();
    repaint();
}

void MixerChannelStrip::applyControlsToMixer()
{
    stemMixer.setStemMuted (index, muteButton.getToggleState());
    stemMixer.setStemSolo (index, soloButton.getToggleState());
    for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
        stemMixer.setStemBusSend (index,
                                  static_cast<jamstudio::audio::MixBus> (b),
                                  static_cast<float> (busSliders[static_cast<size_t> (b)].getValue()));
    notifyChanged();
}

void MixerChannelStrip::notifyChanged()
{
    if (onStemChanged != nullptr)
        onStemChanged (index);
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
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (area, 2.0f);
    g.setColour (JamStudioTheme::getColours().border.withAlpha (0.7f));
    g.drawRoundedRectangle (area, 2.0f, 1.0f);

    auto inner = area.reduced (1.5f);
    const auto h = inner.getHeight();
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
        fillSegment (0.0f, juce::jmin (lit, greenTop), juce::Colour (0xff22cc55));
        if (lit > greenTop)
            fillSegment (greenTop, juce::jmin (lit, yellowTop), juce::Colour (0xffffcc22));
        if (lit > yellowTop)
            fillSegment (yellowTop, lit, juce::Colour (0xffff3344));
    }

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
    auto bounds = getLocalBounds().reduced (juce::jmax (2, getWidth() / 20),
                                            juce::jmax (3, getHeight() / 50));

    const auto nameH = juce::jlimit (16, 28, getHeight() / 14);
    const auto btnH = juce::jlimit (20, 32, getHeight() / 12);
    const auto levelH = juce::jlimit (12, 18, getHeight() / 20);
    const auto labH = juce::jlimit (11, 15, getHeight() / 30);

    nameLabel.setBounds (bounds.removeFromTop (nameH));
    bounds.removeFromTop (2);

    auto buttonRow = bounds.removeFromTop (btnH);
    muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (1));
    soloButton.setBounds (buttonRow.reduced (1));
    bounds.removeFromTop (3);

    levelLabel.setBounds (bounds.removeFromBottom (levelH));
    bounds.removeFromBottom (2);

    const auto meterW = juce::jlimit (6, 10, bounds.getWidth() / 10);
    meterBounds = bounds.removeFromRight (meterW).reduced (1, 2);
    bounds.removeFromRight (1);

    // Six send columns: FOH | M1 | M2 | M3 | M4 | M5
    const auto colW = juce::jmax (1, bounds.getWidth() / jamstudio::audio::kNumMixBuses);
    for (int b = 0; b < jamstudio::audio::kNumMixBuses; ++b)
    {
        auto col = (b + 1 < jamstudio::audio::kNumMixBuses)
                       ? bounds.removeFromLeft (colW)
                       : bounds;
        busLabels[static_cast<size_t> (b)].setBounds (col.removeFromTop (labH));
        busSliders[static_cast<size_t> (b)].setBounds (col.reduced (0, 1));
    }
}

} // namespace jamstudio::ui
