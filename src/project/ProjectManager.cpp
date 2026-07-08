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

bool ProjectManager::saveProject (const juce::File& projectFile, const ProjectData& data)
{
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
                                 juce::String& errorMessage)
{
    transport.stop();

    juce::Array<juce::File> stemFiles;

    for (const auto& stem : data.stems)
    {
        const juce::File file (stem.filePath);

        if (! file.existsAsFile())
        {
            errorMessage = "Missing stem file: " + stem.filePath;
            return false;
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
        errorMessage = "Project does not contain any audio files.";
        return false;
    }

    auto& mixer = transport.getStemMixer();
    mixer.loadStems (stemFiles);

    for (int i = 0; i < mixer.getNumStems() && i < data.stems.size(); ++i)
    {
        const auto& stemState = data.stems.getReference (i);
        mixer.setStemMuted (i, stemState.muted);
        mixer.setStemSolo (i, stemState.solo);
        mixer.setStemVolume (i, stemState.volume);

        if (stemState.name.isNotEmpty())
            mixer.setStemName (i, stemState.name);
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