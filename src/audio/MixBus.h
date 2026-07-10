#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/** Live-mix destinations for multi-output interfaces. */
enum class MixBus : int
{
    foh = 0,      // Front of house / PA
    monitorA = 1, // Band monitor / IEM mix A
    monitorB = 2, // Band monitor / IEM mix B
    count = 3
};

inline constexpr int kNumMixBuses = static_cast<int> (MixBus::count);
inline constexpr int kChannelsPerBus = 2; // stereo per bus
inline constexpr int kMaxMixChannels = kNumMixBuses * kChannelsPerBus; // 6

[[nodiscard]] inline juce::String mixBusName (const MixBus bus)
{
    switch (bus)
    {
        case MixBus::foh: return "FOH";
        case MixBus::monitorA: return "Mon A";
        case MixBus::monitorB: return "Mon B";
        case MixBus::count: break;
    }
    return "Bus";
}

[[nodiscard]] inline juce::String mixBusLongName (const MixBus bus)
{
    switch (bus)
    {
        case MixBus::foh: return "Front of House (PA)";
        case MixBus::monitorA: return "Monitor / IEM A";
        case MixBus::monitorB: return "Monitor / IEM B";
        case MixBus::count: break;
    }
    return "Bus";
}

[[nodiscard]] inline juce::Colour mixBusColour (const MixBus bus)
{
    switch (bus)
    {
        case MixBus::foh: return juce::Colour (0xff3d9eff);      // blue
        case MixBus::monitorA: return juce::Colour (0xff33cc66); // green
        case MixBus::monitorB: return juce::Colour (0xffffaa22); // amber
        case MixBus::count: break;
    }
    return juce::Colours::grey;
}

/** First hardware output channel index for a bus (L = base, R = base+1). */
[[nodiscard]] inline int mixBusOutputOffset (const MixBus bus) noexcept
{
    return static_cast<int> (bus) * kChannelsPerBus;
}

} // namespace jamstudio::audio
