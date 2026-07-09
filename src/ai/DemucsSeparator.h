#pragma once

#include "IStemSeparator.h"

#include <mutex>

namespace jamstudio::ai
{

/** Runs Demucs as an external subprocess to separate a song into stems.
    Defaults to htdemucs_6s so guitar and piano get dedicated stems
    (better for guitar practice than the 4-stem "other" bucket). */
class DemucsSeparator : public IStemSeparator
{
public:
    DemucsSeparator();
    ~DemucsSeparator() override;

    [[nodiscard]] bool isAvailable() const override;

    /** Pretrained model name passed to demucs -n. Default: htdemucs_6s */
    void setModelName (const juce::String& name);
    [[nodiscard]] juce::String getModelName() const;

    /** Random shifts for higher quality (slower). Default: 2 */
    void setShifts (int shifts);
    [[nodiscard]] int getShifts() const;

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
    juce::String modelName { "htdemucs_6s" };
    int shifts { 2 };
    std::atomic<bool> shouldCancel { false };
    std::mutex processMutex;
    juce::ChildProcess* activeProcess = nullptr;
};

} // namespace jamstudio::ai
