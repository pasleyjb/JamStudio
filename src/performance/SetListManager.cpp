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

juce::File SetListManager::mediaFolderForSetList (const juce::File& setListFile)
{
    return setListFile.getSiblingFile (setListFile.getFileNameWithoutExtension() + ".media");
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

namespace
{
juce::String packageFileIntoMedia (const juce::File& source,
                                   const juce::File& mediaDir,
                                   const juce::String& songTag,
                                   int index)
{
    if (! source.existsAsFile())
        return {};

    mediaDir.createDirectory();
    const auto safeSong = juce::File::createLegalFileName (songTag.isNotEmpty() ? songTag : "song");
    auto dest = mediaDir.getChildFile (safeSong + "_" + juce::String (index).paddedLeft ('0', 2)
                                       + "_" + source.getFileName());

    // Already inside this media folder — keep relative name.
    if (source.isAChildOf (mediaDir))
        return source.getRelativePathFrom (mediaDir.getParentDirectory());

    if (dest.existsAsFile() && dest.getSize() == source.getSize())
        return dest.getRelativePathFrom (mediaDir.getParentDirectory());

    if (! source.copyFileTo (dest))
        return source.getFullPathName(); // fall back to absolute

    return dest.getRelativePathFrom (mediaDir.getParentDirectory());
}
} // namespace

bool SetListManager::saveSetList (const juce::File& file, SetList& list)
{
    file.getParentDirectory().createDirectory();
    const auto mediaDir = mediaFolderForSetList (file);

    for (int si = 0; si < list.songs.size(); ++si)
    {
        auto& song = list.songs.getReference (si);
        const auto tag = juce::String (si + 1) + "_" + song.displayName;

        if (song.stageMediaKind == StageMediaKind::video && song.stageMediaPath.isNotEmpty())
        {
            const auto src = song.resolveStageMediaFile (list.sourceFile().existsAsFile()
                                                             ? list.sourceFile()
                                                             : file);
            if (src.existsAsFile())
                song.stageMediaPath = packageFileIntoMedia (src, mediaDir, tag, 0);
        }
        else if (song.stageMediaKind == StageMediaKind::slideshow)
        {
            juce::Array<juce::File> slides = song.resolveSlideFiles (
                list.sourceFile().existsAsFile() ? list.sourceFile() : file);

            if (slides.isEmpty() && song.stageMediaPath.isNotEmpty())
            {
                auto root = song.resolveStageMediaFile (list.sourceFile().existsAsFile()
                                                            ? list.sourceFile()
                                                            : file);
                if (root.isDirectory())
                {
                    for (const auto& entry : juce::RangedDirectoryIterator (
                             root, false, "*.jpg;*.jpeg;*.png;*.gif;*.bmp;*.webp", juce::File::findFiles))
                        slides.add (entry.getFile());
                    slides.sort();
                }
                else if (root.existsAsFile())
                {
                    slides.add (root);
                }
            }

            juce::Array<juce::String> packaged;
            for (int i = 0; i < slides.size(); ++i)
            {
                const auto rel = packageFileIntoMedia (slides.getReference (i), mediaDir, tag, i);
                if (rel.isNotEmpty())
                    packaged.add (rel);
            }

            if (! packaged.isEmpty())
            {
                song.stageSlidePaths = packaged;
                song.stageMediaPath = packaged.getFirst();
            }
        }
    }

    list.sourceFilePath = file.getFullPathName();
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
    list.sourceFilePath = file.getFullPathName();
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

        mixer.setStemBusSend (i, jamstudio::audio::MixBus::foh, match->volume);
        mixer.setStemBusSend (i, jamstudio::audio::MixBus::monitorA, match->monA);
        mixer.setStemBusSend (i, jamstudio::audio::MixBus::monitorB, match->monB);
        mixer.setStemMuted (i, match->muted);
        mixer.setStemSolo (i, match->solo);
    }
}

juce::Array<StemMixPref> captureStemPrefs (const jamstudio::audio::StemMixer& mixer)
{
    juce::Array<StemMixPref> prefs;

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        const auto* stem = mixer.getStem (i);
        if (stem == nullptr)
            continue;

        StemMixPref p;
        p.stemName = stem->getName().isNotEmpty()
                         ? stem->getName()
                         : jamstudio::audio::stemTypeToString (stem->getType());
        p.volume = stem->getBusSend (jamstudio::audio::MixBus::foh);
        p.monA = stem->getBusSend (jamstudio::audio::MixBus::monitorA);
        p.monB = stem->getBusSend (jamstudio::audio::MixBus::monitorB);
        p.muted = stem->isMuted();
        p.solo = stem->isSolo();
        prefs.add (p);
    }

    return prefs;
}

} // namespace jamstudio::performance
