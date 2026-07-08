#pragma once

#include "../audio/TransportController.h"
#include "IndicatorButton.h"

namespace jamstudio::ui
{

/** Slim Audacity-style transport row with 3D controls and metronome LED. */
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
    void updateMetronomeIndicator();

    jamstudio::audio::TransportController& transportController;

    IndicatorButton playButton { "play", "Play" };
    IndicatorButton pauseButton { "pause", "Pause" };
    IndicatorButton stopButton { "stop", "Stop" };
    juce::Slider positionSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label positionLabel;
    IndicatorButton metronomeButton { "metronome", "Metro" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label bpmLabel { {}, "BPM" };
};

} // namespace jamstudio::ui