#pragma once

#include <JuceHeader.h>

namespace jamstudio::amp
{

/** One .nam tone available for selection. */
struct AmpToneInfo
{
    juce::String displayName;
    juce::File file;
    juce::String sourceLabel; // e.g. "My Amps", "Bundled", "Imported"
    juce::int64 fileSizeBytes = 0;
};

/** Scans user AmpModels folder and bundled example models. */
class AmpModelLibrary
{
public:
    [[nodiscard]] static juce::File getUserAmpModelsDirectory();
    [[nodiscard]] static juce::File getBundledExamplesDirectory();

    /** Ensure Documents/JamStudio/AmpModels exists; optionally seed from examples. */
    static void ensureUserLibrary (bool seedBundledExamples = true);

    /** Rescan all known roots. Newest / user models first. */
    void rescan();

    [[nodiscard]] const juce::Array<AmpToneInfo>& getTones() const noexcept { return tones; }
    [[nodiscard]] juce::Array<AmpToneInfo> filter (const juce::String& query) const;

    /** Copy a .nam into the user library (does not overwrite without force). */
    [[nodiscard]] static juce::File importIntoUserLibrary (const juce::File& sourceNam,
                                                          bool overwrite = false);

private:
    void addDirectory (const juce::File& dir, const juce::String& sourceLabel);
    juce::Array<AmpToneInfo> tones;
};

} // namespace jamstudio::amp
