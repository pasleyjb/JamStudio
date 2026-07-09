#pragma once

#include "../audio/StemTrack.h"
#include "../audio/TransportController.h"
#include "StemMiniWaveform.h"

namespace jamstudio::ui
{

/** Horizontal stem waveform lane (controls live in the floating mixer). */
class StemLane : public juce::Component
{
public:
    StemLane (int stemIndex,
              const jamstudio::audio::StemTrack& track,
              juce::AudioFormatManager& formatManager,
              juce::AudioThumbnailCache& thumbnailCache,
              jamstudio::audio::TransportController& transport);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int index = 0;
    jamstudio::audio::StemType type = jamstudio::audio::StemType::unknown;
    juce::Label nameLabel;
    StemMiniWaveform miniWaveform;
};

} // namespace jamstudio::ui
