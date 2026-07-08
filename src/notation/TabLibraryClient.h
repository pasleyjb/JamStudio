#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

/** A score entry in the online tab library catalog. */
struct TabLibraryEntry
{
    juce::String id;
    juce::String title;
    juce::String artist;
    juce::String description;
    juce::StringArray tags;
    juce::String downloadUrl;
    juce::String sourceName;
};

struct TabLibraryCatalog
{
    juce::String name;
    juce::String catalogBaseUrl;
    juce::Array<TabLibraryEntry> entries;
    bool loadedFromNetwork = false;
};

struct TabLibraryFetchResult
{
    bool success = false;
    juce::String errorMessage;
    TabLibraryCatalog catalog;
};

struct TabLibraryDownloadResult
{
    bool success = false;
    juce::String errorMessage;
    juce::File downloadedFile;
};

/** Fetches and searches the JamStudio online tab library catalog. */
class TabLibraryClient
{
public:
    static constexpr const char* defaultRemoteCatalogUrl =
        "https://raw.githubusercontent.com/pasleyjb/JamStudio/develop/samples/tab-library.json";

    using FetchCallback = std::function<void (TabLibraryFetchResult)>;
    using DownloadCallback = std::function<void (TabLibraryDownloadResult)>;

    void fetchCatalogAsync (FetchCallback onComplete);
    void downloadScoreAsync (const TabLibraryEntry& entry,
                             const TabLibraryCatalog& catalog,
                             DownloadCallback onComplete);

    [[nodiscard]] static juce::Array<TabLibraryEntry> search (const TabLibraryCatalog& catalog,
                                                             const juce::String& query);

private:
    [[nodiscard]] static TabLibraryFetchResult loadCatalog();
    [[nodiscard]] static TabLibraryFetchResult parseCatalogJson (const juce::String& jsonText,
                                                                 bool fromNetwork);
    [[nodiscard]] static juce::File findBundledCatalogFile();
    [[nodiscard]] static juce::String fetchUrl (const juce::String& url);
    [[nodiscard]] static juce::String resolveDownloadUrl (const TabLibraryEntry& entry,
                                                          const TabLibraryCatalog& catalog);
    [[nodiscard]] static juce::File resolveLocalCatalogFile (const TabLibraryEntry& entry,
                                                             const TabLibraryCatalog& catalog);
};

} // namespace jamstudio::notation