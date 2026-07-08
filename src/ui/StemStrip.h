#pragma once

#include "../audio/StemTrack.h"
#include "../audio/TransportController.h"
#include "IndicatorButton.h"
#include "StemMiniWaveform.h"

namespace jamstudio::ui
{

/** Audacity-style horizontal track row with waveform, mute/solo, and volume. */
class StemStrip : public juce::Component
{
public:
    using StemChangedCallback = std::function<void (int index, bool muted, bool solo, float volume)>;

    StemStrip (int stemIndex,
               const jamstudio::audio::StemTrack& track,
               juce::AudioFormatManager& formatManager,
               juce::AudioThumbnailCache& thumbnailCache,
               jamstudio::audio::TransportController& transport,
               StemChangedCallback onChanged);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateIndicators();

private:
    void notifyChanged();

    int index;
    juce::Label nameLabel;
    IndicatorButton muteButton { "mute", "M" };
    IndicatorButton soloButton { "solo", "S" };
    StemMiniWaveform miniWaveform;
    juce::Slider volumeSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    StemChangedCallback onStemChanged;
};

} // namespace jamstudio::ui