#pragma once

#include "LyricsTrack.h"
#include "Score.h"

namespace jamstudio::notation
{

/** Helpers for reviewing and applying AI transcription corrections. */
class TranscriptionEditor
{
public:
    static void applyLyricLineTexts (LyricsTrack& track,
                                     const juce::String& title,
                                     const juce::StringArray& lineTexts);

    [[nodiscard]] static juce::String scoreToEditableTabText (const Score& score);
    [[nodiscard]] static bool scoreFromEditableTabText (const juce::String& text,
                                                        const Score& original,
                                                        Score& out,
                                                        juce::String& errorMessage);
};

} // namespace jamstudio::notation