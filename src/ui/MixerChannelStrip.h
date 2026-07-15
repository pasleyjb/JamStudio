#pragma once

#include "../audio/MixBus.h"
#include "../audio/StemMixer.h"
#include "IndicatorButton.h"

#include <array>

namespace jamstudio::ui
{

/**
 * Vertical DAW channel: M/S, FOH + Mon 1–5 sends, peak meter.
 * FOH = house PA; M1–M5 = band / IEM mixes (independent levels).
 */
class MixerChannelStrip : public juce::Component,
                          private juce::Timer
{
public:
    using StemChangedCallback = std::function<void (int index)>;

    MixerChannelStrip (int stemIndex,
                       jamstudio::audio::StemMixer& mixer,
                       StemChangedCallback onChanged);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void syncFromTrack (const jamstudio::audio::StemTrack& track);

private:
    void notifyChanged();
    void applyControlsToMixer();
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

    std::array<juce::Label, jamstudio::audio::kNumMixBuses> busLabels;
    std::array<juce::Slider, jamstudio::audio::kNumMixBuses> busSliders;

    juce::Label levelLabel { {}, "0" };
    juce::Rectangle<int> meterBounds;
    float displayLevel = 0.0f;
    float displayPeakHold = 0.0f;
    StemChangedCallback onStemChanged;
};

} // namespace jamstudio::ui
