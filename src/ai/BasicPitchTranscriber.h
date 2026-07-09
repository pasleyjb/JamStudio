#pragma once

#include "../notation/Score.h"

#include <mutex>

namespace jamstudio::ai
{

struct PitchTranscriptionResult
{
    bool success = false;
    juce::String errorMessage;
    jamstudio::notation::Score score;
};

using PitchTranscriptionProgressCallback = std::function<void (float progress, const juce::String& message)>;

/** Transcribes audio stems to tab notation using Spotify basic-pitch. */
class BasicPitchTranscriber
{
public:
    BasicPitchTranscriber();
    ~BasicPitchTranscriber();

    [[nodiscard]] bool isAvailable() const;

    void transcribeAsync (const juce::File& audioFile,
                          std::function<void (PitchTranscriptionResult)> onComplete,
                          PitchTranscriptionProgressCallback onProgress = nullptr);

    void cancel();

private:
    [[nodiscard]] juce::File findMidiFile (const juce::File& outputDirectory) const;
    void clearActiveProcess();

    juce::String basicPitchExecutable;
    std::atomic<bool> shouldCancel { false };
    std::mutex processMutex;
    juce::ChildProcess* activeProcess = nullptr;
};

} // namespace jamstudio::ai