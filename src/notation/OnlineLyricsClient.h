#pragma once

#include "LyricsTrack.h"
#include "SongMetadata.h"

namespace jamstudio::notation
{

struct OnlineLyricsCandidate
{
    int id = 0;
    juce::String trackName;
    juce::String artistName;
    juce::String albumName;
    double durationSeconds = 0.0;
    bool hasSyncedLyrics = false;
    bool instrumental = false;
    juce::String syncedLyrics; // LRC text when present
    juce::String plainLyrics;

    [[nodiscard]] juce::String displayLine() const
    {
        juce::String line = artistName;

        if (line.isNotEmpty() && trackName.isNotEmpty())
            line += " - ";

        line += trackName;

        if (albumName.isNotEmpty())
            line += "  (" + albumName + ")";

        if (hasSyncedLyrics)
            line += "  [synced]";
        else if (plainLyrics.isNotEmpty())
            line += "  [plain]";

        if (instrumental)
            line += "  [instrumental]";

        return line;
    }
};

/** Looks up timed LRC lyrics online via lrclib.net (no API key). */
class OnlineLyricsClient
{
public:
    using SearchCallback = std::function<void (juce::Array<OnlineLyricsCandidate> results, juce::String error)>;
    using DownloadCallback = std::function<void (LyricsTrack lyrics, juce::String error)>;

    void searchAsync (const SongMetadata& metadata, SearchCallback onComplete);
    void fetchByIdAsync (int lyricsId, DownloadCallback onComplete);

    /** Parses candidate.syncedLyrics / plainLyrics into a LyricsTrack. */
    [[nodiscard]] static bool candidateToLyrics (const OnlineLyricsCandidate& candidate,
                                                 LyricsTrack& out,
                                                 juce::String& error);

private:
    [[nodiscard]] static juce::String httpGet (const juce::String& url, juce::String& error);
    [[nodiscard]] static juce::Array<OnlineLyricsCandidate> parseSearchJson (const juce::String& jsonText);
    [[nodiscard]] static OnlineLyricsCandidate parseCandidateObject (const juce::var& object);
};

} // namespace jamstudio::notation
