#pragma once

#include "../audio/TransportController.h"
#include "IndicatorButton.h"

namespace jamstudio::ui
{

/** Slim Audacity-style transport row with master volume and tempo controls. */
class TransportBar : public juce::Component,
                     public juce::Timer
{
public:
    using DetectTempoCallback = std::function<void()>;

    explicit TransportBar (jamstudio::audio::TransportController& transport);

    void setDetectTempoCallback (DetectTempoCallback callback);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void updatePositionSlider();

    [[nodiscard]] bool isMetronomeEnabled() const noexcept;
    void setMetronomeEnabled (bool enabled);
    [[nodiscard]] double getBpm() const noexcept;
    void setBpm (double bpm);
    void setMasterVolume (float volume);
    [[nodiscard]] float getMasterVolume() const noexcept;

private:
    void updateMetronomeIndicator();

    jamstudio::audio::TransportController& transportController;
    DetectTempoCallback detectTempoCallback;

    IndicatorButton playButton { "play", "Play" };
    IndicatorButton pauseButton { "pause", "Pause" };
    IndicatorButton stopButton { "stop", "Stop" };
    juce::Slider positionSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label positionLabel;
    juce::Label masterLabel { {}, "Master" };
    juce::Slider masterVolumeSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    IndicatorButton metronomeButton { "metronome", "Metro" };
    juce::TextButton detectTempoButton { "Detect" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label bpmLabel { {}, "BPM" };
};

} // namespace jamstudio::ui