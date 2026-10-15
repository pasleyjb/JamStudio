#include "LiveInputs.h"

namespace jamstudio::audio
{

LiveInputs::LiveInputs()
{
    for (auto& channel : channels)
    {
        channel.send[static_cast<size_t> (MixBus::foh)].store (0.0f, std::memory_order_relaxed);
        // Mon A is the DisplayLink dock, so the live mic has to ride the
        // software send. There is no analog mix on that output.
        channel.send[static_cast<size_t> (MixBus::monitorA)].store (0.8f, std::memory_order_relaxed);
        channel.send[static_cast<size_t> (MixBus::monitorB)].store (0.0f, std::memory_order_relaxed);
    }
}

void LiveInputs::setMuted (const int channel, const bool muted) noexcept
{
    if (juce::isPositiveAndBelow (channel, kNumLiveInputs))
        channels[static_cast<size_t> (channel)].muted.store (muted, std::memory_order_relaxed);
}

bool LiveInputs::isMuted (const int channel) const noexcept
{
    if (! juce::isPositiveAndBelow (channel, kNumLiveInputs))
        return false;

    return channels[static_cast<size_t> (channel)].muted.load (std::memory_order_relaxed);
}

void LiveInputs::setSend (const int channel, const MixBus bus, const float gain) noexcept
{
    const auto b = static_cast<int> (bus);

    if (! juce::isPositiveAndBelow (channel, kNumLiveInputs) || ! juce::isPositiveAndBelow (b, kNumMixBuses))
        return;

    channels[static_cast<size_t> (channel)].send[static_cast<size_t> (b)].store (
        juce::jlimit (0.0f, 1.0f, gain), std::memory_order_relaxed);
}

float LiveInputs::getSend (const int channel, const MixBus bus) const noexcept
{
    const auto b = static_cast<int> (bus);

    if (! juce::isPositiveAndBelow (channel, kNumLiveInputs) || ! juce::isPositiveAndBelow (b, kNumMixBuses))
        return 0.0f;

    return channels[static_cast<size_t> (channel)].send[static_cast<size_t> (b)].load (std::memory_order_relaxed);
}

float LiveInputs::getMeter (const int channel) const noexcept
{
    if (! juce::isPositiveAndBelow (channel, kNumLiveInputs))
        return 0.0f;

    return channels[static_cast<size_t> (channel)].meter.load (std::memory_order_relaxed);
}

void LiveInputs::mix (const juce::AudioBuffer<float>& inputs,
                      juce::AudioBuffer<float>& dest,
                      const int destStart,
                      const int numSamples,
                      const std::array<float, kNumMixBuses>& busMaster) noexcept
{
    if (numSamples <= 0 || inputs.getNumChannels() < kNumLiveInputs)
        return;

    const auto availableBuses = juce::jlimit (1, kNumMixBuses, dest.getNumChannels() / kChannelsPerBus);

    for (int channel = 0; channel < kNumLiveInputs; ++channel)
    {
        const auto* src = inputs.getReadPointer (channel);
        auto& state = channels[static_cast<size_t> (channel)];

        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto a = src[i] < 0.0f ? -src[i] : src[i];
            peak = juce::jmax (peak, a);
        }

        const auto prev = state.meter.load (std::memory_order_relaxed);
        state.meter.store (peak >= prev ? peak : prev * 0.82f, std::memory_order_relaxed);

        if (state.muted.load (std::memory_order_relaxed))
            continue;

        for (int b = 0; b < availableBuses; ++b)
        {
            const auto gain = state.send[static_cast<size_t> (b)].load (std::memory_order_relaxed)
                              * busMaster[static_cast<size_t> (b)];

            if (gain <= 0.0001f)
                continue;

            const int base = mixBusOutputOffset (static_cast<MixBus> (b));
            auto* left = base < dest.getNumChannels() ? dest.getWritePointer (base) + destStart : nullptr;
            auto* right = base + 1 < dest.getNumChannels() ? dest.getWritePointer (base + 1) + destStart : nullptr;

            if (left == nullptr && right == nullptr)
                continue;

            dest.setNotClear();

            for (int i = 0; i < numSamples; ++i)
            {
                const auto sample = src[i] * gain;
                if (left != nullptr)
                    left[i] += sample;
                if (right != nullptr)
                    right[i] += sample;
            }
        }
    }
}

} // namespace jamstudio::audio
