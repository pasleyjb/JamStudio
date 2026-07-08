#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/** Generates a click track aligned to a BPM grid. */
class Metronome : public juce::AudioSource
{
public:
    void setEnabled (bool shouldEnable) noexcept;
    void setBpm (double newBpm) noexcept;
    void setVolume (float newVolume) noexcept;
    void reset();

    [[nodiscard]] bool isEnabled() const noexcept { return enabled; }
    [[nodiscard]] double getBpm() const noexcept { return bpm; }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    void renderClick (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    bool enabled = false;
    double bpm = 120.0;
    double sampleRate = 44100.0;
    float volume = 0.5f;
    int64 sampleCounter = 0;
    int clickSamplesRemaining = 0;
};

} // namespace jamstudio::audio