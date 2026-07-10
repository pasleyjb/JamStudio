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
};

} // namespace jamstudio::audio
