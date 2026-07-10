#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>

namespace jamstudio::audio
{

/** Generates a click track aligned to a BPM grid, plus optional N-beat count-in. */
class Metronome : public juce::AudioSource
{
public:
    using CountInFinishedCallback = std::function<void()>;

    void setEnabled (bool shouldEnable) noexcept;
    void setBpm (double newBpm) noexcept;
    void setVolume (float newVolume) noexcept;
    void reset();

    /** Play exactly `beats` clicks, then invoke the finished callback (message thread). */
    void startCountIn (int beats);
    void cancelCountIn() noexcept;
    [[nodiscard]] bool isCountingIn() const noexcept { return countingIn.load (std::memory_order_relaxed); }

    void setCountInFinishedCallback (CountInFinishedCallback cb);

    [[nodiscard]] bool isEnabled() const noexcept { return enabled; }
    [[nodiscard]] double getBpm() const noexcept { return bpm; }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    void renderClick (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void finishCountInIfNeeded (int64 samplesPerBeat);

    bool enabled = false;
    double bpm = 120.0;
    double sampleRate = 44100.0;
    float volume = 0.5f;
    int64 sampleCounter = 0;
    int clickSamplesRemaining = 0;

    std::atomic<bool> countingIn { false };
    int countInBeatsTarget = 4;
    bool countInFinishPosted = false;
    CountInFinishedCallback countInFinishedCallback;
};

} // namespace jamstudio::audio
