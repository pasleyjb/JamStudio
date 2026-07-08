#pragma once

#include "../audio/StemTrack.h"

namespace jamstudio::ui
{

/** Mixer strip for a single stem: label, mute, solo, volume. */
class StemStrip : public juce::Component
{
public:
    using StemChangedCallback = std::function<void (int index, bool muted, bool solo, float volume)>;

    StemStrip (int stemIndex, const jamstudio::audio::StemTrack& track, StemChangedCallback onChanged);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int index;
    juce::Label nameLabel;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::Slider volumeSlider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    StemChangedCallback onStemChanged;
};

} // namespace jamstudio::ui