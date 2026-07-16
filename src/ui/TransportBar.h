#pragma once

#include "../audio/TransportController.h"
#include "IndicatorButton.h"
#include "TapeDeckButton.h"

namespace jamstudio::ui
{

/** Transport row with tape-deck play/pause/stop, record, scrubber, master, and tempo. */
class TransportBar : public juce::Component,
                     public juce::Timer
{
public:
    using DetectTempoCallback = std::function<void()>;
    using RecordCallback = std::function<void()>;

    explicit TransportBar (jamstudio::audio::TransportController& transport);

    void setDetectTempoCallback (DetectTempoCallback callback);
    void setRecordCallback (RecordCallback callback);
    void setInputLevelProvider (std::function<float()> provider);
    void setInputMonitorCallbacks (std::function<bool()> isEnabled,
                                   std::function<void (bool)> setEnabled,
                                   std::function<float()> getGain,
                                   std::function<void (float)> setGain,
                                   std::function<int()> getChannel = {},
                                   std::function<void (int)> setChannel = {});
    /** Refresh In1/In2/…/All list from how many device inputs are open. */
    void setInputMonitorChannelCount (int numOpenInputs);
    void setRecordingActive (bool recording);
    void setInputLevel (float level01);
    void syncInputMonitorUi();

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
    [[nodiscard]] bool isRecordingActive() const noexcept { return recordingActive; }

private:
    void updateMetronomeIndicator();
    void updateCountInIndicator();
    void updateTransportIndicators();
    void updateRecordIndicator();

    jamstudio::audio::TransportController& transportController;
    DetectTempoCallback detectTempoCallback;
    RecordCallback recordCallback;
    std::function<float()> inputLevelProvider;
    std::function<bool()> inputMonitorIsEnabled;
    std::function<void (bool)> inputMonitorSetEnabled;
    std::function<float()> inputMonitorGetGain;
    std::function<void (float)> inputMonitorSetGain;
    std::function<int()> inputMonitorGetChannel;
    std::function<void (int)> inputMonitorSetChannel;

    TapeDeckButton skipBackButton { "skipBack", TapeDeckButton::Icon::skipBack };
    TapeDeckButton playButton { "play", TapeDeckButton::Icon::play };
    TapeDeckButton pauseButton { "pause", TapeDeckButton::Icon::pause };
    TapeDeckButton stopButton { "stop", TapeDeckButton::Icon::stop };
    TapeDeckButton skipForwardButton { "skipForward", TapeDeckButton::Icon::skipForward };
    IndicatorButton recordButton { "record", "REC" };
    juce::Slider positionSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label positionLabel;
    juce::Label masterLabel { {}, "Master" };
    juce::Slider masterVolumeSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    IndicatorButton countInButton { "countIn", "4-IN" };
    IndicatorButton metronomeButton { "metronome", "Metro" };
    juce::TextButton detectTempoButton { "Detect" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label bpmLabel { {}, "BPM" };
    juce::Label inputLabel { {}, "IN" };
    IndicatorButton inputMonitorButton { "inputMon", "MON" };
    juce::ComboBox inputMonitorChannelBox;
    juce::Slider inputMonitorGainSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Rectangle<int> inputMeterBounds;
    int inputMonitorChannelCount = 2;
    float inputLevel = 0.0f;
    bool recordingActive = false;
};

} // namespace jamstudio::ui
