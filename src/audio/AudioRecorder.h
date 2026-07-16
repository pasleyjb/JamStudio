#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace jamstudio::audio
{

/** Captures audio input to a WAV file while playback continues. */
class AudioRecorder : public juce::AudioIODeviceCallback
{
public:
    AudioRecorder();
    ~AudioRecorder() override;

    bool startRecording (const juce::File& destinationFile);
    juce::File stopRecording();
    [[nodiscard]] bool isRecording() const noexcept;
    [[nodiscard]] juce::File getCurrentRecordingFile() const noexcept { return recordingFile; }

    /** Live input envelope 0..1 (updates whenever the device callback runs). */
    [[nodiscard]] float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }

    /**
     * Software input monitor: mixes the live input into the speaker/headphone outs
     * so you can hear yourself with the backing tracks (Practice / any mode).
     * Default on. Mute or lower gain if using open mics + speakers (feedback).
     */
    void setInputMonitorEnabled (bool shouldEnable) noexcept;
    [[nodiscard]] bool isInputMonitorEnabled() const noexcept;
    void setInputMonitorGain (float gain01) noexcept;
    [[nodiscard]] float getInputMonitorGain() const noexcept;
    /**
     * Which hardware input to monitor.
     * 0..N-1 = that channel only (guitar, vocal mic, …).
     * kMonitorFirstTwo (-1) = In 1 + In 2 only (guitar + vocal; quieter than summing 8).
     * kMonitorAllGated (-2) = every open input, but quiet/empty jacks are gated out.
     */
    void setInputMonitorChannel (int channelIndex) noexcept;
    [[nodiscard]] int getInputMonitorChannel() const noexcept;
    static constexpr int kMonitorFirstTwo = -1;
    static constexpr int kMonitorAllGated = -2;
    /** @deprecated use kMonitorFirstTwo */
    static constexpr int kMonitorAllChannels = kMonitorFirstTwo;

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    juce::TimeSliceThread backgroundThread { "JamStudio Recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    juce::File recordingFile;
    double sampleRate = 44100.0;
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<bool> inputMonitorEnabled { true };
    std::atomic<float> inputMonitorGain { 0.45f };
    std::atomic<int> inputMonitorChannel { kMonitorFirstTwo };
};

} // namespace jamstudio::audio
