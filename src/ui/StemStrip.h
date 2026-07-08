#pragma once

#include "../audio/StemTrack.h"
#include "IndicatorButton.h"

namespace jamstudio::ui
{

/** Audacity-style horizontal track row with 3D mute/solo buttons and status LEDs. */
class StemStrip : public juce::Component
{
public:
    using StemChangedCallback = std::function<void (int index, bool muted, bool solo, float volume)>;

    StemStrip (int stemIndex, const jamstudio::audio::StemTrack& track, StemChangedCallback onChanged);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateIndicators();

private:
    void notifyChanged();

    int index;
    juce::Label nameLabel;
    IndicatorButton muteButton { "mute", "M" };
    IndicatorButton soloButton { "solo", "S" };
    juce::Slider volumeSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    StemChangedCallback onStemChanged;
};

} // namespace jamstudio::ui