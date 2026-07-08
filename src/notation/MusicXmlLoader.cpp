#include "MusicXmlLoader.h"

namespace jamstudio::notation
{

bool MusicXmlLoader::loadScoreXmlFromFile (const juce::File& file,
                                           juce::String& xmlText,
                                           juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "Score file does not exist.";
        return false;
    }

    const auto extension = file.getFileExtension().toLowerCase();

    if (extension == ".mxl")
        return extractFromMxl (file, xmlText, errorMessage);

    xmlText = file.loadFileAsString();

    if (xmlText.isEmpty())
    {
        errorMessage = "Score file is empty.";
        return false;
    }

    return true;
}

bool MusicXmlLoader::extractFromMxl (const juce::File& file,
                                     juce::String& xmlText,
                                     juce::String& errorMessage)
{
    juce::ZipFile zip (file);

    if (zip.getNumEntries() == 0)
    {
        errorMessage = "MXL archive is empty.";
        return false;
    }

    juce::String rootPath;

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry (i);

        if (entry == nullptr)
            continue;

        const auto entryName = juce::String (entry->filename);

        if (entryName.endsWithIgnoreCase ("META-INF/container.xml"))
        {
            if (auto stream = std::unique_ptr<juce::InputStream> (zip.createStreamForEntry (*entry)))
            {
                if (auto containerXml = juce::XmlDocument::parse (stream->readEntireStreamAsString()))
                {
                    if (const auto* rootfiles = containerXml->getChildByName ("rootfiles"))
                    {
                        if (const auto* rootfile = rootfiles->getChildByName ("rootfile"))
                            rootPath = rootfile->getStringAttribute ("full-path");
                    }
                }
            }

            break;
        }
    }

    if (rootPath.isEmpty())
    {
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry (i);

            if (entry == nullptr)
                continue;

            const auto entryName = juce::String (entry->filename);

            if (entryName.endsWithIgnoreCase (".xml") || entryName.endsWithIgnoreCase (".musicxml"))
            {
                rootPath = entryName;
                break;
            }
        }
    }

    if (rootPath.isEmpty())
    {
        errorMessage = "Could not find score XML inside MXL archive.";
        return false;
    }

    const auto* scoreEntry = zip.getEntry (rootPath);

    if (scoreEntry == nullptr)
    {
        errorMessage = "MXL root file not found: " + rootPath;
        return false;
    }

    if (auto stream = std::unique_ptr<juce::InputStream> (zip.createStreamForEntry (*scoreEntry)))
    {
        xmlText = stream->readEntireStreamAsString();

        if (xmlText.isNotEmpty())
            return true;
    }

    errorMessage = "Failed to read score XML from MXL archive.";
    return false;
}

} // namespace jamstudio::notation