#include "RecentProjects.h"

namespace jamstudio::project
{

RecentProjects::RecentProjects (const juce::File& file)
    : storageFile (file)
{
    load();
}

void RecentProjects::add (const juce::File& projectFile)
{
    if (! projectFile.existsAsFile())
        return;

    const auto path = projectFile.getFullPathName();
    entries.removeString (path);
    entries.insert (0, path);

    while (entries.size() > maxEntries)
        entries.remove (entries.size() - 1);

    save();
}

juce::StringArray RecentProjects::getProjectPaths() const
{
    return entries;
}

void RecentProjects::buildMenu (juce::PopupMenu& menu) const
{
    menu.clear();

    for (int i = 0; i < entries.size(); ++i)
    {
        const juce::File file (entries[i]);
        menu.addItem (i + 1, file.getFileName() + "  —  " + file.getParentDirectory().getFileName());
    }

    if (entries.isEmpty())
        menu.addItem (1, "(No recent projects)", false);
}

void RecentProjects::load()
{
    entries.clear();

    if (! storageFile.existsAsFile())
        return;

    juce::var parsed;

    if (juce::JSON::parse (storageFile.loadFileAsString(), parsed).failed())
        return;

    if (const auto* array = parsed.getArray())
    {
        for (const auto& item : *array)
            entries.add (item.toString());
    }
}

void RecentProjects::save() const
{
    juce::Array<juce::var> array;

    for (const auto& entry : entries)
        array.add (entry);

    storageFile.getParentDirectory().createDirectory();
    storageFile.replaceWithText (juce::JSON::toString (juce::var (array), true));
}

} // namespace jamstudio::project