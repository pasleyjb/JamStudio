#include "AudioRecorder.h"

#include <cmath>

namespace jamstudio::audio
{

AudioRecorder::AudioRecorder()
{
    backgroundThread.startThread();
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
}

bool AudioRecorder::startRecording (const juce::File& destinationFile)
{
    stopRecording();

    if (sampleRate <= 0.0)
        return false;

    recordingFile = destinationFile;
    destinationFile.getParentDirectory().createDirectory();
    destinationFile.deleteFile();

    if (auto fileStream = std::unique_ptr<juce::OutputStream> (destinationFile.createOutputStream()))
    {
        juce::WavAudioFormat wavFormat;

        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate (sampleRate)
                                 .withNumChannels (1)
                                 .withBitsPerSample (16);

        if (auto writer = wavFormat.createWriterFor (fileStream, options))
        {
            threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (
                writer.release(), backgroundThread, 32768);

            const juce::ScopedLock lock (writerLock);
            activeWriter.store (threadedWriter.get());
            return true;
        }
    }

    recordingFile = juce::File();
    return false;
}

juce::File AudioRecorder::stopRecording()
{
    {
        const juce::ScopedLock lock (writerLock);
        activeWriter.store (nullptr);
    }

    threadedWriter.reset();
    return recordingFile;
}

bool AudioRecorder::isRecording() const noexcept
{
    return activeWriter.load() != nullptr;
}

void AudioRecorder::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
}

void AudioRecorder::audioDeviceStopped()
{
    sampleRate = 0.0;
    inputLevel.store (0.0f, std::memory_order_relaxed);
}

void AudioRecorder::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      const int numInputChannels,
                                                      float* const* outputChannelData,
                                                      const int numOutputChannels,
                                                      const int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (outputChannelData, numOutputChannels, context);

    float peak = 0.0f;

    if (numInputChannels > 0 && inputChannelData != nullptr && inputChannelData[0] != nullptr)
    {
        const auto* in = inputChannelData[0];
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (in[i]));
    }

    const auto prev = inputLevel.load (std::memory_order_relaxed);
    inputLevel.store (peak >= prev ? peak : prev * 0.88f, std::memory_order_relaxed);

    const juce::ScopedLock lock (writerLock);

    if (auto* writer = activeWriter.load())
    {
        if (numInputChannels > 0 && inputChannelData[0] != nullptr)
        {
            const float* channelPointers[] = { inputChannelData[0] };
            writer->write (channelPointers, numSamples);
        }
    }
}

} // namespace jamstudio::audio
