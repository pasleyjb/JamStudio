#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

/** Best-effort song identity used for online lyric lookup. */
struct SongMetadata
{
    juce::String title;
    juce::String artist;
    juce::String album;
    double durationSeconds = 0.0;
    juce::File sourceFile;

    [[nodiscard]] bool hasSearchableFields() const noexcept
    {
        return title.isNotEmpty() || artist.isNotEmpty() || sourceFile.existsAsFile();
    }

    [[nodiscard]] juce::String displayLabel() const
    {
        if (artist.isNotEmpty() && title.isNotEmpty())
            return artist + " — " + title;

        if (title.isNotEmpty())
            return title;

        if (sourceFile.existsAsFile())
            return sourceFile.getFileNameWithoutExtension();

        return "Unknown song";
    }

    [[nodiscard]] juce::String searchQuery() const
    {
        juce::StringArray parts;

        if (artist.isNotEmpty())
            parts.add (artist);

        if (title.isNotEmpty())
            parts.add (title);

        if (parts.isEmpty() && sourceFile.existsAsFile())
            parts.add (sourceFile.getFileNameWithoutExtension());

        return parts.joinIntoString (" ").trim();
    }
};

/** Reads embedded tags when present; falls back to smart filename parsing. */
[[nodiscard]] SongMetadata extractSongMetadata (const juce::File& audioFile,
                                                juce::AudioFormatManager& formatManager);

} // namespace jamstudio::notation
