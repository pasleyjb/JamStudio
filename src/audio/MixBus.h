#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/**
 * Live-mix destinations.
 * FOH = house PA; Mon 1–5 = independent band / IEM mixes.
 * Hardware map (when the device has enough outs):
 *   1–2 FOH, 3–4 Mon1, 5–6 Mon2, 7–8 Mon3, 9–10 Mon4, 11–12 Mon5
 * Legacy aliases: monitorA = mon1, monitorB = mon2.
 */
enum class MixBus : int
{
    foh = 0,
    mon1 = 1,
    mon2 = 2,
    mon3 = 3,
    mon4 = 4,
    mon5 = 5,
    count = 6,

    monitorA = mon1,
    monitorB = mon2
};

inline constexpr int kNumMixBuses = static_cast<int> (MixBus::count);
inline constexpr int kNumBandMonitors = kNumMixBuses - 1; // mon1..mon5
inline constexpr int kChannelsPerBus = 2; // stereo per bus
inline constexpr int kMaxMixChannels = kNumMixBuses * kChannelsPerBus; // 12

[[nodiscard]] inline bool isBandMonitorBus (const MixBus bus) noexcept
{
    const auto i = static_cast<int> (bus);
    return i >= static_cast<int> (MixBus::mon1) && i <= static_cast<int> (MixBus::mon5);
}

[[nodiscard]] inline juce::String mixBusName (const MixBus bus)
{
    switch (bus)
    {
        case MixBus::foh: return "FOH";
        case MixBus::mon1: return "M1";
        case MixBus::mon2: return "M2";
        case MixBus::mon3: return "M3";
        case MixBus::mon4: return "M4";
        case MixBus::mon5: return "M5";
        case MixBus::count: break;
    }
    return "Bus";
}

[[nodiscard]] inline juce::String mixBusLongName (const MixBus bus)
{
    switch (bus)
    {
        case MixBus::foh: return "Front of House (PA)";
        case MixBus::mon1: return "Monitor / IEM 1";
        case MixBus::mon2: return "Monitor / IEM 2";
        case MixBus::mon3: return "Monitor / IEM 3";
        case MixBus::mon4: return "Monitor / IEM 4";
        case MixBus::mon5: return "Monitor / IEM 5";
        case MixBus::count: break;
    }
    return "Bus";
}

[[nodiscard]] inline juce::Colour mixBusColour (const MixBus bus)
{
    switch (bus)
    {
        case MixBus::foh: return juce::Colour (0xff3d9eff);  // blue
        case MixBus::mon1: return juce::Colour (0xff33cc66); // green
        case MixBus::mon2: return juce::Colour (0xffffaa22); // amber
        case MixBus::mon3: return juce::Colour (0xffe056fd); // purple
        case MixBus::mon4: return juce::Colour (0xff00cec9); // teal
        case MixBus::mon5: return juce::Colour (0xffff6b6b); // coral
        case MixBus::count: break;
    }
    return juce::Colours::grey;
}

/** First hardware output channel index for a bus (L = base, R = base+1). */
[[nodiscard]] inline int mixBusOutputOffset (const MixBus bus) noexcept
{
    return static_cast<int> (bus) * kChannelsPerBus;
}

/** Hardware out pair label, e.g. "1-2", "3-4". */
[[nodiscard]] inline juce::String mixBusHardwareOuts (const MixBus bus)
{
    const int base = mixBusOutputOffset (bus) + 1; // 1-based for display
    return juce::String (base) + "-" + juce::String (base + 1);
}

/**
 * Which stereo bus is folded to the PC / virtual-interface speakers.
 * Values 0..5 match MixBus; sumAll mixes every bus to stereo.
 */
enum class OutputMonitorSelect : int
{
    foh = 0,
    mon1 = 1,
    mon2 = 2,
    mon3 = 3,
    mon4 = 4,
    mon5 = 5,
    sumAll = 6,

    monA = mon1,
    monB = mon2
};

[[nodiscard]] inline juce::String outputMonitorSelectName (const OutputMonitorSelect s)
{
    if (s == OutputMonitorSelect::sumAll)
        return "Sum all buses";
    if (s == OutputMonitorSelect::foh)
        return "FOH (" + mixBusHardwareOuts (MixBus::foh) + ")";
    const auto bus = static_cast<MixBus> (static_cast<int> (s));
    return mixBusName (bus) + " (" + mixBusHardwareOuts (bus) + ")";
}

[[nodiscard]] inline juce::String outputMonitorSelectToString (const OutputMonitorSelect s)
{
    switch (s)
    {
        case OutputMonitorSelect::foh: return "foh";
        case OutputMonitorSelect::mon1: return "mon1";
        case OutputMonitorSelect::mon2: return "mon2";
        case OutputMonitorSelect::mon3: return "mon3";
        case OutputMonitorSelect::mon4: return "mon4";
        case OutputMonitorSelect::mon5: return "mon5";
        case OutputMonitorSelect::sumAll: return "sumAll";
    }
    return "foh";
}

[[nodiscard]] inline OutputMonitorSelect outputMonitorSelectFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("mon1") || s.equalsIgnoreCase ("monA") || s.equalsIgnoreCase ("monitorA"))
        return OutputMonitorSelect::mon1;
    if (s.equalsIgnoreCase ("mon2") || s.equalsIgnoreCase ("monB") || s.equalsIgnoreCase ("monitorB"))
        return OutputMonitorSelect::mon2;
    if (s.equalsIgnoreCase ("mon3")) return OutputMonitorSelect::mon3;
    if (s.equalsIgnoreCase ("mon4")) return OutputMonitorSelect::mon4;
    if (s.equalsIgnoreCase ("mon5")) return OutputMonitorSelect::mon5;
    if (s.equalsIgnoreCase ("sumAll") || s.equalsIgnoreCase ("sum")) return OutputMonitorSelect::sumAll;
    return OutputMonitorSelect::foh;
}

/** Default send when a stem is first loaded: FOH + Mon1 up, others down. */
[[nodiscard]] inline std::array<float, kNumMixBuses> defaultStemBusSends()
{
    return { 0.8f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f };
}

/** Default click routing: quiet on FOH, strong on Mon1–2, lighter on 3–5. */
[[nodiscard]] inline std::array<float, kNumMixBuses> defaultClickBusSends()
{
    return { 0.0f, 0.85f, 0.70f, 0.55f, 0.45f, 0.40f };
}

/** Stage media mainly FOH, light bleed to monitors. */
[[nodiscard]] inline std::array<float, kNumMixBuses> defaultStageBusSends()
{
    return { 1.0f, 0.30f, 0.25f, 0.20f, 0.15f, 0.10f };
}

} // namespace jamstudio::audio
