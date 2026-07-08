#pragma once

#include "BasicPitchTranscriber.h"
#include "DemucsSeparator.h"
#include "WhisperTranscriber.h"

namespace jamstudio::ai
{

struct AiToolInfo
{
    juce::String id;
    juce::String name;
    juce::String purpose;
    bool available = false;
    juce::String installCommand;
    juce::String notes;
};

/** Describes optional AI subprocess dependencies and how to install them. */
class AiToolsCatalog
{
public:
    [[nodiscard]] static juce::Array<AiToolInfo> getToolStatuses (const DemucsSeparator& demucs,
                                                                    const WhisperTranscriber& whisper,
                                                                    const BasicPitchTranscriber& basicPitch);

    [[nodiscard]] static juce::String buildSetupMessage (const juce::Array<AiToolInfo>& tools);
    [[nodiscard]] static juce::String buildUnavailableHint (const juce::String& toolId,
                                                            const juce::Array<AiToolInfo>& tools);
};

} // namespace jamstudio::ai