#include "SongMetadata.h"

#include <cmath>

namespace jamstudio::notation
{

namespace
{
bool isGenericFolderName (const juce::String& name)
{
    const auto n = name.trim().toLowerCase();
    static const char* generic[] = {
        "music", "albums", "album", "songs", "tracks", "audio", "media",
        "downloads", "desktop", "documents", "library", "itunes", "flac",
        "mp3", "wav", "lossless", "cd", "disc", "vinyl", "home", "users",
        "var", "tmp", "temp"
    };

    for (const auto* g : generic)
        if (n == g)
            return true;

    return false;
}

juce::String cleanToken (juce::String value)
{
    value = value.trim();

    // Drop common track-number prefixes: "03 - Title", "03. Title", "14-Title"
    while (value.isNotEmpty() && juce::CharacterFunctions::isDigit (value[0]))
        value = value.substring (1).trim();

    while (value.isNotEmpty()
           && (value[0] == '-' || value[0] == '.' || value[0] == '_' || value[0] == ' '))
        value = value.substring (1).trim();

    // Strip trailing " (Live)", " [Remastered]" lightly for search - keep original for display
    return value.trim();
}

juce::String stripFeaturing (juce::String value)
{
    const auto lower = value.toLowerCase();

    for (const auto* marker : { " feat.", " ft.", " featuring ", " with " })
    {
        const auto idx = lower.indexOf (marker);

        if (idx > 0)
            return value.substring (0, idx).trim();
    }

    return value.trim();
}

void applyFilenameFallback (const juce::File& audioFile, SongMetadata& meta)
{
    auto base = audioFile.getFileNameWithoutExtension().trim();

    // Patterns: "Artist - Title", "Artist - Title"
    for (const auto* sep : { " - ", " - ", " - " })
    {
        if (base.contains (sep))
        {
            auto left = cleanToken (base.upToFirstOccurrenceOf (sep, false, false));
            auto right = cleanToken (base.fromFirstOccurrenceOf (sep, false, false));

            // "14 - Long Black Veil" -> left is empty after digit strip, right is title
            if (left.isEmpty() && right.isNotEmpty())
            {
                if (meta.title.isEmpty())
                    meta.title = right;
                break;
            }

            // Prefer "Artist - Title" when left doesn't look like only a track number remnant
            if (meta.artist.isEmpty() && left.isNotEmpty())
                meta.artist = stripFeaturing (left);

            if (meta.title.isEmpty() && right.isNotEmpty())
                meta.title = right;

            return;
        }
    }

    if (meta.title.isEmpty())
        meta.title = cleanToken (base);
}

void applyPathFallback (const juce::File& audioFile, SongMetadata& meta)
{
    const auto parent = audioFile.getParentDirectory();
    const auto parentName = parent.getFileName().trim();
    const auto grandparent = parent.getParentDirectory();
    const auto grandName = grandparent.getFileName().trim();

    if (parentName.isEmpty() || isGenericFolderName (parentName))
        return;

    // .../Albums/Album Name/track  ->  album = parent
    // .../Artist/Album/track       ->  artist = grandparent, album = parent
    // .../Artist/track             ->  artist = parent (if title-only file)

    if (grandName.isNotEmpty() && ! isGenericFolderName (grandName)
        && isGenericFolderName (grandparent.getParentDirectory().getFileName()) == false)
    {
        // If grandparent looks like a real artist folder and parent like album
        if (meta.album.isEmpty())
            meta.album = parentName;

        // Only use grandparent as artist when not a generic "Albums" container
        if (meta.artist.isEmpty() && ! isGenericFolderName (grandName)
            && grandName.toLowerCase() != "albums")
            meta.artist = stripFeaturing (grandName);
    }
    else if (isGenericFolderName (grandName) || grandName.toLowerCase() == "albums")
    {
        // MUSIC/Albums/Cheating at Solitaire/track.flac
        if (meta.album.isEmpty())
            meta.album = parentName;
    }
    else
    {
        // Artist/Song.flac layout
        if (meta.artist.isEmpty() && meta.title.isNotEmpty()
            && ! parentName.equalsIgnoreCase (meta.title))
            meta.artist = stripFeaturing (parentName);

        if (meta.album.isEmpty() && meta.artist.isEmpty())
            meta.album = parentName;
    }
}

juce::String metaValue (const juce::StringPairArray& values, std::initializer_list<const char*> keys)
{
    for (const auto* key : keys)
    {
        const auto v = values.getValue (key, {}).trim();

        if (v.isNotEmpty())
            return v;
    }

    for (int i = 0; i < values.size(); ++i)
    {
        const auto key = values.getAllKeys()[i];

        for (const auto* want : keys)
        {
            if (key.equalsIgnoreCase (want))
            {
                const auto v = values.getAllValues()[i].trim();

                if (v.isNotEmpty())
                    return v;
            }
        }
    }

    return {};
}
} // namespace

juce::String normalizeMatchText (juce::String value)
{
    value = stripFeaturing (value).toLowerCase();

    juce::String out;
    out.preallocateBytes (static_cast<size_t> (value.length()) + 8);

    for (int i = 0; i < value.length(); ++i)
    {
        const auto c = value[i];

        if (juce::CharacterFunctions::isLetterOrDigit (c))
            out << c;
        else if (c == ' ' || c == '-' || c == '_' || c == '\'' || c == '.')
            out << ' ';
    }

    while (out.contains ("  "))
        out = out.replace ("  ", " ");

    return out.trim();
}

double textMatchScore (const juce::String& a, const juce::String& b)
{
    const auto na = normalizeMatchText (a);
    const auto nb = normalizeMatchText (b);

    if (na.isEmpty() || nb.isEmpty())
        return 0.0;

    if (na == nb)
        return 1.0;

    if (na.contains (nb) || nb.contains (na))
        return 0.85;

    juce::StringArray ta, tb;
    ta.addTokens (na, " ", "");
    tb.addTokens (nb, " ", "");
    ta.trim();
    tb.trim();
    ta.removeEmptyStrings();
    tb.removeEmptyStrings();

    if (ta.isEmpty() || tb.isEmpty())
        return 0.0;

    int overlap = 0;

    for (const auto& t : ta)
        if (tb.contains (t))
            ++overlap;

    const auto unionSize = ta.size() + tb.size() - overlap;

    if (unionSize <= 0)
        return 0.0;

    return static_cast<double> (overlap) / static_cast<double> (unionSize);
}

double SongMetadata::scoreCandidate (const juce::String& candidateTitle,
                                     const juce::String& candidateArtist,
                                     const juce::String& candidateAlbum,
                                     const double candidateDurationSeconds) const
{
    double score = 0.0;

    const auto titleScore = textMatchScore (title, candidateTitle);
    const auto artistScore = textMatchScore (artist, candidateArtist);
    const auto albumScore = textMatchScore (album, candidateAlbum);

    // Title is required for a good automatic pick.
    score += titleScore * 90.0;

    if (title.isNotEmpty() && titleScore < 0.35)
        score -= 120.0; // wrong song title

    if (artist.isNotEmpty())
    {
        score += artistScore * 110.0;

        // Hard penalty: we know the artist and this candidate is clearly someone else
        // (e.g. Johnny Cash when we wanted Mike Ness / album version).
        if (artistScore < 0.28)
            score -= 200.0;
        else if (artistScore >= 0.75)
            score += 40.0; // strong artist confirmation
    }
    else
    {
        // No artist tag - album match becomes more important.
        if (album.isNotEmpty())
            score += albumScore * 55.0;
    }

    if (album.isNotEmpty() && albumScore > 0.4)
        score += albumScore * 35.0;

    if (durationSeconds > 5.0 && candidateDurationSeconds > 5.0)
    {
        const auto diff = std::abs (durationSeconds - candidateDurationSeconds);
        score -= diff * 1.8; // prefer same length / same arrangement

        if (diff < 3.0)
            score += 25.0;
        else if (diff > 45.0)
            score -= 40.0; // live vs studio, different cut, etc.
    }

    return score;
}

SongMetadata extractSongMetadata (const juce::File& audioFile, juce::AudioFormatManager& formatManager)
{
    SongMetadata meta;
    meta.sourceFile = audioFile;

    if (! audioFile.existsAsFile())
        return meta;

    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (audioFile)))
    {
        if (reader->sampleRate > 0.0 && reader->lengthInSamples > 0)
            meta.durationSeconds = static_cast<double> (reader->lengthInSamples) / reader->sampleRate;

        const auto& values = reader->metadataValues;
        meta.title = metaValue (values, { "title", "TITLE", "TIT2", "NAME", "TRACKNAME" });
        meta.artist = metaValue (values, { "artist", "ARTIST", "TPE1", "Author", "author",
                                           "ALBUMARTIST", "albumartist", "TPE2" });
        meta.album = metaValue (values, { "album", "ALBUM", "TALB" });

        // Prefer album artist when artist is missing (compilations aside)
        if (meta.artist.isEmpty())
            meta.artist = metaValue (values, { "album artist", "ALBUM ARTIST", "TPE2", "ALBUMARTIST" });
    }

    applyFilenameFallback (audioFile, meta);
    applyPathFallback (audioFile, meta);

    meta.title = meta.title.trim();
    meta.artist = stripFeaturing (meta.artist.trim());
    meta.album = meta.album.trim();

    return meta;
}

} // namespace jamstudio::notation
