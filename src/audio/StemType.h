#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

enum class StemType
{
    guitar,     // Primary practice stem (htdemucs_6s)
    bass,
    drums,
    vocals,
    piano,
    other,
    recording,
    unknown
};

inline juce::String stemTypeToString (StemType type)
{
    switch (type)
    {
        case StemType::guitar:    return "Guitar";
        case StemType::bass:      return "Bass";
        case StemType::drums:     return "Drums";
        case StemType::vocals:    return "Vocals";
        case StemType::piano:     return "Piano";
        case StemType::other:     return "Other";
        case StemType::recording: return "Recording";
        case StemType::unknown:   return "Unknown";
    }

    return "Unknown";
}

/** Mixer / practice UI order - guitar first for learning. */
inline int stemTypeSortOrder (StemType type)
{
    switch (type)
    {
        case StemType::guitar:    return 0;
        case StemType::bass:      return 1;
        case StemType::drums:     return 2;
        case StemType::vocals:    return 3;
        case StemType::piano:     return 4;
        case StemType::other:     return 5;
        case StemType::recording: return 6;
        case StemType::unknown:   return 7;
    }

    return 99;
}

/** Per-stem accent colour for mixer / lane chrome (DAW-style). */
inline juce::Colour stemTypeColour (StemType type)
{
    switch (type)
    {
        case StemType::guitar:    return juce::Colour (0xff3d9cf0); // blue
        case StemType::bass:      return juce::Colour (0xff9b59f5); // purple
        case StemType::drums:     return juce::Colour (0xfff0a030); // orange
        case StemType::vocals:    return juce::Colour (0xff3dd68c); // green
        case StemType::piano:     return juce::Colour (0xffe85d8a); // pink
        case StemType::other:     return juce::Colour (0xff8a9bb0); // slate
        case StemType::recording: return juce::Colour (0xffef4444); // red
        case StemType::unknown:   return juce::Colour (0xff6b7280); // grey
    }

    return juce::Colour (0xff6b7280);
}

inline StemType stemTypeFromFileName (const juce::String& fileName)
{
    const auto lower = fileName.toLowerCase();

    // Check specific instruments before the generic "other" bucket.
    if (lower.contains ("guitar"))
        return StemType::guitar;

    if (lower.contains ("piano") || lower.contains ("keys"))
        return StemType::piano;

    if (lower.contains ("vocal"))
        return StemType::vocals;

    if (lower.contains ("drum"))
        return StemType::drums;

    if (lower.contains ("bass"))
        return StemType::bass;

    if (lower.contains ("other"))
        return StemType::other;

    if (lower.contains ("recording") || lower.startsWith ("take"))
        return StemType::recording;

    return StemType::unknown;
}

} // namespace jamstudio::audio
