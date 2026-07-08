#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/** Tracks recording takes for a project, naming them and pruning beyond a limit. */
class RecordingTakeManager
{
public:
    static constexpr int maxTakes = 8;

    struct Take
    {
        juce::File file;
        juce::String displayName;
        juce::Time recordedAt;
    };

    struct AddResult
    {
        Take take;
        juce::Array<juce::File> prunedFiles;
    };

    [[nodiscard]] int getNumTakes() const noexcept { return takes.size(); }
    [[nodiscard]] const Take* getTake (int index) const noexcept;

    AddResult addTake (const juce::File& file);
    void addRestoredTake (const juce::File& file, const juce::String& displayName);
    void clear();

private:
    juce::Array<Take> takes;
};

} // namespace jamstudio::audio