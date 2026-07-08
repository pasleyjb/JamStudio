#include "StemStrip.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

StemStrip::StemStrip (const int stemIndex,
                      const jamstudio::audio::StemTrack& track,
                      juce::AudioFormatManager& formatManager,
                      juce::AudioThumbnailCache& thumbnailCache,
                      jamstudio::audio::TransportController& transport,
                      StemChangedCallback onChanged)
    : index (stemIndex),
      miniWaveform (formatManager, thumbnailCache, transport, track.getFile()),
      onStemChanged (std::move (onChanged))
{
    nameLabel.setText (jamstudio::audio::stemTypeToString (track.getType()), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (nameLabel);

    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (track.isMuted(), juce::dontSendNotification);
    muteButton.setIndicatorColour (JamStudioTheme::getColours().indicatorMute);
    muteButton.onClick = [this] { notifyChanged(); updateIndicators(); };
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setToggleState (track.isSolo(), juce::dontSendNotification);
    soloButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
    soloButton.onClick = [this] { notifyChanged(); updateIndicators(); };
    addAndMakeVisible (soloButton);

    addAndMakeVisible (miniWaveform);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (track.getVolume(), juce::dontSendNotification);
    volumeSlider.onValueChange = [this] { notifyChanged(); };
    addAndMakeVisible (volumeSlider);

    updateIndicators();
}

void StemStrip::notifyChanged()
{
    if (onStemChanged != nullptr)
        onStemChanged (index, muteButton.getToggleState(), soloButton.getToggleState(),
                       static_cast<float> (volumeSlider.getValue()));
}

void StemStrip::updateIndicators()
{
    muteButton.setIndicatorActive (muteButton.getToggleState());
    soloButton.setIndicatorActive (soloButton.getToggleState());
}

void StemStrip::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.trackBackground);
    g.setColour (colours.border);
    g.drawRect (getLocalBounds(), 1);
}

void StemStrip::resized()
{
    auto bounds = getLocalBounds().reduced (4, 2);

    muteButton.setBounds (bounds.removeFromLeft (36));
    bounds.removeFromLeft (3);
    soloButton.setBounds (bounds.removeFromLeft (36));
    bounds.removeFromLeft (6);
    nameLabel.setBounds (bounds.removeFromLeft (68));
    bounds.removeFromLeft (6);
    volumeSlider.setBounds (bounds.removeFromRight (64));
    bounds.removeFromRight (6);
    miniWaveform.setBounds (bounds);
}

} // namespace jamstudio::ui