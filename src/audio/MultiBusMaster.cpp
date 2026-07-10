#include "MultiBusMaster.h"

namespace jamstudio::audio
{

MultiBusMaster::MultiBusMaster (StemMixer& stems, Metronome& metro, StageMediaPlayer& stage)
    : stemMixer (stems),
      metronome (metro),
      stageMedia (stage)
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
}

void MultiBusMaster::releaseResources()
{
    stemMixer.releaseResources();
    metronome.releaseResources();
    stageMedia.releaseResources();
    auxScratch.setSize (0, 0);
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
}

} // namespace jamstudio::audio
