#include "AmpModelLibrary.h"

namespace jamstudio::amp
{

juce::File AmpModelLibrary::getUserAmpModelsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("JamStudio")
        .getChildFile ("AmpModels");
}

juce::File AmpModelLibrary::getBundledExamplesDirectory()
{
   #ifdef JAMSTUDIO_SOURCE_DIR
    const auto fromSource = juce::File (JAMSTUDIO_SOURCE_DIR)
                                .getChildFile ("third_party/NeuralAmpModelerCore/example_models");
    if (fromSource.isDirectory())
        return fromSource;
   #endif

    auto cwd = juce::File::getCurrentWorkingDirectory()
                   .getChildFile ("third_party/NeuralAmpModelerCore/example_models");
    if (cwd.isDirectory())
        return cwd;

    return juce::File::getCurrentWorkingDirectory()
        .getChildFile ("../third_party/NeuralAmpModelerCore/example_models");
}

void AmpModelLibrary::ensureUserLibrary (const bool seedBundledExamples)
{
    const auto userDir = getUserAmpModelsDirectory();
    userDir.createDirectory();

    if (! seedBundledExamples)
        return;

    const auto bundled = getBundledExamplesDirectory();
    if (! bundled.isDirectory())
        return;

    // Seed only when the library is empty so we never clobber user files.
    if (userDir.findChildFiles (juce::File::findFiles, false, "*.nam").isEmpty())
    {
        for (const auto& f : bundled.findChildFiles (juce::File::findFiles, false, "*.nam"))
            f.copyFileTo (userDir.getChildFile (f.getFileName()));
    }
}

void AmpModelLibrary::addDirectory (const juce::File& dir, const juce::String& sourceLabel)
{
    if (! dir.isDirectory())
        return;

    for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.nam"))
    {
        AmpToneInfo info;
        info.displayName = f.getFileNameWithoutExtension();
        info.file = f;
        info.sourceLabel = sourceLabel;
        info.fileSizeBytes = f.getSize();
        tones.add (std::move (info));
    }
}

void AmpModelLibrary::rescan()
{
    tones.clearQuick();
    ensureUserLibrary (true);

    addDirectory (getUserAmpModelsDirectory(), "My Amps");

    const auto bundled = getBundledExamplesDirectory();
    // Avoid listing the same path twice if user lib was seeded from bundled path only
    if (bundled.isDirectory()
        && bundled.getFullPathName() != getUserAmpModelsDirectory().getFullPathName())
    {
        addDirectory (bundled, "Bundled");
    }

    // Prefer user tones, then name A-Z
    struct Less
    {
        int compareElements (const AmpToneInfo& a, const AmpToneInfo& b) const
        {
            const int source = a.sourceLabel.compareIgnoreCase (b.sourceLabel);
            // "My Amps" before "Bundled"
            if (a.sourceLabel == "My Amps" && b.sourceLabel != "My Amps")
                return -1;
            if (b.sourceLabel == "My Amps" && a.sourceLabel != "My Amps")
                return 1;
            if (source != 0)
                return source;
            return a.displayName.compareIgnoreCase (b.displayName);
        }
    } sorter;

    tones.sort (sorter);
}

juce::Array<AmpToneInfo> AmpModelLibrary::filter (const juce::String& query) const
{
    if (query.trim().isEmpty())
        return tones;

    juce::Array<AmpToneInfo> out;
    const auto q = query.trim().toLowerCase();

    for (const auto& t : tones)
        if (t.displayName.toLowerCase().contains (q)
            || t.sourceLabel.toLowerCase().contains (q)
            || t.file.getFileName().toLowerCase().contains (q))
            out.add (t);

    return out;
}

juce::File AmpModelLibrary::importIntoUserLibrary (const juce::File& sourceNam, const bool overwrite)
{
    ensureUserLibrary (false);

    if (! sourceNam.existsAsFile() || ! sourceNam.hasFileExtension (".nam"))
        return {};

    auto dest = getUserAmpModelsDirectory().getChildFile (sourceNam.getFileName());

    if (dest.existsAsFile() && ! overwrite)
    {
        // Unique name
        int n = 2;
        while (dest.existsAsFile())
        {
            dest = getUserAmpModelsDirectory().getChildFile (
                sourceNam.getFileNameWithoutExtension() + "-" + juce::String (n) + ".nam");
            ++n;
        }
    }

    if (! sourceNam.copyFileTo (dest))
        return {};

    return dest;
}

} // namespace jamstudio::amp
