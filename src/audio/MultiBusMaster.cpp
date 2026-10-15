#include "MultiBusMaster.h"

namespace jamstudio::audio
{

MultiBusMaster::MultiBusMaster (StemMixer& stems, Metronome& metro, StageMediaPlayer& stage,
                                  TakeRecorder& takes, LiveInputs& live)
    : stemMixer (stems),
      metronome (metro),
      stageMedia (stage),
      takeRecorder (takes),
      liveInputs (live)
{
}

void MultiBusMaster::setClickBusSend (const MixBus bus, const float gain) noexcept
{
    const auto i = static_cast<int> (bus);
    if (juce::isPositiveAndBelow (i, kNumMixBuses))
        clickSend[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, gain);
}

float MultiBusMaster::getClickBusSend (const MixBus bus) const noexcept
{
    const auto i = static_cast<int> (bus);
    return juce::isPositiveAndBelow (i, kNumMixBuses) ? clickSend[static_cast<size_t> (i)] : 0.0f;
}

void MultiBusMaster::setStageBusSend (const MixBus bus, const float gain) noexcept
{
    const auto i = static_cast<int> (bus);
    if (juce::isPositiveAndBelow (i, kNumMixBuses))
        stageSend[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, gain);
}

float MultiBusMaster::getStageBusSend (const MixBus bus) const noexcept
{
    const auto i = static_cast<int> (bus);
    return juce::isPositiveAndBelow (i, kNumMixBuses) ? stageSend[static_cast<size_t> (i)] : 0.0f;
}

void MultiBusMaster::prepareToPlay (const int samplesPerBlockExpected, const double sampleRate)
{
    stemMixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
    metronome.prepareToPlay (samplesPerBlockExpected, sampleRate);
    stageMedia.prepareToPlay (samplesPerBlockExpected, sampleRate);
    auxScratch.setSize (2, juce::jmax (samplesPerBlockExpected, 512), false, true, true);
    inputScratch.setSize (kNumLiveInputs, juce::jmax (samplesPerBlockExpected, 512), false, true, true);
    outputCeiling = 1.0f;
}

void MultiBusMaster::releaseResources()
{
    stemMixer.releaseResources();
    metronome.releaseResources();
    stageMedia.releaseResources();
    auxScratch.setSize (0, 0);
    inputScratch.setSize (0, 0);
}

void MultiBusMaster::addSourceToBuses (const juce::AudioBuffer<float>& source,
                                       const int startSample,
                                       const int numSamples,
                                       juce::AudioBuffer<float>& dest,
                                       const int destStart,
                                       const std::array<float, kNumMixBuses>& sends)
{
    const auto outCh = dest.getNumChannels();
    const auto availableBuses = juce::jlimit (1, kNumMixBuses, outCh / kChannelsPerBus);
    const auto srcCh = source.getNumChannels();

    for (int b = 0; b < availableBuses; ++b)
    {
        const auto gain = sends[static_cast<size_t> (b)];
        if (gain <= 0.0001f)
            continue;

        const int base = mixBusOutputOffset (static_cast<MixBus> (b));
        if (base < outCh)
            dest.addFrom (base, destStart, source, 0, startSample, numSamples, gain);
        if (base + 1 < outCh)
            dest.addFrom (base + 1, destStart, source,
                          juce::jmin (1, srcCh - 1), startSample, numSamples, gain);
    }
}

void MultiBusMaster::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Stems write multi-bus directly into the output buffer.
    stemMixer.getNextAudioBlock (bufferToFill);

    // Mic and instrument stay in the cue even when the transport is stopped.
    if (bufferToFill.buffer != nullptr && bufferToFill.numSamples > 0)
    {
        if (inputScratch.getNumSamples() < bufferToFill.numSamples)
            inputScratch.setSize (kNumLiveInputs, bufferToFill.numSamples, false, false, true);

        inputScratch.setNotClear();
        for (int ch = 0; ch < inputScratch.getNumChannels(); ++ch)
            juce::FloatVectorOperations::clear (inputScratch.getWritePointer (ch), bufferToFill.numSamples);

        takeRecorder.copyLatchedInputs (inputScratch.getWritePointer (0),
                                        inputScratch.getWritePointer (1),
                                        bufferToFill.numSamples);

        std::array<float, kNumMixBuses> masters {};
        for (int b = 0; b < kNumMixBuses; ++b)
            masters[static_cast<size_t> (b)] = stemMixer.getBusMaster (static_cast<MixBus> (b));

        liveInputs.mix (inputScratch, *bufferToFill.buffer, bufferToFill.startSample,
                        bufferToFill.numSamples, masters);
    }

    if (auxScratch.getNumSamples() < bufferToFill.numSamples)
        auxScratch.setSize (2, bufferToFill.numSamples, false, false, true);

    // Click / metronome
    {
        juce::AudioSourceChannelInfo info (&auxScratch, 0, bufferToFill.numSamples);
        metronome.getNextAudioBlock (info);
        addSourceToBuses (auxScratch, 0, bufferToFill.numSamples,
                          *bufferToFill.buffer, bufferToFill.startSample, clickSend);
    }

    // Stage media / video sound
    {
        juce::AudioSourceChannelInfo info (&auxScratch, 0, bufferToFill.numSamples);
        stageMedia.getNextAudioBlock (info);
        addSourceToBuses (auxScratch, 0, bufferToFill.numSamples,
                          *bufferToFill.buffer, bufferToFill.startSample, stageSend);
    }

    // getMagnitude/applyGain return immediately when the buffer is marked clear,
    // which left a hot block untouched. Scan the samples and scale them.
    auto& out = *bufferToFill.buffer;
    const auto start = bufferToFill.startSample;
    const auto num = bufferToFill.numSamples;
    float peak = 0.0f;

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
    {
        const auto* samples = out.getReadPointer (ch) + start;
        for (int i = 0; i < num; ++i)
        {
            const auto a = samples[i] < 0.0f ? -samples[i] : samples[i];
            peak = juce::jmax (peak, a);
        }
    }

    renderedPeak.store (peak, std::memory_order_relaxed);
    renderedBlocks.fetch_add (1, std::memory_order_relaxed);

    const float target = peak > 0.98f ? 0.98f / peak : 1.0f;
    outputCeiling += (target - outputCeiling) * (target < outputCeiling ? 1.0f : 0.02f);

    if (outputCeiling < 0.999f)
    {
        out.setNotClear();
        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            juce::FloatVectorOperations::multiply (out.getWritePointer (ch) + start, outputCeiling, num);
    }
}

} // namespace jamstudio::audio
