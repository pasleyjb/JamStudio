#pragma once

namespace jamstudio::audio
{

/** Audio-thread handoff of one rendered 6-channel mix block. */
class MixOutputTap
{
public:
    virtual ~MixOutputTap() = default;

    virtual void sendMix (const float* const* mixChannels,
                          int numMixChannels,
                          int numSamples,
                          float* const* deviceOut,
                          int numDeviceOut) noexcept = 0;
};

} // namespace jamstudio::audio
