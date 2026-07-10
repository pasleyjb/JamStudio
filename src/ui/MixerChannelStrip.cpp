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
        fohSlider.setValue (track->getBusSend (jamstudio::audio::MixBus::foh), juce::dontSendNotification);
        monASlider.setValue (track->getBusSend (jamstudio::audio::MixBus::monitorA), juce::dontSendNotification);
        monBSlider.setValue (track->getBusSend (jamstudio::audio::MixBus::monitorB), juce::dontSendNotification);
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

    auto setupFader = [this] (juce::Slider& s, juce::Label& lab, juce::Colour accent)
    {
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
            levelLabel.setText (juce::String (static_cast<int> (fohSlider.getValue() * 100)),
                                juce::dontSendNotification);
            applyControlsToMixer();
        };
        addAndMakeVisible (s);
    };

    setupFader (fohSlider, fohLabel, jamstudio::audio::mixBusColour (jamstudio::audio::MixBus::foh));
    setupFader (monASlider, monALabel, jamstudio::audio::mixBusColour (jamstudio::audio::MixBus::monitorA));
    setupFader (monBSlider, monBLabel, jamstudio::audio::mixBusColour (jamstudio::audio::MixBus::monitorB));

    fohSlider.setTooltip ("Front of house / PA send");
    monASlider.setTooltip ("Monitor / IEM mix A send");
    monBSlider.setTooltip ("Monitor / IEM mix B send");

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
    fohSlider.setValue (track.getBusSend (jamstudio::audio::MixBus::foh), juce::dontSendNotification);
    monASlider.setValue (track.getBusSend (jamstudio::audio::MixBus::monitorA), juce::dontSendNotification);
    monBSlider.setValue (track.getBusSend (jamstudio::audio::MixBus::monitorB), juce::dontSendNotification);
    levelLabel.setText (juce::String (static_cast<int> (track.getVolume() * 100)),
                        juce::dontSendNotification);
    updateIndicators();
    repaint();
}

void MixerChannelStrip::applyControlsToMixer()
{
    stemMixer.setStemMuted (index, muteButton.getToggleState());
    stemMixer.setStemSolo (index, soloButton.getToggleState());
    stemMixer.setStemBusSend (index, jamstudio::audio::MixBus::foh,
                              static_cast<float> (fohSlider.getValue()));
    stemMixer.setStemBusSend (index, jamstudio::audio::MixBus::monitorA,
                              static_cast<float> (monASlider.getValue()));
    stemMixer.setStemBusSend (index, jamstudio::audio::MixBus::monitorB,
                              static_cast<float> (monBSlider.getValue()));
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
    auto bounds = getLocalBounds().reduced (juce::jmax (2, getWidth() / 16),
                                            juce::jmax (3, getHeight() / 50));

    const auto nameH = juce::jlimit (16, 28, getHeight() / 14);
    const auto btnH = juce::jlimit (20, 32, getHeight() / 12);
    const auto levelH = juce::jlimit (12, 18, getHeight() / 20);
    const auto labH = juce::jlimit (12, 16, getHeight() / 28);

    nameLabel.setBounds (bounds.removeFromTop (nameH));
    bounds.removeFromTop (2);

    auto buttonRow = bounds.removeFromTop (btnH);
    muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (1));
    soloButton.setBounds (buttonRow.reduced (1));
    bounds.removeFromTop (3);

    levelLabel.setBounds (bounds.removeFromBottom (levelH));
    bounds.removeFromBottom (2);

    const auto meterW = juce::jlimit (7, 12, bounds.getWidth() / 8);
    meterBounds = bounds.removeFromRight (meterW).reduced (1, 2);
    bounds.removeFromRight (2);

    // Three send columns: FOH | Mon A | Mon B
    const auto colW = bounds.getWidth() / 3;
    auto fohCol = bounds.removeFromLeft (colW);
    auto monACol = bounds.removeFromLeft (colW);
    auto monBCol = bounds;

    fohLabel.setBounds (fohCol.removeFromTop (labH));
    fohSlider.setBounds (fohCol.reduced (1, 1));

    monALabel.setBounds (monACol.removeFromTop (labH));
    monASlider.setBounds (monACol.reduced (1, 1));

    monBLabel.setBounds (monBCol.removeFromTop (labH));
    monBSlider.setBounds (monBCol.reduced (1, 1));
}

} // namespace jamstudio::ui
