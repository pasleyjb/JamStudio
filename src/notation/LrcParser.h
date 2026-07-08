#pragma once

#include "LyricsTrack.h"

namespace jamstudio::notation
{

/** Parses standard and extended LRC lyric files. */
class LrcParser
{
public:
    [[nodiscard]] static bool parseFile (const juce::File& file, LyricsTrack& track, juce::String& errorMessage);
    [[nodiscard]] static bool parseText (const juce::String& lrcText, LyricsTrack& track, juce::String& errorMessage);

private:
    [[nodiscard]] static bool parseTimestamp (const juce::String& timestamp, double& seconds);
    [[nodiscard]] static bool parseMetadata (const juce::String& tag, const juce::String& value, LyricsTrack& track);
};

} // namespace jamstudio::notation