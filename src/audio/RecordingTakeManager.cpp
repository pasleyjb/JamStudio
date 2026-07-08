#include "RecordingTakeManager.h"

namespace jamstudio::audio
{

const RecordingTakeManager::Take* RecordingTakeManager::getTake (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, takes.size()))
        return nullptr;

    return &takes.getReference (index);
}

RecordingTakeManager::AddResult RecordingTakeManager::addTake (const juce::File& file)
{
    AddResult result;
    result.take.file = file;
    result.take.recordedAt = juce::Time::getCurrentTime();
    result.take.displayName = "Take " + juce::String (takes.size() + 1)
                            + " — " + result.take.recordedAt.formatted ("%H:%M:%S");

    takes.add (result.take);

    while (takes.size() > maxTakes)
    {
        result.prunedFiles.add (takes.getFirst().file);
        takes.remove (0);
    }

    for (int i = 0; i < takes.size(); ++i)
    {
        auto& take = takes.getReference (i);
        take.displayName = "Take " + juce::String (i + 1)
                         + " — " + take.recordedAt.formatted ("%H:%M:%S");
    }

    result.take = takes.getLast();
    return result;
}

void RecordingTakeManager::addRestoredTake (const juce::File& file, const juce::String& displayName)
{
    Take take;
    take.file = file;
    take.displayName = displayName;
    take.recordedAt = file.getLastModificationTime();
    takes.add (take);
}

void RecordingTakeManager::clear()
{
    takes.clear();
}

} // namespace jamstudio::audio