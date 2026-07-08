#pragma once

#include "IStemSeparator.h"

namespace jamstudio::ai
{

/** Runs Demucs as an external subprocess to separate a song into stems. */
class DemucsSeparator : public IStemSeparator
{
public:
    DemucsSeparator();
    ~DemucsSeparator() override;

    [[nodiscard]] bool isAvailable() const override;

    void separateAsync (const juce::File& inputFile,
                        std::function<void (SeparationResult)> onComplete,
                        SeparationProgressCallback onProgress = nullptr) override;

    void cancel() override;

private:
    [[nodiscard]] juce::File getOutputDirectory (const juce::File& inputFile) const;
    [[nodiscard]] juce::Array<juce::File> findStemFiles (const juce::File& outputDirectory) const;
    [[nodiscard]] juce::StringArray buildCommand (const juce::File& inputFile,
                                                  const juce::File& outputDirectory) const;

    juce::String demucsExecutable;
    std::atomic<bool> shouldCancel { false };
};

} // namespace jamstudio::ai