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

void Metronome::setCountInFinishedCallback (CountInFinishedCallback cb)
{
    countInFinishedCallback = std::move (cb);
}

void Metronome::startCountIn (const int beats)
{
    countInBeatsTarget = juce::jmax (1, beats);
    countInFinishPosted = false;
    sampleCounter = 0;
    clickSamplesRemaining = 0;
    countingIn.store (true, std::memory_order_relaxed);
}

void Metronome::cancelCountIn() noexcept
{
    countingIn.store (false, std::memory_order_relaxed);
    countInFinishPosted = false;
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

    if ((! enabled && ! countingIn.load (std::memory_order_relaxed))
        || sampleRate <= 0.0
        || bpm <= 0.0)
        return;

    renderClick (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void Metronome::finishCountInIfNeeded (const int64 samplesPerBeat)
{
    if (! countingIn.load (std::memory_order_relaxed) || countInFinishPosted)
        return;

    if (sampleCounter < static_cast<int64> (countInBeatsTarget) * samplesPerBeat)
        return;

    countInFinishPosted = true;
    countingIn.store (false, std::memory_order_relaxed);
    clickSamplesRemaining = 0;

    if (countInFinishedCallback != nullptr)
    {
        juce::MessageManager::callAsync ([cb = countInFinishedCallback]
        {
            if (cb != nullptr)
                cb();
        });
    }
}

void Metronome::renderClick (juce::AudioBuffer<float>& buffer, const int startSample, const int numSamples)
{
    const auto samplesPerBeat = static_cast<int64> (sampleRate * 60.0 / bpm);

    if (samplesPerBeat <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        finishCountInIfNeeded (samplesPerBeat);

        const auto inCountIn = countingIn.load (std::memory_order_relaxed);
        const auto shouldClick = enabled || inCountIn;

        if (! shouldClick)
        {
            ++sampleCounter;
            continue;
        }

        if (clickSamplesRemaining > 0)
        {
            const auto envelope = static_cast<float> (clickSamplesRemaining)
                                  / static_cast<float> (clickLengthSamples);
            const auto beatIndex = samplesPerBeat > 0 ? (sampleCounter / samplesPerBeat) : 0;
            const auto isAccent = (sampleCounter % samplesPerBeat) == 0
                                  && (beatIndex % 4) == 0;
            const auto frequency = isAccent ? accentFrequency : beatFrequency;
            const auto phase = static_cast<float> (sampleCounter % clickLengthSamples)
                               / static_cast<float> (sampleRate);
            const auto value = std::sin (juce::MathConstants<float>::twoPi * frequency * phase)
                               * envelope * volume;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.addSample (channel, startSample + sample, value);

            --clickSamplesRemaining;
        }

        // Do not start a click on the sample that ends count-in.
        if (sampleCounter % samplesPerBeat == 0
            && (inCountIn || enabled)
            && ! (inCountIn && sampleCounter >= static_cast<int64> (countInBeatsTarget) * samplesPerBeat))
        {
            clickSamplesRemaining = clickLengthSamples;
        }

        ++sampleCounter;
    }

    finishCountInIfNeeded (samplesPerBeat);
}

} // namespace jamstudio::audio
