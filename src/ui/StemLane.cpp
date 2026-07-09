#include "StemLane.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

StemLane::StemLane (const int stemIndex,
                    const jamstudio::audio::StemTrack& track,
                    juce::AudioFormatManager& formatManager,
                    juce::AudioThumbnailCache& thumbnailCache,
                    jamstudio::audio::TransportController& transport)
    : index (stemIndex),
      type (track.getType()),
      miniWaveform (formatManager, thumbnailCache, transport, track.getFile())
{
    juce::ignoreUnused (index);
    nameLabel.setText (track.getName().isNotEmpty()
                           ? track.getName()
                           : jamstudio::audio::stemTypeToString (track.getType()),
                       juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (nameLabel);
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
    bounds.removeFromLeft (4);
    miniWaveform.setBounds (bounds.reduced (2, 3));
}

} // namespace jamstudio::ui
