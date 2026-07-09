#include "StemTrack.h"

#include <cmath>

namespace jamstudio::audio
{

bool StemTrack::loadFromFile (const juce::File& file, juce::AudioFormatManager& formatManager)
{
    if (! file.existsAsFile())
        return false;

    auto* newReader = formatManager.createReaderFor (file);

    if (newReader == nullptr)
        return false;

    reader.reset (newReader);
    sourceFile = file;
    type = stemTypeFromFileName (file.getFileNameWithoutExtension());

    // Friendly mixer labels (e.g. "Guitar" instead of demucs "guitar").
    if (type != StemType::unknown && type != StemType::recording)
        name = stemTypeToString (type);
    else
        name = file.getFileNameWithoutExtension();

    muted = false;
    solo = false;
    volume = 0.8f;
    meterLevel.store (0.0f, std::memory_order_relaxed);
    meterPeakHold.store (0.0f, std::memory_order_relaxed);
    peakHoldTimer.store (0.0f, std::memory_order_relaxed);
    return true;
}

void StemTrack::updateMeterFromBuffer (const juce::AudioBuffer<float>& buffer, const float gain) const noexcept
{
    float peak = 0.0f;
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);

        for (int i = 0; i < samples; ++i)
            peak = juce::jmax (peak, std::abs (data[i] * gain));
    }

    // Soft clip visual range so loud material still hits red without exploding the scale.
    peak = juce::jlimit (0.0f, 1.0f, peak);

    // Envelope: instant rise, slower fall (classic VU feel).
    const auto previous = meterLevel.load (std::memory_order_relaxed);
    const auto envelope = peak >= previous ? peak : previous * 0.86f;
    meterLevel.store (envelope, std::memory_order_relaxed);

    auto hold = meterPeakHold.load (std::memory_order_relaxed);

    if (peak >= hold)
    {
        meterPeakHold.store (peak, std::memory_order_relaxed);
        // Hold the peak light for ~1.6s before slow decay starts.
        peakHoldTimer.store (1.6f, std::memory_order_relaxed);
    }
}

void StemTrack::tickMeterPeakHold (const float deltaSeconds) noexcept
{
    auto timer = peakHoldTimer.load (std::memory_order_relaxed);

    if (timer > 0.0f)
    {
        timer = juce::jmax (0.0f, timer - deltaSeconds);
        peakHoldTimer.store (timer, std::memory_order_relaxed);
        return;
    }

    // Slow fall after hold — “old school” sticky peak.
    auto hold = meterPeakHold.load (std::memory_order_relaxed);

    if (hold > 0.0005f)
    {
        hold *= std::pow (0.15f, deltaSeconds); // ~smooth decay
        meterPeakHold.store (hold, std::memory_order_relaxed);
    }
    else
    {
        meterPeakHold.store (0.0f, std::memory_order_relaxed);
    }

    // Also ease live level toward silence when not playing.
    auto level = meterLevel.load (std::memory_order_relaxed);

    if (level > 0.0005f)
        meterLevel.store (level * std::pow (0.08f, deltaSeconds), std::memory_order_relaxed);
    else
        meterLevel.store (0.0f, std::memory_order_relaxed);
}

void StemTrack::readIntoBuffer (juce::AudioBuffer<float>& output,
                                const int64 startSample,
                                const int numSamples,
                                const bool anySoloActive) const
{
    if (reader == nullptr || numSamples <= 0)
        return;

    const bool isAudible = anySoloActive ? solo : ! muted;

    if (! isAudible || volume <= 0.0f)
    {
        // Fall toward silence on mute.
        const auto previous = meterLevel.load (std::memory_order_relaxed);
        meterLevel.store (previous * 0.85f, std::memory_order_relaxed);
        return;
    }

    juce::AudioBuffer<float> tempBuffer (static_cast<int> (reader->numChannels), numSamples);
    tempBuffer.clear();

    if (! reader->read (&tempBuffer, 0, numSamples, startSample, true, true))
        return;

    updateMeterFromBuffer (tempBuffer, volume);

    const auto outputChannels = output.getNumChannels();
    const auto sourceChannels = tempBuffer.getNumChannels();

    for (int channel = 0; channel < outputChannels; ++channel)
    {
        const auto sourceChannel = juce::jmin (channel, sourceChannels - 1);
        output.addFrom (channel, 0, tempBuffer, sourceChannel, 0, numSamples, volume);
    }
}

} // namespace jamstudio::audio
