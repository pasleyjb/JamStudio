#pragma once

#include "../audio/TransportController.h"
#include "IndicatorButton.h"
#include "TapeDeckButton.h"

namespace jamstudio::ui
{

/** Transport row with tape-deck play/pause/stop, scrubber, master, and tempo. */
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
    void updateTransportIndicators();

    jamstudio::audio::TransportController& transportController;
    DetectTempoCallback detectTempoCallback;

    TapeDeckButton playButton { "play", TapeDeckButton::Icon::play };
    TapeDeckButton pauseButton { "pause", TapeDeckButton::Icon::pause };
    TapeDeckButton stopButton { "stop", TapeDeckButton::Icon::stop };
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
