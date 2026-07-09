#include "OnlineLyricsClient.h"

#include "LrcParser.h"

#include <cmath>

namespace jamstudio::notation
{

namespace
{
constexpr auto kUserAgent = "JamStudio/0.9.6 (https://github.com/pasleyjb/JamStudio)";
constexpr auto kSearchUrl = "https://lrclib.net/api/search";
constexpr auto kGetUrl = "https://lrclib.net/api/get/";

juce::String urlEncode (const juce::String& value)
{
    return juce::URL::addEscapeChars (value, true);
}
} // namespace

juce::String OnlineLyricsClient::httpGet (const juce::String& url, juce::String& error)
{
    juce::URL request (url);

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (15000)
                             .withHttpRequestCmd ("GET")
                             .withExtraHeaders ("User-Agent: " + juce::String (kUserAgent) + "\r\n"
                                                "Accept: application/json\r\n");

    if (auto stream = request.createInputStream (options))
    {
        const auto body = stream->readEntireStreamAsString();

        if (body.isEmpty())
        {
            error = "Empty response from lyric service.";
            return {};
        }

        return body;
    }

    error = "Could not reach lyric service (network or TLS). Check your connection.";
    return {};
}

OnlineLyricsCandidate OnlineLyricsClient::parseCandidateObject (const juce::var& object)
{
    OnlineLyricsCandidate c;

    if (auto* props = object.getDynamicObject())
    {
        c.id = static_cast<int> (props->getProperty ("id"));
        c.trackName = props->getProperty ("trackName").toString().trim();

        if (c.trackName.isEmpty())
            c.trackName = props->getProperty ("name").toString().trim();

        c.artistName = props->getProperty ("artistName").toString().trim();
        c.albumName = props->getProperty ("albumName").toString().trim();
        c.durationSeconds = static_cast<double> (props->getProperty ("duration"));
        c.instrumental = static_cast<bool> (props->getProperty ("instrumental"));
        c.syncedLyrics = props->getProperty ("syncedLyrics").toString();
        c.plainLyrics = props->getProperty ("plainLyrics").toString();
        c.hasSyncedLyrics = c.syncedLyrics.trim().isNotEmpty();
    }

    return c;
}

juce::Array<OnlineLyricsCandidate> OnlineLyricsClient::parseSearchJson (const juce::String& jsonText)
{
    juce::Array<OnlineLyricsCandidate> results;
    const auto parsed = juce::JSON::parse (jsonText);

    if (auto* arr = parsed.getArray())
    {
        for (const auto& item : *arr)
        {
            auto c = parseCandidateObject (item);

            if (c.trackName.isNotEmpty() || c.id > 0)
                results.add (std::move (c));
        }
    }
    else if (parsed.getDynamicObject() != nullptr)
    {
        // Single object (get-by-id style)
        auto c = parseCandidateObject (parsed);

        if (c.id > 0)
            results.add (std::move (c));
    }

    return results;
}

void OnlineLyricsClient::searchAsync (const SongMetadata& metadata, SearchCallback onComplete)
{
    juce::Thread::launch ([metadata, onComplete = std::move (onComplete)]
    {
        juce::Array<OnlineLyricsCandidate> results;
        juce::String error;

        auto runSearch = [&] (const juce::String& url)
        {
            juce::String requestError;
            const auto body = httpGet (url, requestError);

            if (body.isEmpty())
            {
                if (error.isEmpty())
                    error = requestError;

                return;
            }

            auto parsed = parseSearchJson (body);

            for (auto& c : parsed)
            {
                bool exists = false;

                for (const auto& existing : results)
                    if (existing.id == c.id && c.id != 0)
                        exists = true;

                if (! exists)
                    results.add (std::move (c));
            }
        };

        // 1) Artist + title (best)
        if (metadata.title.isNotEmpty() && metadata.artist.isNotEmpty())
        {
            juce::String url = juce::String (kSearchUrl)
                               + "?track_name=" + urlEncode (metadata.title)
                               + "&artist_name=" + urlEncode (metadata.artist);

            if (metadata.album.isNotEmpty())
                url += "&album_name=" + urlEncode (metadata.album);

            runSearch (url);
        }

        // 2) Title + album when artist tag missing (common for ripped CDs)
        if (results.isEmpty() && metadata.title.isNotEmpty() && metadata.album.isNotEmpty())
        {
            runSearch (juce::String (kSearchUrl)
                       + "?track_name=" + urlEncode (metadata.title)
                       + "&album_name=" + urlEncode (metadata.album));
        }

        // 3) Free-text: always "Artist Title" when possible — never title alone if artist exists
        if (results.isEmpty())
        {
            const auto q = metadata.searchQuery();

            if (q.isNotEmpty())
                runSearch (juce::String (kSearchUrl) + "?q=" + urlEncode (q));
        }

        // Sort by artist+title identity, then synced, then duration
        struct Sorter
        {
            SongMetadata meta;

            int compareElements (const OnlineLyricsCandidate& a, const OnlineLyricsCandidate& b) const
            {
                const auto sa = meta.scoreCandidate (a.trackName, a.artistName, a.albumName, a.durationSeconds)
                                + (a.hasSyncedLyrics ? 20.0 : 0.0)
                                + (a.instrumental ? -40.0 : 0.0);
                const auto sb = meta.scoreCandidate (b.trackName, b.artistName, b.albumName, b.durationSeconds)
                                + (b.hasSyncedLyrics ? 20.0 : 0.0)
                                + (b.instrumental ? -40.0 : 0.0);

                if (sa > sb + 0.5) return -1;
                if (sb > sa + 0.5) return 1;
                return a.displayLine().compareIgnoreCase (b.displayLine());
            }
        };

        Sorter sorter { metadata };
        results.sort (sorter);

        // Drop obvious mismatches when we know the artist (keep list useful for manual UI)
        if (metadata.artist.isNotEmpty())
        {
            juce::Array<OnlineLyricsCandidate> filtered;

            for (const auto& c : results)
            {
                const auto s = metadata.scoreCandidate (c.trackName, c.artistName, c.albumName, c.durationSeconds);

                if (s >= -20.0) // reject hard artist mismatches
                    filtered.add (c);
            }

            if (filtered.size() > 0)
                results = std::move (filtered);
        }

        // Cap list for UI
        while (results.size() > 40)
            results.removeLast();

        if (results.isEmpty() && error.isEmpty())
            error = "No online lyrics found for \"" + metadata.displayLabel() + "\".";

        juce::MessageManager::callAsync ([onComplete, results, error]
        {
            onComplete (results, error);
        });
    });
}

void OnlineLyricsClient::fetchByIdAsync (const int lyricsId, DownloadCallback onComplete)
{
    juce::Thread::launch ([lyricsId, onComplete = std::move (onComplete)]
    {
        LyricsTrack lyrics;
        juce::String error;

        if (lyricsId <= 0)
        {
            error = "Invalid lyrics id.";
            juce::MessageManager::callAsync ([onComplete, lyrics, error] { onComplete (lyrics, error); });
            return;
        }

        const auto url = juce::String (kGetUrl) + juce::String (lyricsId);
        juce::String requestError;
        const auto body = httpGet (url, requestError);

        if (body.isEmpty())
        {
            error = requestError.isNotEmpty() ? requestError : "Empty response from lyric service.";
            juce::MessageManager::callAsync ([onComplete, lyrics, error] { onComplete (lyrics, error); });
            return;
        }

        const auto candidate = parseCandidateObject (juce::JSON::parse (body));

        if (! candidateToLyrics (candidate, lyrics, error))
        {
            juce::MessageManager::callAsync ([onComplete, lyrics, error] { onComplete (lyrics, error); });
            return;
        }

        juce::MessageManager::callAsync ([onComplete, lyrics, error]
        {
            onComplete (lyrics, error);
        });
    });
}

bool OnlineLyricsClient::candidateToLyrics (const OnlineLyricsCandidate& candidate,
                                            LyricsTrack& out,
                                            juce::String& error)
{
    out.clear();

    if (candidate.syncedLyrics.trim().isNotEmpty())
    {
        if (! LrcParser::parseText (candidate.syncedLyrics, out, error))
            return false;
    }
    else if (candidate.plainLyrics.trim().isNotEmpty())
    {
        // Untimed plain lyrics — still useful; put as sequential lines without timestamps.
        out.setTitle (candidate.trackName);
        const auto lines = juce::StringArray::fromLines (candidate.plainLyrics);
        double t = 0.0;

        for (const auto& line : lines)
        {
            const auto text = line.trim();

            if (text.isEmpty())
            {
                t += 1.0;
                continue;
            }

            LyricLine lyric;
            lyric.startSeconds = t;
            lyric.endSeconds = t + 3.0;
            lyric.text = text;
            out.addLine (std::move (lyric));
            t += 3.0;
        }

        if (out.isEmpty())
        {
            error = "Plain lyrics were empty.";
            return false;
        }
    }
    else if (candidate.instrumental)
    {
        error = "Selected match is instrumental (no lyrics).";
        return false;
    }
    else
    {
        error = "Selected match has no lyric text.";
        return false;
    }

    if (out.getTitle().isEmpty())
        out.setTitle (candidate.artistName.isNotEmpty()
                          ? candidate.artistName + " — " + candidate.trackName
                          : candidate.trackName);

    out.finalizeTiming();
    return true;
}

} // namespace jamstudio::notation
