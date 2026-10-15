#include "HostEnvironment.h"

#include <JuceHeader.h>

#if JUCE_MAC
 #include <cstdlib>
#endif

namespace jamstudio::platform
{

namespace
{
#if JUCE_MAC
void addIfDirectory (juce::StringArray& paths, const juce::File& dir)
{
    if (dir.isDirectory())
        paths.addIfNotAlreadyThere (dir.getFullPathName());
}
#endif
} // namespace

void prepareHostEnvironment()
{
   #if JUCE_MAC
    juce::StringArray prefixes;
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    addIfDirectory (prefixes, juce::File ("/opt/homebrew/bin"));
    addIfDirectory (prefixes, juce::File ("/usr/local/bin"));
    addIfDirectory (prefixes, home.getChildFile (".local/bin"));

    for (const char* optRoot : { "/opt/homebrew/opt", "/usr/local/opt" })
    {
        const juce::File opt (optRoot);
        if (! opt.isDirectory())
            continue;

        for (const auto& child : opt.findChildFiles (juce::File::findDirectories, false, "python@*"))
            addIfDirectory (prefixes, child.getChildFile ("bin"));
    }

    const auto pythonUser = home.getChildFile ("Library/Python");
    if (pythonUser.isDirectory())
    {
        for (const auto& version : pythonUser.findChildFiles (juce::File::findDirectories, false, "3.*"))
            addIfDirectory (prefixes, version.getChildFile ("bin"));
    }

    const auto current = juce::String::fromUTF8 (std::getenv ("PATH") != nullptr ? std::getenv ("PATH") : "");
    juce::StringArray existing;
    existing.addTokens (current, ":", "");

    for (const auto& part : existing)
        if (part.isNotEmpty())
            prefixes.addIfNotAlreadyThere (part);

    ::setenv ("PATH", prefixes.joinIntoString (":").toRawUTF8(), 1);
   #endif
}

} // namespace jamstudio::platform
