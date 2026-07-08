#pragma once

#include "../audio/TransportController.h"

namespace jamstudio::ui
{

/** Transport controls: play, pause, stop, position, metronome, BPM. */
class TransportBar : public juce::Component,
                     public juce::Timer
{
public:
    TransportBar (jamstudio::audio::TransportController& transport);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void updatePositionSlider();

    [[nodiscard]] bool isMetronomeEnabled() const noexcept;
    void setMetronomeEnabled (bool enabled);
    [[nodiscard]] double getBpm() const noexcept;
    void setBpm (double bpm);

private:
    jamstudio::audio::TransportController& transportController;

    juce::TextButton playButton { "Play" };
    juce::TextButton pauseButton { "Pause" };
    juce::TextButton stopButton { "Stop" };
    juce::Slider positionSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label positionLabel;
    juce::ToggleButton metronomeButton { "Metronome" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label bpmLabel { {}, "BPM" };
};

} // namespace jamstudio::ui