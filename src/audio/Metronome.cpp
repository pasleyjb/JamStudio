#include "Metronome.h"

namespace jamstudio::audio
{

namespace
{
constexpr int clickLengthSamples = 1200;
constexpr float accentFrequency = 1500.0f;
constexpr float beatFrequency = 1000.0f;
} // namespace

void Metronome::setEnabled (const bool shouldEnable) noexcept
{
    enabled = shouldEnable;
}

void Metronome::setBpm (const double newBpm) noexcept
{
    bpm = juce::jmax (20.0, newBpm);
}

void Metronome::setVolume (const float newVolume) noexcept
{
    volume = juce::jlimit (0.0f, 1.0f, newVolume);
}

void Metronome::reset()
{
    sampleCounter = 0;
    clickSamplesRemaining = 0;
}

void Metronome::prepareToPlay (const int /*samplesPerBlockExpected*/, const double newSampleRate)
{
    sampleRate = newSampleRate;
}

void Metronome::releaseResources() {}

void Metronome::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (! enabled || sampleRate <= 0.0 || bpm <= 0.0)
        return;

    renderClick (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void Metronome::renderClick (juce::AudioBuffer<float>& buffer, const int startSample, const int numSamples)
{
    const auto samplesPerBeat = static_cast<int64> (sampleRate * 60.0 / bpm);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (clickSamplesRemaining > 0)
        {
            const auto envelope = static_cast<float> (clickSamplesRemaining) / static_cast<float> (clickLengthSamples);
            const auto isAccent = (sampleCounter % samplesPerBeat) == 0
                                  && (sampleCounter / samplesPerBeat) % 4 == 0;
            const auto frequency = isAccent ? accentFrequency : beatFrequency;
            const auto phase = static_cast<float> (sampleCounter % clickLengthSamples) / static_cast<float> (sampleRate);
            const auto value = std::sin (juce::MathConstants<float>::twoPi * frequency * phase) * envelope * volume;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.addSample (channel, startSample + sample, value);

            --clickSamplesRemaining;
        }

        if (sampleCounter % samplesPerBeat == 0)
            clickSamplesRemaining = clickLengthSamples;

        ++sampleCounter;
    }
}

} // namespace jamstudio::audio