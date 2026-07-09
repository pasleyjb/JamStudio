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

    [[nodiscard]] static bool saveSetList (const juce::File& file, const SetList& list);
    [[nodiscard]] static bool loadSetList (const juce::File& file, SetList& list, juce::String& error);

    [[nodiscard]] static juce::File defaultSetListFile();
};

/** Apply name-matched stem prefs to a loaded mixer. */
void applyStemPrefs (jamstudio::audio::StemMixer& mixer,
                     const juce::Array<StemMixPref>& prefs);

} // namespace jamstudio::performance
