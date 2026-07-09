#include "TabLibraryClient.h"

#include "SongMetadata.h"

namespace jamstudio::notation
{

namespace
{
constexpr auto userAgent = "JamStudio/0.9.6";

juce::String readEntryString (const juce::var& object, const char* key)
{
    if (auto* props = object.getDynamicObject())
        return props->getProperty (key).toString().trim();

    return {};
}

juce::StringArray readEntryTags (const juce::var& object)
{
    juce::StringArray tags;

    if (auto* props = object.getDynamicObject())
    {
        const auto tagsVar = props->getProperty ("tags");

        if (tagsVar.isArray())
        {
            for (const auto& tag : *tagsVar.getArray())
                tags.add (tag.toString().trim());
        }
    }

    return tags;
}

TabLibraryEntry parseEntry (const juce::var& entryVar, const juce::String& sourceName)
{
    TabLibraryEntry entry;
    entry.id = readEntryString (entryVar, "id");
    entry.title = readEntryString (entryVar, "title");
    entry.artist = readEntryString (entryVar, "artist");
    entry.description = readEntryString (entryVar, "description");
    entry.tags = readEntryTags (entryVar);
    entry.sourceName = sourceName;

    const auto url = readEntryString (entryVar, "url");
    const auto path = readEntryString (entryVar, "path");

    if (url.isNotEmpty())
        entry.downloadUrl = url;
    else if (path.isNotEmpty())
        entry.downloadUrl = path;

    return entry;
}
} // namespace

juce::File TabLibraryClient::findBundledCatalogFile()
{
    const juce::String catalogName = "tab-library.json";

    const auto candidates = {
        juce::File::getCurrentWorkingDirectory().getChildFile ("samples").getChildFile (catalogName),
        juce::File::getSpecialLocation (juce::File::currentExecutableFile)
            .getParentDirectory()
            .getChildFile ("samples")
            .getChildFile (catalogName),
        juce::File::getSpecialLocation (juce::File::currentExecutableFile)
            .getParentDirectory()
            .getParentDirectory()
            .getChildFile ("samples")
            .getChildFile (catalogName)
    };

    for (const auto& candidate : candidates)
    {
        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

juce::String TabLibraryClient::fetchUrl (const juce::String& url)
{
    juce::ChildProcess process;

    if (! process.start ({ "curl", "-sS", "-L", "--max-time", "20",
                           "-A", userAgent, url }))
        return {};

    juce::MemoryOutputStream output;

    for (;;)
    {
        char buffer[8192];
        const auto bytesRead = process.readProcessOutput (buffer, static_cast<int> (sizeof (buffer)));

        if (bytesRead <= 0)
            break;

        output.write (buffer, static_cast<size_t> (bytesRead));
    }

    process.waitForProcessToFinish (25000);
    return output.toString();
}

TabLibraryFetchResult TabLibraryClient::parseCatalogJson (const juce::String& jsonText,
                                                          const bool fromNetwork)
{
    TabLibraryFetchResult result;
    const auto parsed = juce::JSON::parse (jsonText);

    if (! parsed.isObject())
    {
        result.errorMessage = "Catalog response was not valid JSON.";
        return result;
    }

    if (auto* props = parsed.getDynamicObject())
    {
        result.catalog.name = props->getProperty ("name").toString();
        result.catalog.catalogBaseUrl = props->getProperty ("catalogBaseUrl").toString().trim();

        if (result.catalog.name.isEmpty())
            result.catalog.name = "Tab Library";

        const auto scoresVar = props->getProperty ("scores");

        if (scoresVar.isArray())
        {
            for (const auto& entryVar : *scoresVar.getArray())
            {
                auto entry = parseEntry (entryVar, result.catalog.name);

                if (entry.title.isNotEmpty() && entry.downloadUrl.isNotEmpty())
                    result.catalog.entries.add (std::move (entry));
            }
        }
    }

    if (result.catalog.entries.isEmpty())
    {
        result.errorMessage = "Catalog contained no downloadable scores.";
        return result;
    }

    result.catalog.loadedFromNetwork = fromNetwork;
    result.success = true;
    return result;
}

TabLibraryFetchResult TabLibraryClient::loadCatalog()
{
    TabLibraryFetchResult result;

    const auto remoteJson = fetchUrl (defaultRemoteCatalogUrl);

    if (remoteJson.isNotEmpty())
    {
        result = parseCatalogJson (remoteJson, true);

        if (result.success)
            return result;
    }

    const auto bundledCatalog = findBundledCatalogFile();

    if (! bundledCatalog.existsAsFile())
    {
        if (result.errorMessage.isEmpty())
            result.errorMessage = "Could not reach the online tab library. Check your internet connection.";

        return result;
    }

    result = parseCatalogJson (bundledCatalog.loadFileAsString(), false);

    if (result.success && result.catalog.catalogBaseUrl.isEmpty())
        result.catalog.catalogBaseUrl = bundledCatalog.getParentDirectory().getFullPathName();

    if (! result.success && result.errorMessage.isEmpty())
        result.errorMessage = "Failed to read bundled tab library catalog.";

    return result;
}

void TabLibraryClient::fetchCatalogAsync (FetchCallback onComplete)
{
    juce::Thread::launch ([onComplete = std::move (onComplete)]
    {
        const auto result = loadCatalog();
        juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
    });
}

juce::Array<TabLibraryEntry> TabLibraryClient::search (const TabLibraryCatalog& catalog,
                                                       const juce::String& query)
{
    const auto trimmedQuery = query.trim().toLowerCase();

    if (trimmedQuery.isEmpty())
        return catalog.entries;

    juce::Array<TabLibraryEntry> matches;

    for (const auto& entry : catalog.entries)
    {
        const auto haystack = (entry.title + " " + entry.artist + " " + entry.description
                               + " " + entry.tags.joinIntoString (" ")).toLowerCase();

        if (haystack.contains (trimmedQuery))
            matches.add (entry);
    }

    return matches;
}

juce::Array<TabLibraryEntry> TabLibraryClient::searchByMetadata (const TabLibraryCatalog& catalog,
                                                                 const juce::String& title,
                                                                 const juce::String& artist)
{
    struct Ranked
    {
        TabLibraryEntry entry;
        double score = 0.0;
    };

    juce::Array<Ranked> ranked;
    SongMetadata probe;
    probe.title = title;
    probe.artist = artist;

    for (const auto& entry : catalog.entries)
    {
        auto score = probe.scoreCandidate (entry.title, entry.artist);

        // Also allow free-text presence of tokens when scoring is weak but related
        if (score < 30.0 && title.isNotEmpty())
        {
            const auto hay = (entry.title + " " + entry.artist).toLowerCase();
            const auto t = title.toLowerCase();
            const auto a = artist.toLowerCase();

            if (hay.contains (t))
                score += 40.0;

            if (a.isNotEmpty() && hay.contains (a))
                score += 50.0;
        }

        if (score >= 35.0)
            ranked.add ({ entry, score });
    }

    struct RankSorter
    {
        static int compareElements (const Ranked& x, const Ranked& y)
        {
            if (x.score > y.score) return -1;
            if (y.score > x.score) return 1;
            return 0;
        }
    };

    RankSorter sorter;
    ranked.sort (sorter);

    juce::Array<TabLibraryEntry> matches;

    for (const auto& r : ranked)
        matches.add (r.entry);

    // Fallback: old free-text search with "artist title" query only
    if (matches.isEmpty())
    {
        juce::StringArray parts;

        if (artist.isNotEmpty())
            parts.add (artist);

        if (title.isNotEmpty())
            parts.add (title);

        matches = search (catalog, parts.joinIntoString (" "));
    }

    return matches;
}

juce::String TabLibraryClient::resolveDownloadUrl (const TabLibraryEntry& entry,
                                                     const TabLibraryCatalog& catalog)
{
    if (entry.downloadUrl.startsWithIgnoreCase ("http://")
        || entry.downloadUrl.startsWithIgnoreCase ("https://"))
        return entry.downloadUrl;

    if (catalog.catalogBaseUrl.startsWithIgnoreCase ("http://")
        || catalog.catalogBaseUrl.startsWithIgnoreCase ("https://"))
        return juce::URL (catalog.catalogBaseUrl).getChildURL (entry.downloadUrl).toString (true);

    return {};
}

juce::File TabLibraryClient::resolveLocalCatalogFile (const TabLibraryEntry& entry,
                                                      const TabLibraryCatalog& catalog)
{
    if (entry.downloadUrl.startsWithIgnoreCase ("http://")
        || entry.downloadUrl.startsWithIgnoreCase ("https://"))
        return {};

    juce::File baseDir;

    if (catalog.catalogBaseUrl.isNotEmpty()
        && ! catalog.catalogBaseUrl.startsWithIgnoreCase ("http"))
    {
        baseDir = juce::File (catalog.catalogBaseUrl);
    }
    else
    {
        const auto bundledCatalog = findBundledCatalogFile();

        if (bundledCatalog.existsAsFile())
            baseDir = bundledCatalog.getParentDirectory();
    }

    if (! baseDir.exists())
        return {};

    return baseDir.getChildFile (entry.downloadUrl);
}

void TabLibraryClient::downloadScoreAsync (const TabLibraryEntry& entry,
                                           const TabLibraryCatalog& catalog,
                                           DownloadCallback onComplete)
{
    juce::Thread::launch ([entry, catalog, onComplete = std::move (onComplete)]
    {
        TabLibraryDownloadResult result;

        const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("JamStudio")
                                 .getChildFile ("tab-library");

        if (! tempDir.createDirectory())
        {
            result.errorMessage = "Could not create a temporary download folder.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        const auto safeName = entry.title.replaceCharacters ("\\/:*?\"<>|", "--------");
        const auto extension = entry.downloadUrl.fromLastOccurrenceOf (".", false, false);
        const auto outputFile = tempDir.getChildFile (safeName + "." + (extension.isNotEmpty() ? extension : "musicxml"));

        if (const auto localFile = resolveLocalCatalogFile (entry, catalog); localFile.existsAsFile())
        {
            if (localFile.copyFileTo (outputFile))
            {
                result.success = true;
                result.downloadedFile = outputFile;
            }
            else
            {
                result.errorMessage = "Could not copy score from local library.";
            }

            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        const auto downloadUrl = resolveDownloadUrl (entry, catalog);

        if (downloadUrl.isEmpty())
        {
            result.errorMessage = "Score does not have a downloadable URL.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        juce::ChildProcess process;

        if (process.start ({ "curl", "-sS", "-L", "--max-time", "30",
                              "-A", userAgent,
                              "-o", outputFile.getFullPathName(),
                              downloadUrl })
            && process.waitForProcessToFinish (35000) == 0
            && outputFile.existsAsFile()
            && outputFile.getSize() > 0)
        {
            result.success = true;
            result.downloadedFile = outputFile;
        }
        else if (outputFile.existsAsFile())
            outputFile.deleteFile();

        if (! result.success && result.errorMessage.isEmpty())
            result.errorMessage = "Failed to download score from the library.";

        juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
    });
}

} // namespace jamstudio::notation