#include "SongMetadata.h"

namespace jamstudio::notation
{

namespace
{
juce::String cleanToken (juce::String value)
{
    value = value.trim();
    // Drop common track-number prefixes: "03 - Title", "03. Title"
    while (value.isNotEmpty() && juce::CharacterFunctions::isDigit (value[0]))
        value = value.substring (1).trim();

    if (value.startsWithChar ('-') || value.startsWithChar ('.') || value.startsWithChar ('_'))
        value = value.substring (1).trim();

    return value.trim();
}

void applyFilenameFallback (const juce::File& audioFile, SongMetadata& meta)
{
    auto base = audioFile.getFileNameWithoutExtension().trim();

    // Patterns: "Artist - Title", "Artist – Title", "Artist_Title"
    for (const auto* sep : { " - ", " – ", " — ", "_" })
    {
        if (base.contains (sep))
        {
            auto artist = base.upToFirstOccurrenceOf (sep, false, false).trim();
            auto title = base.fromFirstOccurrenceOf (sep, false, false).trim();
            artist = cleanToken (artist);
            title = cleanToken (title);

            if (meta.artist.isEmpty() && artist.isNotEmpty())
                meta.artist = artist;

            if (meta.title.isEmpty() && title.isNotEmpty())
                meta.title = title;

            return;
        }
    }

    if (meta.title.isEmpty())
        meta.title = cleanToken (base);
}

juce::String metaValue (const juce::StringPairArray& values, std::initializer_list<const char*> keys)
{
    for (const auto* key : keys)
    {
        const auto v = values.getValue (key, {}).trim();

        if (v.isNotEmpty())
            return v;
    }

    // Case-insensitive scan
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
        meta.title = metaValue (values, { "title", "TITLE", "TIT2", "NAME" });
        meta.artist = metaValue (values, { "artist", "ARTIST", "TPE1", "Author", "author" });
        meta.album = metaValue (values, { "album", "ALBUM", "TALB" });
    }

    applyFilenameFallback (audioFile, meta);
    return meta;
}

} // namespace jamstudio::notation
