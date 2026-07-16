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

void AudioRecorder::setInputMonitorEnabled (const bool shouldEnable) noexcept
{
    inputMonitorEnabled.store (shouldEnable, std::memory_order_relaxed);
}

bool AudioRecorder::isInputMonitorEnabled() const noexcept
{
    return inputMonitorEnabled.load (std::memory_order_relaxed);
}

void AudioRecorder::setInputMonitorGain (const float gain01) noexcept
{
    inputMonitorGain.store (juce::jlimit (0.0f, 1.5f, gain01), std::memory_order_relaxed);
}

float AudioRecorder::getInputMonitorGain() const noexcept
{
    return inputMonitorGain.load (std::memory_order_relaxed);
}

void AudioRecorder::setInputMonitorChannel (const int channelIndex) noexcept
{
    if (channelIndex == kMonitorAllGated)
        inputMonitorChannel.store (kMonitorAllGated, std::memory_order_relaxed);
    else if (channelIndex < 0)
        inputMonitorChannel.store (kMonitorFirstTwo, std::memory_order_relaxed);
    else
        inputMonitorChannel.store (channelIndex, std::memory_order_relaxed);
}

int AudioRecorder::getInputMonitorChannel() const noexcept
{
    return inputMonitorChannel.load (std::memory_order_relaxed);
}

void AudioRecorder::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      const int numInputChannels,
                                                      float* const* outputChannelData,
                                                      const int numOutputChannels,
                                                      const int numSamples,
                                                      const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (context);

    // This callback's output is mixed with transport stems by the device manager.
    // Always start from silence so we never leave garbage in our contribution.
    if (outputChannelData != nullptr)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
    }

    const int monCh = inputMonitorChannel.load (std::memory_order_relaxed);
    const bool firstTwo = (monCh == kMonitorFirstTwo);
    const bool allGated = (monCh == kMonitorAllGated);
    const bool multi = firstTwo || allGated;
    const float* singleIn = nullptr;
    if (! multi && inputChannelData != nullptr && monCh >= 0 && monCh < numInputChannels)
        singleIn = inputChannelData[monCh];

    // Single-pass peak for meter + gate (avoid scanning each channel 2–3 times).
    constexpr float kNoiseGate = 0.0045f;
    constexpr int kMaxMonCh = 8;
    float chPeak[kMaxMonCh] {};
    float peak = 0.0f;
    const int monLimit = multi
                             ? (firstTwo ? juce::jmin (2, numInputChannels)
                                         : juce::jmin (kMaxMonCh, numInputChannels))
                             : 0;

    if (inputChannelData != nullptr && numInputChannels > 0 && numSamples > 0)
    {
        if (multi)
        {
            for (int c = 0; c < monLimit; ++c)
            {
                const auto* src = inputChannelData[c];
                if (src == nullptr)
                    continue;
                float p = 0.0f;
                for (int i = 0; i < numSamples; ++i)
                    p = juce::jmax (p, std::abs (src[i]));
                chPeak[c] = p;
                peak = juce::jmax (peak, p);
            }
        }
        else
        {
            const auto* src = singleIn != nullptr ? singleIn : inputChannelData[0];
            if (src != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                    peak = juce::jmax (peak, std::abs (src[i]));
            }
        }
    }

    const auto prev = inputLevel.load (std::memory_order_relaxed);
    inputLevel.store (peak >= prev ? peak : prev * 0.88f, std::memory_order_relaxed);

    // ---- Software input monitor (guitar, singing, or both) ----
    if (inputMonitorEnabled.load (std::memory_order_relaxed)
        && inputChannelData != nullptr
        && outputChannelData != nullptr
        && numOutputChannels > 0
        && numSamples > 0
        && numInputChannels > 0)
    {
        const float gain = inputMonitorGain.load (std::memory_order_relaxed);
        if (gain > 0.0001f)
        {
            auto addToOuts = [&] (const float* src, const float g)
            {
                if (src == nullptr || g <= 0.0f)
                    return;
                for (int ch = 0; ch < numOutputChannels; ++ch)
                    if (outputChannelData[ch] != nullptr)
                        juce::FloatVectorOperations::addWithMultiply (outputChannelData[ch], src, g, numSamples);
            };

            if (multi)
            {
                int active = 0;
                for (int c = 0; c < monLimit; ++c)
                    if (chPeak[c] >= kNoiseGate)
                        ++active;

                const float scale = gain * (active > 1 ? 0.65f : 1.0f);
                for (int c = 0; c < monLimit; ++c)
                {
                    if (chPeak[c] < kNoiseGate)
                        continue;
                    addToOuts (inputChannelData[c], scale);
                }
            }
            else if (singleIn != nullptr && peak >= kNoiseGate * 0.5f)
            {
                addToOuts (singleIn, gain);
            }
        }
    }

    const juce::ScopedLock lock (writerLock);

    if (auto* writer = activeWriter.load())
    {
        // Record the selected channel, or input 1 when summing
        const float* recIn = singleIn;
        if (recIn == nullptr && inputChannelData != nullptr && numInputChannels > 0)
            recIn = inputChannelData[0];
        if (recIn != nullptr)
        {
            const float* channelPointers[] = { recIn };
            writer->write (channelPointers, numSamples);
        }
    }
}

} // namespace jamstudio::audio
