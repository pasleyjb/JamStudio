#include "PracticeSetupPipeline.h"

#include "../audio/StemType.h"
#include "../notation/MusicXmlParser.h"

#include <cmath>

namespace jamstudio::app
{

namespace
{
juce::String stemDisplayName (const juce::File& file)
{
    return jamstudio::audio::stemTypeToString (jamstudio::audio::stemTypeFromFileName (file.getFileName()));
}
} // namespace

PracticeSetupPipeline::PracticeSetupPipeline (jamstudio::ai::DemucsSeparator& demucs,
                                              jamstudio::ai::WhisperTranscriber& whisper,
                                              jamstudio::ai::BasicPitchTranscriber& basicPitch,
                                              juce::AudioFormatManager& formats)
    : demucsSeparator (demucs),
      whisperTranscriber (whisper),
      basicPitchTranscriber (basicPitch),
      formatManager (formats)
{
}

void PracticeSetupPipeline::start (const juce::File& songFile,
                                   ProgressCallback onProgress,
                                   CompleteCallback onComplete)
{
    cancel();

    progressCallback = std::move (onProgress);
    completeCallback = std::move (onComplete);
    shouldCancel = false;
    running = true;
    const auto job = ++generation;

    result = {};
    result.songFile = songFile;
    webTabsAttempted = webLyricsAttempted = false;
    webTabsDone = webLyricsDone = false;
    stemsToTranscribe.clear();
    nextStemIndex = 0;

    if (! songFile.existsAsFile())
    {
        finishFailure ("Song file does not exist.");
        return;
    }

    metadata = jamstudio::notation::extractSongMetadata (songFile, formatManager);
    result.projectTitle = sanitizeProjectTitle (
        metadata.title.isNotEmpty() ? metadata.title
                                    : songFile.getFileNameWithoutExtension());

    if (! demucsSeparator.isAvailable())
    {
        finishFailure ("Demucs is not available. Install it from AI Tools setup, then try again.");
        return;
    }

    reportProgress (0.02f, "Separating stems for \"" + result.projectTitle + "\"...");

    demucsSeparator.separateAsync (songFile,
        [this, job] (const jamstudio::ai::SeparationResult& separation)
        {
            if (! isStillActive (job))
                return;

            afterStems (separation);
        },
        [this, job] (const float progress, const juce::String& message)
        {
            if (! isStillActive (job))
                return;

            // Stem separation is ~0–40% of the overall pipeline.
            reportProgress (0.02f + progress * 0.38f, message);
        });
}

void PracticeSetupPipeline::cancel()
{
    shouldCancel = true;
    ++generation;
    demucsSeparator.cancel();
    whisperTranscriber.cancel();
    basicPitchTranscriber.cancel();
    running = false;
}

bool PracticeSetupPipeline::isStillActive (const uint32_t gen) const noexcept
{
    return running.load() && ! shouldCancel.load() && generation.load() == gen;
}

void PracticeSetupPipeline::reportProgress (const float progress, const juce::String& message)
{
    if (progressCallback)
        progressCallback (juce::jlimit (0.0f, 1.0f, progress), message);
}

void PracticeSetupPipeline::finishSuccess()
{
    running = false;
    result.success = true;
    result.cancelled = false;

    if (completeCallback)
        completeCallback (std::move (result));
}

void PracticeSetupPipeline::finishFailure (const juce::String& error)
{
    running = false;
    result.success = false;
    result.cancelled = false;
    result.errorMessage = error;

    if (completeCallback)
        completeCallback (std::move (result));
}

void PracticeSetupPipeline::finishCancelled()
{
    running = false;
    result.success = false;
    result.cancelled = true;
    result.errorMessage = "Practice setup cancelled.";

    if (completeCallback)
        completeCallback (std::move (result));
}

void PracticeSetupPipeline::afterStems (const jamstudio::ai::SeparationResult& separation)
{
    if (shouldCancel.load())
    {
        finishCancelled();
        return;
    }

    if (! separation.success || separation.stemFiles.isEmpty())
    {
        if (separation.errorMessage.containsIgnoreCase ("cancel"))
        {
            finishCancelled();
            return;
        }

        finishFailure (separation.errorMessage.isNotEmpty()
                           ? separation.errorMessage
                           : "Stem separation failed.");
        return;
    }

    result.stemFiles = separation.stemFiles;
    reportProgress (0.42f, "Looking up tabs and lyrics online...");

    fetchWebTabs();
    fetchWebLyrics();
}

void PracticeSetupPipeline::fetchWebTabs()
{
    webTabsAttempted = true;
    const auto job = generation.load();
    const auto title = metadata.title;
    const auto artist = metadata.artist;
    const auto queryLabel = metadata.displayLabel();

    tabLibraryClient.fetchCatalogAsync (
        [this, job, title, artist, queryLabel] (jamstudio::notation::TabLibraryFetchResult catalogResult)
        {
            if (! isStillActive (job))
                return;

            if (! catalogResult.success || catalogResult.catalog.entries.isEmpty())
            {
                result.notes.add ("Tab library unavailable — will try AI tabs.");
                webTabsDone = true;
                afterWebAssets();
                return;
            }

            // Prefer artist + title ranking (not title-only substring matches).
            const auto matches = jamstudio::notation::TabLibraryClient::searchByMetadata (
                catalogResult.catalog, title, artist);

            if (matches.isEmpty())
            {
                result.notes.add ("No matching tabs online for \"" + queryLabel + "\".");
                webTabsDone = true;
                afterWebAssets();
                return;
            }

            const auto entry = matches.getReference (0);
            const auto matchScore = metadata.scoreCandidate (entry.title, entry.artist);

            // Reject weak title-only hits when we know the artist (wrong band's arrangement).
            if (artist.isNotEmpty() && matchScore < 40.0)
            {
                result.notes.add ("Online tabs for \"" + entry.title + "\" did not match artist \""
                                  + artist + "\" — using AI tabs instead.");
                webTabsDone = true;
                afterWebAssets();
                return;
            }

            reportProgress (0.48f, "Downloading tabs: " + entry.artist + " — " + entry.title + "...");

            tabLibraryClient.downloadScoreAsync (entry, catalogResult.catalog,
                [this, job, entry] (jamstudio::notation::TabLibraryDownloadResult download)
                {
                    if (! isStillActive (job))
                        return;

                    if (download.success && download.downloadedFile.existsAsFile())
                    {
                        juce::String parseError;
                        jamstudio::notation::Score parsed;

                        if (jamstudio::notation::MusicXmlParser::parseFile (download.downloadedFile,
                                                                            parsed,
                                                                            parseError)
                            && ! parsed.isEmpty())
                        {
                            result.score = std::move (parsed);
                            result.score.setTitle (entry.artist.isNotEmpty()
                                                       ? entry.artist + " — " + entry.title
                                                       : entry.title);
                            result.scoreSource = "web";
                            result.notes.add ("Tabs from online library: "
                                              + (entry.artist.isNotEmpty()
                                                     ? entry.artist + " — " + entry.title
                                                     : entry.title));
                        }
                        else
                        {
                            result.notes.add ("Downloaded tabs could not be parsed"
                                              + (parseError.isNotEmpty() ? (": " + parseError) : "."));
                        }
                    }
                    else
                    {
                        result.notes.add (download.errorMessage.isNotEmpty()
                                              ? download.errorMessage
                                              : "Tab download failed.");
                    }

                    webTabsDone = true;
                    afterWebAssets();
                });
        });
}

void PracticeSetupPipeline::fetchWebLyrics()
{
    webLyricsAttempted = true;
    const auto job = generation.load();

    if (! metadata.hasSearchableFields())
    {
        result.notes.add ("No song metadata for lyric search.");
        webLyricsDone = true;
        afterWebAssets();
        return;
    }

    onlineLyricsClient.searchAsync (metadata,
        [this, job] (juce::Array<jamstudio::notation::OnlineLyricsCandidate> candidates, juce::String error)
        {
            if (! isStillActive (job))
                return;

            if (error.isNotEmpty() || candidates.isEmpty())
            {
                result.notes.add (error.isNotEmpty() ? error : "No online lyrics found.");
                webLyricsDone = true;
                afterWebAssets();
                return;
            }

            // Rank by artist + title (+ album/duration). Never prefer a famous cover
            // just because the title matches (e.g. Johnny Cash vs album artist).
            int bestIndex = -1;
            double bestScore = -1.0e9;

            for (int i = 0; i < candidates.size(); ++i)
            {
                const auto& c = candidates.getReference (i);
                auto score = metadata.scoreCandidate (c.trackName, c.artistName,
                                                      c.albumName, c.durationSeconds);

                if (c.hasSyncedLyrics)
                    score += 25.0;
                else if (c.plainLyrics.isNotEmpty())
                    score += 8.0;

                if (c.instrumental)
                    score -= 60.0;

                if (score > bestScore)
                {
                    bestScore = score;
                    bestIndex = i;
                }
            }

            // Minimum confidence: require real title match; if artist known, require it too.
            const double minAccept = metadata.artist.isNotEmpty() ? 50.0 : 35.0;

            if (bestIndex < 0 || bestScore < minAccept)
            {
                result.notes.add ("Online lyrics did not match "
                                  + metadata.displayLabel()
                                  + " closely enough (best score "
                                  + juce::String (bestScore, 1)
                                  + ") — will try AI lyrics.");
                webLyricsDone = true;
                afterWebAssets();
                return;
            }

            const auto& best = candidates.getReference (bestIndex);
            juce::String lyricsError;
            jamstudio::notation::LyricsTrack lyrics;

            if (jamstudio::notation::OnlineLyricsClient::candidateToLyrics (best, lyrics, lyricsError)
                && ! lyrics.isEmpty())
            {
                result.lyrics = std::move (lyrics);
                result.lyricsSource = "web";
                result.notes.add ("Lyrics from web: " + best.displayLine()
                                  + "  [match " + juce::String (bestScore, 0) + "]");
                webLyricsDone = true;
                afterWebAssets();
                return;
            }

            // Fetch by id if search payload lacked full lyric text.
            if (best.id > 0)
            {
                reportProgress (0.50f, "Downloading lyrics...");
                onlineLyricsClient.fetchByIdAsync (best.id,
                    [this, job] (jamstudio::notation::LyricsTrack fetched, juce::String fetchError)
                    {
                        if (! isStillActive (job))
                            return;

                        if (fetchError.isEmpty() && ! fetched.isEmpty())
                        {
                            result.lyrics = std::move (fetched);
                            result.lyricsSource = "web";
                            result.notes.add ("Lyrics downloaded from lrclib.");
                        }
                        else
                        {
                            result.notes.add (fetchError.isNotEmpty() ? fetchError
                                                                      : "Could not download lyrics.");
                        }

                        webLyricsDone = true;
                        afterWebAssets();
                    });
                return;
            }

            result.notes.add (lyricsError.isNotEmpty() ? lyricsError : "Could not parse online lyrics.");
            webLyricsDone = true;
            afterWebAssets();
        });
}

void PracticeSetupPipeline::afterWebAssets()
{
    if (! webTabsDone || ! webLyricsDone)
        return;

    if (shouldCancel.load())
    {
        finishCancelled();
        return;
    }

    runAiLyricsIfNeeded();
}

void PracticeSetupPipeline::runAiLyricsIfNeeded()
{
    const auto job = generation.load();

    if (! result.lyrics.isEmpty())
    {
        runAiTabsIfNeeded();
        return;
    }

    if (! whisperTranscriber.isAvailable())
    {
        result.notes.add ("Whisper unavailable — lyrics left empty.");
        result.lyricsSource = "none";
        runAiTabsIfNeeded();
        return;
    }

    auto vocals = findStemByType (result.stemFiles, jamstudio::audio::StemType::vocals);

    if (! vocals.existsAsFile())
        vocals = result.songFile;

    if (! vocals.existsAsFile())
    {
        result.notes.add ("No vocals stem for AI lyrics.");
        result.lyricsSource = "none";
        runAiTabsIfNeeded();
        return;
    }

    reportProgress (0.55f, "AI lyrics (Whisper) — web lookup failed...");

    whisperTranscriber.transcribeAsync (vocals,
        [this, job] (const jamstudio::ai::TranscriptionResult& transcription)
        {
            if (! isStillActive (job))
                return;

            if (transcription.success && ! transcription.lyrics.isEmpty())
            {
                result.lyrics = transcription.lyrics;
                result.lyricsSource = "ai";
                result.notes.add ("Lyrics generated with Whisper.");
            }
            else
            {
                result.lyricsSource = "none";
                result.notes.add (transcription.errorMessage.isNotEmpty()
                                      ? transcription.errorMessage
                                      : "AI lyrics failed.");
            }

            runAiTabsIfNeeded();
        },
        [this, job] (const float progress, const juce::String& message)
        {
            if (! isStillActive (job))
                return;

            reportProgress (0.55f + progress * 0.15f, message);
        });
}

void PracticeSetupPipeline::runAiTabsIfNeeded()
{
    if (shouldCancel.load())
    {
        finishCancelled();
        return;
    }

    if (! result.score.isEmpty())
    {
        reportProgress (0.98f, "Finishing practice project...");
        finishSuccess();
        return;
    }

    if (! basicPitchTranscriber.isAvailable())
    {
        result.notes.add ("basic-pitch unavailable — tabs left empty.");
        result.scoreSource = "none";
        reportProgress (0.98f, "Finishing practice project...");
        finishSuccess();
        return;
    }

    stemsToTranscribe = melodicStems (result.stemFiles);

    if (stemsToTranscribe.isEmpty())
    {
        // Fall back to full mix if demucs only produced drums / unknown.
        if (result.songFile.existsAsFile())
            stemsToTranscribe.add (result.songFile);
    }

    if (stemsToTranscribe.isEmpty())
    {
        result.notes.add ("No melodic stems for AI tabs.");
        result.scoreSource = "none";
        finishSuccess();
        return;
    }

    nextStemIndex = 0;
    result.score.clear();
    result.score.setTitle (result.projectTitle);
    reportProgress (0.72f, "AI tabs for instruments (basic-pitch)...");
    transcribeNextStem();
}

void PracticeSetupPipeline::transcribeNextStem()
{
    const auto job = generation.load();

    if (shouldCancel.load())
    {
        finishCancelled();
        return;
    }

    if (nextStemIndex >= stemsToTranscribe.size())
    {
        if (result.score.isEmpty())
        {
            result.scoreSource = "none";
            result.notes.add ("AI tab transcription produced no notes.");
        }
        else
        {
            result.scoreSource = "ai";
            result.notes.add ("Tabs generated with basic-pitch for "
                              + juce::String (result.score.getNumParts()) + " part(s).");
        }

        reportProgress (0.98f, "Finishing practice project...");
        finishSuccess();
        return;
    }

    const auto stemFile = stemsToTranscribe.getReference (nextStemIndex);
    const auto partName = stemDisplayName (stemFile);
    const auto stemProgressBase = 0.72f
        + (0.24f * static_cast<float> (nextStemIndex)
           / static_cast<float> (juce::jmax (1, stemsToTranscribe.size())));

    reportProgress (stemProgressBase, "AI tab: " + partName + "...");

    basicPitchTranscriber.transcribeAsync (stemFile,
        [this, job, partName] (const jamstudio::ai::PitchTranscriptionResult& pitchResult)
        {
            if (! isStillActive (job))
                return;

            if (pitchResult.success && ! pitchResult.score.isEmpty())
                mergeScorePart (pitchResult.score, partName);
            else if (pitchResult.errorMessage.isNotEmpty()
                     && ! pitchResult.errorMessage.containsIgnoreCase ("cancel"))
                result.notes.add (partName + ": " + pitchResult.errorMessage);

            ++nextStemIndex;
            transcribeNextStem();
        },
        [this, job, stemProgressBase, partName] (const float progress, const juce::String& message)
        {
            if (! isStillActive (job))
                return;

            const auto span = 0.24f / static_cast<float> (juce::jmax (1, stemsToTranscribe.size()));
            reportProgress (stemProgressBase + progress * span,
                            message.isNotEmpty() ? (partName + ": " + message) : ("AI tab: " + partName));
        });
}

void PracticeSetupPipeline::mergeScorePart (const jamstudio::notation::Score& partScore,
                                            const juce::String& partName)
{
    if (result.score.getTempo() <= 0.0 && partScore.getTempo() > 0.0)
        result.score.setTempo (partScore.getTempo());

    if (result.score.getTitle().isEmpty())
        result.score.setTitle (partScore.getTitle().isNotEmpty() ? partScore.getTitle()
                                                                : result.projectTitle);

    if (partScore.getNumParts() == 0)
        return;

    // Prefer a single named part per stem; fall back to merging all parts with a suffix.
    for (int i = 0; i < partScore.getNumParts(); ++i)
    {
        if (const auto* src = partScore.getPart (i))
        {
            jamstudio::notation::ScorePart copy = *src;

            if (partScore.getNumParts() == 1)
                copy.name = partName;
            else
                copy.name = partName + " — "
                            + (src->name.isNotEmpty() ? src->name : ("Part " + juce::String (i + 1)));

            if (copy.notationMode == jamstudio::notation::NotationMode::hidden
                || copy.notationMode == jamstudio::notation::NotationMode::standard)
                copy.notationMode = jamstudio::notation::NotationMode::tab;

            result.score.addPart (std::move (copy));
        }
    }

    result.score.setNotationMode (jamstudio::notation::NotationMode::tab);
}

juce::File PracticeSetupPipeline::findStemByType (const juce::Array<juce::File>& stems,
                                                  const jamstudio::audio::StemType type)
{
    for (const auto& file : stems)
    {
        if (jamstudio::audio::stemTypeFromFileName (file.getFileName()) == type)
            return file;
    }

    return {};
}

juce::Array<juce::File> PracticeSetupPipeline::melodicStems (const juce::Array<juce::File>& stems)
{
    juce::Array<juce::File> melodic;

    // Guitar-first practice order, then other melodic instruments.
    const jamstudio::audio::StemType order[] = {
        jamstudio::audio::StemType::guitar,
        jamstudio::audio::StemType::bass,
        jamstudio::audio::StemType::piano,
        jamstudio::audio::StemType::other,
        jamstudio::audio::StemType::vocals
    };

    for (const auto type : order)
    {
        if (const auto file = findStemByType (stems, type); file.existsAsFile())
            melodic.addIfNotAlreadyThere (file);
    }

    return melodic;
}

juce::String PracticeSetupPipeline::sanitizeProjectTitle (const juce::String& raw)
{
    auto title = raw.trim();

    if (title.isEmpty())
        title = "Untitled";

    // Safe file-name characters for the project.
    title = title.replaceCharacter ('/', '-')
                 .replaceCharacter ('\\', '-')
                 .replaceCharacter (':', '-')
                 .replaceCharacter ('*', '-')
                 .replaceCharacter ('?', '-')
                 .replaceCharacter ('"', '\'')
                 .replaceCharacter ('<', '-')
                 .replaceCharacter ('>', '-')
                 .replaceCharacter ('|', '-');

    while (title.contains ("  "))
        title = title.replace ("  ", " ");

    return title.trim();
}

} // namespace jamstudio::app
