#pragma once

#include "../audio/StemMixer.h"
#include "SetListData.h"

namespace jamstudio::performance
{

/** Load / save set lists under Documents/JamStudio/SetLists/. */
class SetListManager
{
public:
    [[nodiscard]] static juce::File getSetListsDirectory();
    [[nodiscard]] static juce::Array<juce::File> listSavedSetLists();
    [[nodiscard]] static juce::Array<juce::File> listAvailableProjects();

    /**
     * Save set list JSON and package stage media into `<name>.media/` next to the file.
     * Paths inside the setlist are rewritten to relative form so reopening loads media.
     */
    [[nodiscard]] static bool saveSetList (const juce::File& file, SetList& list);
    [[nodiscard]] static bool loadSetList (const juce::File& file, SetList& list, juce::String& error);

    [[nodiscard]] static juce::File defaultSetListFile();

    /** Folder next to a setlist that holds packaged stage media. */
    [[nodiscard]] static juce::File mediaFolderForSetList (const juce::File& setListFile);
};

/** Apply name-matched stem prefs (FOH + Mon A/B + mute/solo) to a loaded mixer. */
void applyStemPrefs (jamstudio::audio::StemMixer& mixer,
                     const juce::Array<StemMixPref>& prefs);

/** Snapshot current mixer stem levels into setlist prefs for the playing track. */
[[nodiscard]] juce::Array<StemMixPref> captureStemPrefs (const jamstudio::audio::StemMixer& mixer);

} // namespace jamstudio::performance
