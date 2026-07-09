#include "ProjectManager.h"

#include "../notation/LrcParser.h"
#include "../notation/MusicXmlParser.h"

namespace jamstudio::project
{

namespace
{
juce::var stemToVar (const StemState& stem)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("file", stem.filePath);
    obj->setProperty ("name", stem.name);
    obj->setProperty ("muted", stem.muted);
    obj->setProperty ("solo", stem.solo);
    obj->setProperty ("volume", stem.volume);
    return juce::var (obj);
}

StemState varToStem (const juce::var& value)
{
    StemState stem;

    if (auto* obj = value.getDynamicObject())
    {
        stem.filePath = obj->getProperty ("file").toString();
        stem.name = obj->getProperty ("name").toString();
        stem.muted = static_cast<bool> (obj->getProperty ("muted"));
        stem.solo = static_cast<bool> (obj->getProperty ("solo"));
        stem.volume = static_cast<float> (static_cast<double> (obj->getProperty ("volume")));
    }

    return stem;
}
} // namespace

ProjectData ProjectManager::captureState (const juce::File& songFile,
                                          const juce::File& scoreFile,
                                          const juce::File& lyricsFile,
                                          const jamstudio::notation::Score& score,
                                          const jamstudio::notation::LyricsTrack& lyrics,
                                          jamstudio::audio::TransportController& transport,
                                          const jamstudio::ui::TransportBar& transportBar)
{
    ProjectData data;
    data.songFilePath = songFile.getFullPathName();
    data.scoreFilePath = scoreFile.getFullPathName();
    data.lyricsFilePath = lyricsFile.getFullPathName();

    if (scoreFile.existsAsFile())
    {
        data.hasEmbeddedScore = false;
    }
    else if (! score.isEmpty())
    {
        data.hasEmbeddedScore = true;
        data.embeddedScore = score.toVar();
    }

    if (lyricsFile.existsAsFile())
    {
        data.hasEmbeddedLyrics = false;
    }
    else if (! lyrics.isEmpty())
    {
        data.hasEmbeddedLyrics = true;
        data.embeddedLyrics = lyrics.toVar();
    }
    data.metronomeEnabled = transportBar.isMetronomeEnabled();
    data.metronomeBpm = transportBar.getBpm();
    data.transportPosition = transport.getPosition();
    data.masterVolume = transportBar.getMasterVolume();

    auto& mixer = transport.getStemMixer();

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        if (const auto* stem = mixer.getStem (i))
        {
            StemState state;
            state.filePath = stem->getFile().getFullPathName();
            state.name = stem->getName();
            state.muted = stem->isMuted();
            state.solo = stem->isSolo();
            state.volume = stem->getVolume();
            data.stems.add (state);
        }
    }

    return data;
}

juce::File ProjectManager::getMediaStemsDirectory (const juce::File& projectFile)
{
    return projectFile.getParentDirectory()
        .getChildFile (projectFile.getFileNameWithoutExtension() + ".media")
        .getChildFile ("stems");
}

juce::File ProjectManager::resolveStemFile (const juce::String& storedPath,
                                            const juce::File& projectFile)
{
    if (storedPath.isEmpty())
        return {};

    const juce::File absolute (storedPath);

    if (absolute.existsAsFile())
        return absolute;

    if (projectFile != juce::File())
    {
        const auto projectDir = projectFile.getParentDirectory();

        // Relative to project directory
        const auto relative = projectDir.getChildFile (storedPath);

        if (relative.existsAsFile())
            return relative;

        // Same filename under "{Name}.media/stems/"
        const auto mediaMatch = getMediaStemsDirectory (projectFile)
                                    .getChildFile (absolute.getFileName());

        if (mediaMatch.existsAsFile())
            return mediaMatch;

        // Search media stems folder recursively (demucs nested folders, etc.)
        const auto mediaRoot = getMediaStemsDirectory (projectFile);

        if (mediaRoot.isDirectory())
        {
            for (const auto& entry : juce::RangedDirectoryIterator (mediaRoot, true,
                                                                    absolute.getFileName(),
                                                                    juce::File::findFiles))
                return entry.getFile();
        }
    }

    // Last resort: permanent Documents/JamStudio/Stems cache by filename
    const auto stemsCache = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                .getChildFile ("JamStudio")
                                .getChildFile ("Stems");

    if (stemsCache.isDirectory())
    {
        const auto name = absolute.getFileName();

        for (const auto& entry : juce::RangedDirectoryIterator (stemsCache, true, name,
                                                                juce::File::findFiles))
            return entry.getFile();
    }

    return {};
}

bool ProjectManager::relocateStemMedia (const juce::File& projectFile,
                                        ProjectData& data,
                                        juce::String& errorMessage)
{
    if (data.stems.isEmpty())
        return true;

    const auto mediaDir = getMediaStemsDirectory (projectFile);

    if (! mediaDir.createDirectory() && ! mediaDir.isDirectory())
    {
        errorMessage = "Could not create project media folder: " + mediaDir.getFullPathName();
        return false;
    }

    for (auto& stem : data.stems)
    {
        auto source = resolveStemFile (stem.filePath, projectFile);

        if (! source.existsAsFile())
            source = juce::File (stem.filePath);

        if (! source.existsAsFile())
        {
            errorMessage = "Missing stem file (cannot pack into project): " + stem.filePath
                           + "\n\nRe-run Practice setup for this song, or separate stems again, "
                             "then Save Project.";
            return false;
        }

        // Stable, flat name: prefer mixer name (Guitar.wav) so reloads stay clear.
        auto destName = stem.name.trim();

        if (destName.isEmpty())
            destName = source.getFileNameWithoutExtension();

        destName = juce::File::createLegalFileName (destName);

        if (! destName.endsWithIgnoreCase (".wav") && ! destName.endsWithIgnoreCase (".flac")
            && ! destName.endsWithIgnoreCase (".mp3") && ! destName.endsWithIgnoreCase (".ogg")
            && ! destName.endsWithIgnoreCase (".aiff"))
            destName += source.getFileExtension().isNotEmpty() ? source.getFileExtension()
                                                               : ".wav";

        auto dest = mediaDir.getChildFile (destName);

        // Avoid collisions (e.g. two "Other")
        if (dest.existsAsFile() && dest.getFullPathName() != source.getFullPathName())
        {
            // If contents already there from a previous save with same path identity, keep it.
            if (dest.getSize() == source.getSize()
                && dest.getLastModificationTime() >= source.getLastModificationTime())
            {
                stem.filePath = dest.getFullPathName();
                continue;
            }

            dest = mediaDir.getNonexistentChildFile (dest.getFileNameWithoutExtension(),
                                                     dest.getFileExtension());
        }

        if (source.getFullPathName() != dest.getFullPathName())
        {
            dest.deleteFile();

            if (! source.copyFileTo (dest))
            {
                errorMessage = "Failed to copy stem to permanent project media:\n"
                               + dest.getFullPathName();
                return false;
            }
        }

        stem.filePath = dest.getFullPathName();
    }

    return true;
}

bool ProjectManager::saveProject (const juce::File& projectFile, ProjectData& data)
{
    juce::String mediaError;

    // Always pack stems next to the project so reopening never depends on /tmp.
    if (! relocateStemMedia (projectFile, data, mediaError))
    {
        // Still try to write JSON with original paths if copy failed mid-session —
        // but surface the error by failing save so the user knows.
        juce::ignoreUnused (mediaError);
        // Prefer failing when we have stems that should be permanent.
        if (! data.stems.isEmpty())
            return false;
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", data.version);
    root->setProperty ("songFile", data.songFilePath);
    root->setProperty ("scoreFile", data.scoreFilePath);
    root->setProperty ("lyricsFile", data.lyricsFilePath);
    root->setProperty ("metronomeEnabled", data.metronomeEnabled);
    root->setProperty ("metronomeBpm", data.metronomeBpm);
    root->setProperty ("transportPosition", data.transportPosition);
    root->setProperty ("masterVolume", data.masterVolume);

    juce::Array<juce::var> stemArray;

    for (const auto& stem : data.stems)
        stemArray.add (stemToVar (stem));

    root->setProperty ("stems", stemArray);
    root->setProperty ("hasEmbeddedScore", data.hasEmbeddedScore);
    root->setProperty ("hasEmbeddedLyrics", data.hasEmbeddedLyrics);

    if (data.hasEmbeddedScore)
        root->setProperty ("embeddedScore", data.embeddedScore);

    if (data.hasEmbeddedLyrics)
        root->setProperty ("embeddedLyrics", data.embeddedLyrics);

    const auto json = juce::JSON::toString (juce::var (root), true);
    projectFile.getParentDirectory().createDirectory();
    return projectFile.replaceWithText (json);
}

bool ProjectManager::loadProject (const juce::File& projectFile,
                                ProjectData& data,
                                juce::String& errorMessage)
{
    if (! projectFile.existsAsFile())
    {
        errorMessage = "Project file does not exist.";
        return false;
    }

    juce::var parsed;
    const auto result = juce::JSON::parse (projectFile.loadFileAsString(), parsed);

    if (result.failed())
    {
        errorMessage = "Invalid project file: " + result.getErrorMessage();
        return false;
    }

    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
    {
        errorMessage = "Project file is not a valid JSON object.";
        return false;
    }

    data.version = static_cast<int> (root->getProperty ("version"));
    data.songFilePath = root->getProperty ("songFile").toString();
    data.scoreFilePath = root->getProperty ("scoreFile").toString();
    data.lyricsFilePath = root->getProperty ("lyricsFile").toString();
    data.metronomeEnabled = static_cast<bool> (root->getProperty ("metronomeEnabled"));
    data.metronomeBpm = static_cast<double> (root->getProperty ("metronomeBpm"));
    data.transportPosition = static_cast<double> (root->getProperty ("transportPosition"));
    if (root->hasProperty ("masterVolume"))
        data.masterVolume = static_cast<float> (static_cast<double> (root->getProperty ("masterVolume")));
    data.stems.clear();

    if (const auto* stems = root->getProperty ("stems").getArray())
    {
        for (const auto& stemVar : *stems)
            data.stems.add (varToStem (stemVar));
    }

    data.hasEmbeddedScore = static_cast<bool> (root->getProperty ("hasEmbeddedScore"));
    data.hasEmbeddedLyrics = static_cast<bool> (root->getProperty ("hasEmbeddedLyrics"));
    data.embeddedScore = root->getProperty ("embeddedScore");
    data.embeddedLyrics = root->getProperty ("embeddedLyrics");

    return true;
}

bool ProjectManager::applyState (const ProjectData& data,
                                 jamstudio::audio::TransportController& transport,
                                 jamstudio::ui::TransportBar& transportBar,
                                 jamstudio::notation::Score& score,
                                 jamstudio::notation::LyricsTrack& lyrics,
                                 juce::File& songFile,
                                 juce::File& scoreFile,
                                 juce::File& lyricsFile,
                                 juce::String& errorMessage,
                                 const juce::File& projectFile)
{
    transport.stop();

    juce::Array<juce::File> stemFiles;
    juce::StringArray missing;

    for (const auto& stem : data.stems)
    {
        const auto file = resolveStemFile (stem.filePath, projectFile);

        if (! file.existsAsFile())
        {
            missing.add (stem.name.isNotEmpty() ? stem.name + " → " + stem.filePath
                                                : stem.filePath);
            continue;
        }

        stemFiles.add (file);
    }

    if (stemFiles.isEmpty() && data.songFilePath.isNotEmpty())
    {
        const juce::File song (data.songFilePath);

        if (! song.existsAsFile())
        {
            errorMessage = "Missing song file: " + data.songFilePath;
            return false;
        }

        stemFiles.add (song);
        songFile = song;
    }

    if (stemFiles.isEmpty())
    {
        errorMessage = "Missing stem file(s). Stems were likely saved under /tmp and cleaned up.\n"
                       "Re-run Practice → New from Song (or Separate Stems), then Save Project.\n\n"
                       + missing.joinIntoString ("\n");
        return false;
    }

    if (missing.size() > 0 && stemFiles.size() < data.stems.size())
    {
        // Partial load is better than hard fail when some media survived.
        errorMessage = {};
    }

    auto& mixer = transport.getStemMixer();
    mixer.loadStems (stemFiles);

    for (int i = 0; i < mixer.getNumStems() && i < data.stems.size(); ++i)
    {
        // Match by order of successfully resolved stems — rebuild stem state carefully.
        const auto& stemState = data.stems.getReference (i);
        const auto resolved = resolveStemFile (stemState.filePath, projectFile);

        if (! resolved.existsAsFile())
            continue;

        // Find mixer index for this file
        for (int mi = 0; mi < mixer.getNumStems(); ++mi)
        {
            if (const auto* s = mixer.getStem (mi);
                s != nullptr && s->getFile() == resolved)
            {
                mixer.setStemMuted (mi, stemState.muted);
                mixer.setStemSolo (mi, stemState.solo);
                mixer.setStemVolume (mi, stemState.volume);

                if (stemState.name.isNotEmpty())
                    mixer.setStemName (mi, stemState.name);
                break;
            }
        }
    }

    if (data.songFilePath.isNotEmpty())
        songFile = juce::File (data.songFilePath);

    score.clear();
    lyrics.clear();
    scoreFile = juce::File();
    lyricsFile = juce::File();

    auto scoreLoaded = false;

    if (data.scoreFilePath.isNotEmpty())
    {
        const juce::File loadedScore (data.scoreFilePath);

        if (loadedScore.existsAsFile())
        {
            juce::String scoreError;

            if (jamstudio::notation::MusicXmlParser::parseFile (loadedScore, score, scoreError))
            {
                scoreFile = loadedScore;
                scoreLoaded = true;
            }
        }
    }

    if (! scoreLoaded && data.hasEmbeddedScore)
        scoreLoaded = jamstudio::notation::Score::fromVar (data.embeddedScore, score);

    auto lyricsLoaded = false;

    if (data.lyricsFilePath.isNotEmpty())
    {
        const juce::File loadedLyrics (data.lyricsFilePath);

        if (loadedLyrics.existsAsFile())
        {
            juce::String lyricsError;

            if (jamstudio::notation::LrcParser::parseFile (loadedLyrics, lyrics, lyricsError))
            {
                lyricsFile = loadedLyrics;
                lyricsLoaded = true;
            }
        }
    }

    if (! lyricsLoaded && data.hasEmbeddedLyrics)
        lyricsLoaded = jamstudio::notation::LyricsTrack::fromVar (data.embeddedLyrics, lyrics);

    transportBar.setMetronomeEnabled (data.metronomeEnabled);
    transportBar.setBpm (data.metronomeBpm);
    transportBar.setMasterVolume (data.masterVolume);
    transport.getMetronome().setEnabled (data.metronomeEnabled);
    transport.getMetronome().setBpm (data.metronomeBpm);
    transport.setPosition (data.transportPosition);

    errorMessage = {};
    return true;
}

} // namespace jamstudio::project