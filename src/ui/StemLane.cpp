#include "StemLane.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

StemLane::StemLane (const int stemIndex,
                    jamstudio::audio::StemMixer& mixer,
                    juce::AudioFormatManager& formatManager,
                    juce::AudioThumbnailCache& thumbnailCache,
                    jamstudio::audio::TransportController& transport)
    : index (stemIndex),
      stemMixer (mixer),
      miniWaveform (formatManager, thumbnailCache, transport,
                    mixer.getStem (stemIndex) != nullptr ? mixer.getStem (stemIndex)->getFile()
                                                         : juce::File())
{
    const auto* track = stemMixer.getStem (index);
    const auto armed = track != nullptr && track->isRecordArmed();

    if (track != nullptr)
    {
        type = track->getType();
        nameLabel.setText (track->getName().isNotEmpty()
                               ? track->getName()
                               : jamstudio::audio::stemTypeToString (track->getType()),
                           juce::dontSendNotification);
    }

    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (nameLabel);

    recordButton.setClickingTogglesState (true);
    recordButton.setToggleState (armed, juce::dontSendNotification);
    recordButton.setIndicatorColour (juce::Colour (0xffff3344));
    recordButton.setIndicatorActive (armed);
    recordButton.setTooltip ("Print this stem into the take");
    recordButton.onClick = [this]
    {
        stemMixer.setStemRecordArmed (index, recordButton.getToggleState());
        recordButton.setIndicatorActive (recordButton.getToggleState());
    };
    addAndMakeVisible (recordButton);
    addAndMakeVisible (miniWaveform);
}

void StemLane::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.trackBackground);

    const auto accent = jamstudio::audio::stemTypeColour (type);
    g.setColour (accent);
    g.fillRect (0, 0, 4, getHeight());

    g.setColour (colours.border.withAlpha (0.65f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void StemLane::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft (8);
    nameLabel.setBounds (bounds.removeFromLeft (72).reduced (0, 4));
    recordButton.setBounds (bounds.removeFromLeft (34).reduced (1, 8));
    bounds.removeFromLeft (2);
    miniWaveform.setBounds (bounds.reduced (2, 3));
}

} // namespace jamstudio::ui
