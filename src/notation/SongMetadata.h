#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

/** Best-effort song identity used for online lyric / tab lookup. */
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

    [[nodiscard]] bool hasArtistAndTitle() const noexcept
    {
        return artist.isNotEmpty() && title.isNotEmpty();
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

    /** Prefer "Artist Title" for free-text APIs (never bare title alone if artist exists). */
    [[nodiscard]] juce::String searchQuery() const
    {
        juce::StringArray parts;

        if (artist.isNotEmpty())
            parts.add (artist);

        if (title.isNotEmpty())
            parts.add (title);
        else if (parts.isEmpty() && sourceFile.existsAsFile())
            parts.add (sourceFile.getFileNameWithoutExtension());

        return parts.joinIntoString (" ").trim();
    }

    /**
     * Score how well a candidate artist/title/album/duration matches this song.
     * Higher is better. Negative / low scores mean a bad match (wrong artist, etc.).
     */
    [[nodiscard]] double scoreCandidate (const juce::String& candidateTitle,
                                         const juce::String& candidateArtist,
                                         const juce::String& candidateAlbum = {},
                                         double candidateDurationSeconds = 0.0) const;
};

/** Lowercase, strip punctuation / feat. noise for fuzzy compares. */
[[nodiscard]] juce::String normalizeMatchText (juce::String value);

/** 0..1 similarity: token overlap + substring bonus. */
[[nodiscard]] double textMatchScore (const juce::String& a, const juce::String& b);

/** Reads embedded tags when present; falls back to path / filename parsing. */
[[nodiscard]] SongMetadata extractSongMetadata (const juce::File& audioFile,
                                                juce::AudioFormatManager& formatManager);

} // namespace jamstudio::notation
