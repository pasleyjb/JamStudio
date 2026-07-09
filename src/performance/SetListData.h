#pragma once

#include <JuceHeader.h>

namespace jamstudio::performance
{

/** Mixer preference for one stem role (matched by name, e.g. "Guitar"). */
struct StemMixPref
{
    juce::String stemName; // "Guitar", "Vocals", "Drums", "Bass", …
    float volume = 0.8f;
    bool muted = false;
    bool solo = false;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static StemMixPref fromVar (const juce::var& data);
};

/** One song in a live set — links a .jamstudio project + stage mix prefs. */
struct SetListSong
{
    juce::String projectPath;
    juce::String displayName;
    juce::Array<StemMixPref> stemPrefs;
    bool showTabs = true;
    bool showLyrics = true;
    /** Prefer a score part name containing this (e.g. "Guitar") when loading tabs. */
    juce::String preferredPartHint { "Guitar" };

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static SetListSong fromVar (const juce::var& data);
    [[nodiscard]] juce::File projectFile() const { return juce::File (projectPath); }
};

/** Ordered set list for Performance mode. */
struct SetList
{
    juce::String name { "My Set" };
    juce::Array<SetListSong> songs;
    /** Applied to any song that has empty stemPrefs. */
    juce::Array<StemMixPref> defaultStemPrefs;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static SetList fromVar (const juce::var& data);
    [[nodiscard]] bool isEmpty() const noexcept { return songs.isEmpty(); }

    /** Lead guitar + vocalist defaults: guitar down, vocals off, rhythm up. */
    [[nodiscard]] static juce::Array<StemMixPref> leadGuitarSingerDefaults();
};

} // namespace jamstudio::performance
