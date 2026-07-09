#pragma once

#include "../audio/StemMixer.h"
#include "IndicatorButton.h"

namespace jamstudio::ui
{

/** Vertical DAW channel: colour bar, M/S, fader, old-school peak meter. */
class MixerChannelStrip : public juce::Component,
                          private juce::Timer
{
public:
    using StemChangedCallback = std::function<void (int index, bool muted, bool solo, float volume)>;

    MixerChannelStrip (int stemIndex,
                       jamstudio::audio::StemMixer& mixer,
                       StemChangedCallback onChanged);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void syncFromTrack (const jamstudio::audio::StemTrack& track);

private:
    void notifyChanged();
    void updateIndicators();
    void timerCallback() override;
    void paintLevelMeter (juce::Graphics& g, juce::Rectangle<float> area,
                           float level, float peakHold) const;

    int index = 0;
    jamstudio::audio::StemMixer& stemMixer;
    jamstudio::audio::StemType type = jamstudio::audio::StemType::unknown;
    juce::Label nameLabel;
    IndicatorButton muteButton { "mute", "M" };
    IndicatorButton soloButton { "solo", "S" };
    juce::Slider volumeSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label levelLabel { {}, "0" };
    juce::Rectangle<int> meterBounds;
    float displayLevel = 0.0f;
    float displayPeakHold = 0.0f;
    StemChangedCallback onStemChanged;
};

} // namespace jamstudio::ui
