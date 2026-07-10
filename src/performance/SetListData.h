#pragma once

#include <JuceHeader.h>

namespace jamstudio::performance
{

/** Mixer preference for one stem role (matched by name, e.g. "Guitar"). */
struct StemMixPref
{
    juce::String stemName; // "Guitar", "Vocals", "Drums", "Bass", ...
    float volume = 0.8f;   // FOH send
    float monA = 0.7f;     // Monitor / IEM A send
    float monB = 0.0f;     // Monitor / IEM B send
    bool muted = false;
    bool solo = false;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static StemMixPref fromVar (const juce::var& data);
};

/** Stage-board media pinned to a set-list song (video file or image slideshow). */
enum class StageMediaKind
{
    none = 0,
    video,
    slideshow
};

[[nodiscard]] juce::String stageMediaKindToString (StageMediaKind kind);
[[nodiscard]] StageMediaKind stageMediaKindFromString (const juce::String& s);

/** One song in a live set - links a .jamstudio project + stage mix + stage media. */
struct SetListSong
{
    juce::String projectPath;
    juce::String displayName;
    juce::Array<StemMixPref> stemPrefs;
    bool showTabs = true;
    bool showLyrics = true;
    /** Prefer a score part name containing this (e.g. "Guitar") when loading tabs. */
    juce::String preferredPartHint { "Guitar" };

    /** Pinned stage show media (paths may be relative to the .setlist file). */
    StageMediaKind stageMediaKind = StageMediaKind::none;
    juce::String stageMediaPath;                 // video file, or primary slideshow folder/file
    juce::Array<juce::String> stageSlidePaths;   // explicit slide image paths (slideshow)
    bool stageMediaAutoPlay = true;              // start with the song
    float stageSlideSeconds = 5.0f;              // per-slide duration for slideshows

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static SetListSong fromVar (const juce::var& data);
    [[nodiscard]] juce::File projectFile() const { return juce::File (projectPath); }

    /** Resolve stage media against the setlist file's directory. */
    [[nodiscard]] juce::File resolveStageMediaFile (const juce::File& setListFile) const;
    [[nodiscard]] juce::Array<juce::File> resolveSlideFiles (const juce::File& setListFile) const;
    [[nodiscard]] bool hasStageMedia() const noexcept
    {
        return stageMediaKind != StageMediaKind::none
               && (stageMediaPath.isNotEmpty() || ! stageSlidePaths.isEmpty());
    }
};

/** Ordered set list for Performance / Stage Show Builder. */
struct SetList
{
    juce::String name { "My Set" };
    juce::Array<SetListSong> songs;
    /** Applied to any song that has empty stemPrefs. */
    juce::Array<StemMixPref> defaultStemPrefs;
    /** Absolute path of the .setlist file this was loaded from / last saved to. */
    juce::String sourceFilePath;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static SetList fromVar (const juce::var& data);
    [[nodiscard]] bool isEmpty() const noexcept { return songs.isEmpty(); }
    [[nodiscard]] juce::File sourceFile() const { return juce::File (sourceFilePath); }

    /** Lead guitar + vocalist defaults: guitar down, vocals off, rhythm up. */
    [[nodiscard]] static juce::Array<StemMixPref> leadGuitarSingerDefaults();
};

} // namespace jamstudio::performance
