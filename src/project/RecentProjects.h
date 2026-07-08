#pragma once

#include <JuceHeader.h>

namespace jamstudio::project
{

/** Tracks recently opened JamStudio project files. */
class RecentProjects
{
public:
    static constexpr int maxEntries = 8;

    explicit RecentProjects (const juce::File& storageFile);

    void add (const juce::File& projectFile);
    [[nodiscard]] juce::StringArray getProjectPaths() const;
    void buildMenu (juce::PopupMenu& menu) const;

private:
    void load();
    void save() const;

    juce::File storageFile;
    juce::StringArray entries;
};

} // namespace jamstudio::project