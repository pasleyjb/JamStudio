#include "SetListManager.h"

#include "../audio/StemMixer.h"
#include "../audio/StemType.h"

namespace jamstudio::performance
{

juce::File SetListManager::getSetListsDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("JamStudio")
                   .getChildFile ("SetLists");
    dir.createDirectory();
    return dir;
}

juce::File SetListManager::defaultSetListFile()
{
    return getSetListsDirectory().getChildFile ("default.setlist");
}

juce::Array<juce::File> SetListManager::listSavedSetLists()
{
    juce::Array<juce::File> files;
    const auto dir = getSetListsDirectory();

    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*.setlist", juce::File::findFiles))
        files.add (entry.getFile());

    files.sort();
    return files;
}

juce::Array<juce::File> SetListManager::listAvailableProjects()
{
    juce::Array<juce::File> files;
    const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("JamStudio")
                         .getChildFile ("Projects");

    if (! dir.isDirectory())
        return files;

    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*.jamstudio", juce::File::findFiles))
        files.add (entry.getFile());

    files.sort();
    return files;
}

bool SetListManager::saveSetList (const juce::File& file, const SetList& list)
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (list.toVar(), true));
}

bool SetListManager::loadSetList (const juce::File& file, SetList& list, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "Set list file does not exist.";
        return false;
    }

    juce::var parsed;
    const auto result = juce::JSON::parse (file.loadFileAsString(), parsed);

    if (result.failed())
    {
        error = result.getErrorMessage();
        return false;
    }

    list = SetList::fromVar (parsed);
    return true;
}

void applyStemPrefs (jamstudio::audio::StemMixer& mixer, const juce::Array<StemMixPref>& prefs)
{
    if (prefs.isEmpty())
        return;

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        auto* stem = mixer.getStem (i);

        if (stem == nullptr)
            continue;

        const auto stemLabel = stem->getName().isNotEmpty()
                                   ? stem->getName()
                                   : jamstudio::audio::stemTypeToString (stem->getType());

        const StemMixPref* match = nullptr;

        for (const auto& pref : prefs)
        {
            if (pref.stemName.isEmpty())
                continue;

            if (stemLabel.containsIgnoreCase (pref.stemName)
                || pref.stemName.containsIgnoreCase (stemLabel)
                || jamstudio::audio::stemTypeToString (stem->getType()).equalsIgnoreCase (pref.stemName))
            {
                match = &pref;
                break;
            }
        }

        if (match == nullptr)
            continue;

        mixer.setStemVolume (i, match->volume);
        mixer.setStemMuted (i, match->muted);
        mixer.setStemSolo (i, match->solo);
    }
}

} // namespace jamstudio::performance
