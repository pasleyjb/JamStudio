#include "LrcParser.h"

#include <cmath>

namespace jamstudio::notation
{

namespace
{
juce::StringArray extractTimestamps (const juce::String& line)
{
    juce::StringArray timestamps;
    auto remaining = line;

    while (remaining.containsChar ('['))
    {
        const auto open = remaining.indexOfChar ('[');
        const auto close = remaining.indexOfChar (']');

        if (close < 0 || close <= open)
            break;

        timestamps.add (remaining.substring (open + 1, close));
        remaining = remaining.substring (close + 1);
    }

    return timestamps;
}

juce::String extractLyricText (const juce::String& line)
{
    auto text = line;

    while (text.containsChar (']'))
    {
        const auto close = text.indexOfChar (']');
        text = text.substring (close + 1);
    }

    return text.trim();
}
} // namespace

bool LrcParser::parseTimestamp (const juce::String& timestamp, double& seconds)
{
    // Supports [mm:ss.xx], [mm:ss:xx], and [hh:mm:ss.xx]
    const auto parts = juce::StringArray::fromTokens (timestamp, ":", "");

    if (parts.size() < 2)
        return false;

    double minutes = 0.0;
    juce::String secToken;

    if (parts.size() >= 3 && parts[0].containsOnly ("0123456789")
        && parts[1].containsOnly ("0123456789")
        && ! parts[2].containsOnly ("0123456789"))
    {
        // hh:mm:ss.xx
        minutes = parts[0].getDoubleValue() * 60.0 + parts[1].getDoubleValue();
        secToken = parts[2];
    }
    else if (parts.size() >= 3 && parts[2].containsOnly ("0123456789"))
    {
        // mm:ss:xx (centiseconds with colon separator — common in some LRC variants)
        minutes = parts[0].getDoubleValue();
        const auto secs = parts[1].getDoubleValue();
        const auto frac = parts[2].getDoubleValue()
                          / std::pow (10.0, juce::jmax (1, parts[2].length()));
        seconds = minutes * 60.0 + secs + frac;
        return true;
    }
    else
    {
        minutes = parts[0].getDoubleValue();
        secToken = parts[1];
    }

    auto secParts = juce::StringArray::fromTokens (secToken, ".", "");
    const auto secs = secParts[0].getDoubleValue();
    const auto fraction = secParts.size() > 1
        ? secParts[1].getDoubleValue() / std::pow (10.0, juce::jmax (1, secParts[1].length()))
        : 0.0;

    seconds = minutes * 60.0 + secs + fraction;
    return true;
}

bool LrcParser::parseMetadata (const juce::String& tag, const juce::String& value, LyricsTrack& track)
{
    if (tag.equalsIgnoreCase ("ti") || tag.equalsIgnoreCase ("title"))
    {
        track.setTitle (value);
        return true;
    }

    return false;
}

bool LrcParser::parseFile (const juce::File& file, LyricsTrack& track, juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "Lyrics file does not exist.";
        return false;
    }

    return parseText (file.loadFileAsString(), track, errorMessage);
}

bool LrcParser::parseText (const juce::String& lrcText, LyricsTrack& track, juce::String& errorMessage)
{
    track.clear();

    auto parsedAny = false;
    const auto lines = juce::StringArray::fromLines (lrcText);

    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();

        if (trimmed.isEmpty())
            continue;

        if (trimmed.startsWithChar ('['))
        {
            const auto close = trimmed.indexOfChar (']');

            if (close > 1)
            {
                const auto inside = trimmed.substring (1, close);

                if (inside.containsChar (':') && ! juce::CharacterFunctions::isDigit (inside[0]))
                {
                    const auto colon = inside.indexOfChar (':');
                    parseMetadata (inside.substring (0, colon), inside.substring (colon + 1), track);
                    continue;
                }
            }
        }

        const auto timestamps = extractTimestamps (trimmed);
        const auto lyricText = extractLyricText (trimmed);

        if (timestamps.isEmpty() || lyricText.isEmpty())
            continue;

        for (const auto& timestamp : timestamps)
        {
            double seconds = 0.0;

            if (parseTimestamp (timestamp, seconds))
            {
                track.addLine (seconds, lyricText);
                parsedAny = true;
            }
        }
    }

    if (! parsedAny)
    {
        errorMessage = "No timed lyric lines were found in the LRC file.";
        return false;
    }

    track.finalizeTiming();
    return true;
}

} // namespace jamstudio::notation