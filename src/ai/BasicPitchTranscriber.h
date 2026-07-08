#pragma once

#include "../notation/Score.h"

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

    [[nodiscard]] bool isAvailable() const;

    void transcribeAsync (const juce::File& audioFile,
                          std::function<void (PitchTranscriptionResult)> onComplete,
                          PitchTranscriptionProgressCallback onProgress = nullptr);

    void cancel();

private:
    [[nodiscard]] juce::File findMidiFile (const juce::File& outputDirectory) const;

    juce::String basicPitchExecutable;
    std::atomic<bool> shouldCancel { false };
};

} // namespace jamstudio::ai