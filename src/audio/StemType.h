#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

enum class StemType
{
    vocals,
    drums,
    bass,
    other,
    recording,
    unknown
};

inline juce::String stemTypeToString (StemType type)
{
    switch (type)
    {
        case StemType::vocals:  return "Vocals";
        case StemType::drums:   return "Drums";
        case StemType::bass:    return "Bass";
        case StemType::other:     return "Other";
        case StemType::recording: return "Recording";
        case StemType::unknown:   return "Unknown";
    }

    return "Unknown";
}

inline StemType stemTypeFromFileName (const juce::String& fileName)
{
    const auto lower = fileName.toLowerCase();

    if (lower.contains ("vocal"))
        return StemType::vocals;

    if (lower.contains ("drum"))
        return StemType::drums;

    if (lower.contains ("bass"))
        return StemType::bass;

    if (lower.contains ("other") || lower.contains ("guitar"))
        return StemType::other;

    if (lower.contains ("recording") || lower.startsWith ("take"))
        return StemType::recording;

    return StemType::unknown;
}

} // namespace jamstudio::audio