#pragma once

#include "MixBus.h"

#include <array>
#include <atomic>

namespace jamstudio::audio
{

/** Fast Track input 1 (mic) and input 2 (instrument). */
inline constexpr int kNumLiveInputs = 2;

/**
 * Live cue sends for the mic and the instrument.
 * FOH stays off until someone raises it. Mon A starts up so the player
 * hears themselves on the monitor while the house mix stays the backing track.
 */
class LiveInputs
{
public:
    LiveInputs();

    void setMuted (int channel, bool muted) noexcept;
    [[nodiscard]] bool isMuted (int channel) const noexcept;

    void setSend (int channel, MixBus bus, float gain) noexcept;
    [[nodiscard]] float getSend (int channel, MixBus bus) const noexcept;

    /** Pre-fader peak of the input, 0..1. */
    [[nodiscard]] float getMeter (int channel) const noexcept;

    /** Audio thread. `inputs` is mono mic on channel 0 and mono instrument on channel 1. */
    void mix (const juce::AudioBuffer<float>& inputs,
              juce::AudioBuffer<float>& dest,
              int destStart,
              int numSamples,
              const std::array<float, kNumMixBuses>& busMaster) noexcept;

private:
    struct Channel
    {
        std::array<std::atomic<float>, kNumMixBuses> send;
        std::atomic<bool> muted { false };
        std::atomic<float> meter { 0.0f };
    };

    std::array<Channel, kNumLiveInputs> channels;
};

} // namespace jamstudio::audio
