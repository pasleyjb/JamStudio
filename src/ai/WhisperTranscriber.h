#pragma once

#include "../notation/LyricsTrack.h"

#include <mutex>

namespace jamstudio::ai
{

struct TranscriptionResult
{
    bool success = false;
    juce::String errorMessage;
    jamstudio::notation::LyricsTrack lyrics;
};

using TranscriptionProgressCallback = std::function<void (float progress, const juce::String& message)>;

/** Transcribes vocals to timed lyrics using OpenAI Whisper. */
class WhisperTranscriber
{
public:
    WhisperTranscriber();
    ~WhisperTranscriber();

    [[nodiscard]] bool isAvailable() const;

    void transcribeAsync (const juce::File& audioFile,
                          std::function<void (TranscriptionResult)> onComplete,
                          TranscriptionProgressCallback onProgress = nullptr);

    /** Requests cancel and immediately kills the running Whisper process if any. */
    void cancel();

private:
    [[nodiscard]] bool parseWhisperJson (const juce::File& jsonFile,
                                         jamstudio::notation::LyricsTrack& lyrics,
                                         juce::String& error) const;

    void clearActiveProcess();

    juce::String whisperExecutable;
    std::atomic<bool> shouldCancel { false };
    std::mutex processMutex;
    juce::ChildProcess* activeProcess = nullptr;
};

} // namespace jamstudio::ai
