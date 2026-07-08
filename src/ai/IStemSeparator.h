#pragma once

#include <JuceHeader.h>

namespace jamstudio::ai
{

struct SeparationResult
{
    bool success = false;
    juce::String errorMessage;
    juce::Array<juce::File> stemFiles;
};

using SeparationProgressCallback = std::function<void (float progress, const juce::String& statusMessage)>;

/** Interface for splitting a mixed song into instrument stems. */
class IStemSeparator
{
public:
    virtual ~IStemSeparator() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;

    virtual void separateAsync (const juce::File& inputFile,
                                std::function<void (SeparationResult)> onComplete,
                                SeparationProgressCallback onProgress = nullptr) = 0;

    virtual void cancel() = 0;
};

} // namespace jamstudio::ai