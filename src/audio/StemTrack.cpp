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

    if (type != StemType::unknown && type != StemType::recording)
        name = stemTypeToString (type);
    else
        name = file.getFileNameWithoutExtension();

    muted = false;
    solo = false;
    recordArmed.store (false, std::memory_order_relaxed);
    startOffsetSeconds.store (0.0, std::memory_order_relaxed);
    busSends = { 0.8f, 0.7f, 0.0f };
    meterLevel.store (0.0f, std::memory_order_relaxed);
    meterPeakHold.store (0.0f, std::memory_order_relaxed);
    peakHoldTimer.store (0.0f, std::memory_order_relaxed);
    return true;
}

void StemTrack::setBusSend (const MixBus bus, const float gain) noexcept
{
    const auto i = static_cast<int> (bus);
    if (juce::isPositiveAndBelow (i, kNumMixBuses))
        busSends[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, gain);
}

void StemTrack::setStartOffsetSeconds (const double seconds) noexcept
{
    const auto safe = (std::isfinite (seconds) && seconds > 0.0) ? seconds : 0.0;
    startOffsetSeconds.store (safe, std::memory_order_relaxed);
}

float StemTrack::getBusSend (const MixBus bus) const noexcept
{
    const auto i = static_cast<int> (bus);
    if (juce::isPositiveAndBelow (i, kNumMixBuses))
        return busSends[static_cast<size_t> (i)];
    return 0.0f;
}

void StemTrack::updateMeterFromDry (const juce::AudioBuffer<float>& buffer, const float displayGain) const noexcept
{
    float peak = 0.0f;
    const auto channels = buffer.getNumChannels();
    const auto samples = buffer.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < samples; ++i)
            peak = juce::jmax (peak, std::abs (data[i] * displayGain));
    }

    peak = juce::jlimit (0.0f, 1.0f, peak);
    const auto previous = meterLevel.load (std::memory_order_relaxed);
    const auto envelope = peak >= previous ? peak : previous * 0.86f;
    meterLevel.store (envelope, std::memory_order_relaxed);

    auto hold = meterPeakHold.load (std::memory_order_relaxed);
    if (peak >= hold)
    {
        meterPeakHold.store (peak, std::memory_order_relaxed);
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

    auto hold = meterPeakHold.load (std::memory_order_relaxed);
    if (hold > 0.0005f)
        meterPeakHold.store (hold * std::pow (0.15f, deltaSeconds), std::memory_order_relaxed);
    else
        meterPeakHold.store (0.0f, std::memory_order_relaxed);

    auto level = meterLevel.load (std::memory_order_relaxed);
    if (level > 0.0005f)
        meterLevel.store (level * std::pow (0.08f, deltaSeconds), std::memory_order_relaxed);
    else
        meterLevel.store (0.0f, std::memory_order_relaxed);
}

double StemTrack::getFileSampleRate() const noexcept
{
    if (reader != nullptr && reader->sampleRate > 0.0)
        return reader->sampleRate;

    return 44100.0;
}

bool StemTrack::readDryResampled (juce::AudioBuffer<float>& temp,
                                  const double startSeconds,
                                  const int numOutputSamples,
                                  const double deviceSampleRate,
                                  const bool anySoloActive,
                                  const bool ignoreAudibility) const
{
    if (reader == nullptr || numOutputSamples <= 0 || deviceSampleRate <= 0.0)
        return false;

    if (! ignoreAudibility)
    {
        const bool isAudible = anySoloActive ? solo : ! muted;
        if (! isAudible)
            return false;

        bool anySend = false;
        for (float s : busSends)
            if (s > 0.0001f)
                anySend = true;

        if (! anySend)
            return false;
    }

    const auto offset = startOffsetSeconds.load (std::memory_order_relaxed);
    const double blockEnd = startSeconds + static_cast<double> (numOutputSamples) / deviceSampleRate;

    if (blockEnd <= offset)
        return false;

    int skipOut = 0;
    auto fileSeconds = startSeconds - offset;

    if (fileSeconds < 0.0)
    {
        skipOut = static_cast<int> (std::ceil ((-fileSeconds) * deviceSampleRate));
        skipOut = juce::jlimit (0, numOutputSamples, skipOut);
        fileSeconds = 0.0;
    }

    if (skipOut >= numOutputSamples)
        return false;

    const auto fileSR = getFileSampleRate();
    const double startSampleF = juce::jmax (0.0, fileSeconds) * fileSR;
    const auto played = numOutputSamples - skipOut;
    const double ratio = fileSR / deviceSampleRate;
    const int sourceSamples = juce::jmax (2, static_cast<int> (std::ceil (static_cast<double> (played) * ratio)) + 2);
    const auto startSample = static_cast<juce::int64> (startSampleF);

    juce::AudioBuffer<float> sourceBuffer (static_cast<int> (reader->numChannels), sourceSamples);
    sourceBuffer.clear();

    if (! reader->read (&sourceBuffer, 0, sourceSamples, startSample, true, true))
        return false;

    const int outCh = juce::jmin (2, temp.getNumChannels());
    temp.setNotClear();
    for (int ch = 0; ch < temp.getNumChannels(); ++ch)
        juce::FloatVectorOperations::clear (temp.getWritePointer (ch), temp.getNumSamples());

    const double phase0 = startSampleF - static_cast<double> (startSample);
    const int sourceChannels = sourceBuffer.getNumChannels();

    for (int ch = 0; ch < outCh; ++ch)
    {
        const auto srcCh = juce::jmin (ch, sourceChannels - 1);
        const auto* src = sourceBuffer.getReadPointer (srcCh);
        auto* dst = temp.getWritePointer (ch);

        for (int i = skipOut; i < numOutputSamples; ++i)
        {
            const double pos = phase0 + static_cast<double> (i - skipOut) * ratio;
            const auto i0 = juce::jlimit (0, sourceSamples - 1, static_cast<int> (pos));
            const auto i1 = juce::jmin (sourceSamples - 1, i0 + 1);
            const auto frac = static_cast<float> (pos - static_cast<double> (i0));
            dst[i] = src[i0] * (1.0f - frac) + src[i1] * frac;
        }
    }

    return true;
}

} // namespace jamstudio::audio
