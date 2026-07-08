#include "StemStrip.h"

namespace jamstudio::ui
{

StemStrip::StemStrip (const int stemIndex,
                      const jamstudio::audio::StemTrack& track,
                      StemChangedCallback onChanged)
    : index (stemIndex),
      onStemChanged (std::move (onChanged))
{
    nameLabel.setText (jamstudio::audio::stemTypeToString (track.getType()), juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (nameLabel);

    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (track.isMuted(), juce::dontSendNotification);
    muteButton.onClick = [this]
    {
        if (onStemChanged != nullptr)
            onStemChanged (index, muteButton.getToggleState(), soloButton.getToggleState(),
                           static_cast<float> (volumeSlider.getValue()));
    };
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setToggleState (track.isSolo(), juce::dontSendNotification);
    soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
    soloButton.onClick = [this]
    {
        if (onStemChanged != nullptr)
            onStemChanged (index, muteButton.getToggleState(), soloButton.getToggleState(),
                           static_cast<float> (volumeSlider.getValue()));
    };
    addAndMakeVisible (soloButton);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (track.getVolume(), juce::dontSendNotification);
    volumeSlider.onValueChange = [this]
    {
        if (onStemChanged != nullptr)
            onStemChanged (index, muteButton.getToggleState(), soloButton.getToggleState(),
                           static_cast<float> (volumeSlider.getValue()));
    };
    addAndMakeVisible (volumeSlider);
}

void StemStrip::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2a2a));
    g.setColour (juce::Colour (0xff3d3d3d));
    g.drawRect (getLocalBounds(), 1);
}

void StemStrip::resized()
{
    auto bounds = getLocalBounds().reduced (6);

    nameLabel.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (4);

    auto buttonRow = bounds.removeFromTop (28);
    muteButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (2));
    soloButton.setBounds (buttonRow.reduced (2));

    bounds.removeFromTop (4);
    volumeSlider.setBounds (bounds);
}

} // namespace jamstudio::ui