#pragma once

#include "../ai/BasicPitchTranscriber.h"
#include "../ai/DemucsSeparator.h"
#include "../ai/WhisperTranscriber.h"
#include "../audio/StemType.h"
#include "../notation/LyricsTrack.h"
#include "../notation/OnlineLyricsClient.h"
#include "../notation/Score.h"
#include "../notation/SongMetadata.h"
#include "../notation/TabLibraryClient.h"

#include <atomic>
#include <functional>

namespace jamstudio::app
{

/** Result of the automatic Practice song setup pipeline. */
struct PracticeSetupResult
{
    bool success = false;
    bool cancelled = false;
    juce::String errorMessage;
    juce::File songFile;
    juce::String projectTitle;
    juce::Array<juce::File> stemFiles;
    jamstudio::notation::Score score;
    jamstudio::notation::LyricsTrack lyrics;
    juce::String scoreSource;   // "web" | "ai" | "none"
    juce::String lyricsSource;  // "web" | "ai" | "none"
    juce::StringArray notes;    // non-fatal status notes for the UI
};

/**
 * Background practice setup:
 *  1) Stem-separate the song (Demucs)
 *  2) Download tabs + lyrics from the web when possible
 *  3) Fall back to AI (basic-pitch per instrument, Whisper for lyrics)
 *
 * Does not touch the UI mixer - caller applies the result and saves the project.
 */
class PracticeSetupPipeline
{
public:
    using ProgressCallback = std::function<void (float progress, const juce::String& message)>;
    using CompleteCallback = std::function<void (PracticeSetupResult)>;

    PracticeSetupPipeline (jamstudio::ai::DemucsSeparator& demucs,
                           jamstudio::ai::WhisperTranscriber& whisper,
                           jamstudio::ai::BasicPitchTranscriber& basicPitch,
                           juce::AudioFormatManager& formatManager);

    void start (const juce::File& songFile,
                ProgressCallback onProgress,
                CompleteCallback onComplete);

    void cancel();

    [[nodiscard]] bool isRunning() const noexcept { return running.load(); }

private:
    void reportProgress (float progress, const juce::String& message);
    void finishSuccess();
    void finishFailure (const juce::String& error);
    void finishCancelled();
    [[nodiscard]] bool isStillActive (uint32_t generation) const noexcept;

    void afterStems (const jamstudio::ai::SeparationResult& separation);
    void fetchWebTabs();
    void fetchWebLyrics();
    void afterWebAssets();
    void runAiLyricsIfNeeded();
    void runAiTabsIfNeeded();
    void transcribeNextStem();
    void mergeScorePart (const jamstudio::notation::Score& partScore, const juce::String& partName);

    [[nodiscard]] static juce::File findStemByType (const juce::Array<juce::File>& stems,
                                                   jamstudio::audio::StemType type);
    [[nodiscard]] static juce::Array<juce::File> melodicStems (const juce::Array<juce::File>& stems);
    [[nodiscard]] static juce::String sanitizeProjectTitle (const juce::String& raw);

    jamstudio::ai::DemucsSeparator& demucsSeparator;
    jamstudio::ai::WhisperTranscriber& whisperTranscriber;
    jamstudio::ai::BasicPitchTranscriber& basicPitchTranscriber;
    juce::AudioFormatManager& formatManager;

    jamstudio::notation::OnlineLyricsClient onlineLyricsClient;
    jamstudio::notation::TabLibraryClient tabLibraryClient;

    ProgressCallback progressCallback;
    CompleteCallback completeCallback;

    PracticeSetupResult result;
    jamstudio::notation::SongMetadata metadata;

    juce::Array<juce::File> stemsToTranscribe;
    int nextStemIndex = 0;

    std::atomic<bool> running { false };
    std::atomic<bool> shouldCancel { false };
    std::atomic<uint32_t> generation { 0 };

    bool webTabsAttempted = false;
    bool webLyricsAttempted = false;
    bool webTabsDone = false;
    bool webLyricsDone = false;
};

} // namespace jamstudio::app
