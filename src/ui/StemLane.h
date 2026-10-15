#pragma once

#include "../audio/StemMixer.h"
#include "../audio/TransportController.h"
#include "IndicatorButton.h"
#include "StemMiniWaveform.h"

namespace jamstudio::ui
{

/** Horizontal stem waveform lane with a record-arm button. Levels live in the mixer. */
class StemLane : public juce::Component
{
public:
    StemLane (int stemIndex,
              jamstudio::audio::StemMixer& mixer,
              juce::AudioFormatManager& formatManager,
              juce::AudioThumbnailCache& thumbnailCache,
              jamstudio::audio::TransportController& transport);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int index = 0;
    jamstudio::audio::StemMixer& stemMixer;
    jamstudio::audio::StemType type = jamstudio::audio::StemType::unknown;
    juce::Label nameLabel;
    IndicatorButton recordButton { "recordArm", "R" };
    StemMiniWaveform miniWaveform;
};

} // namespace jamstudio::ui
