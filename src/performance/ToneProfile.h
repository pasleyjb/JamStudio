#pragma once

#include <JuceHeader.h>
#include <array>

namespace jamstudio::performance
{

/** Live instrument rack slots (2 guitars + bass). */
enum class LiveInstrumentRole
{
    guitar1 = 0,
    guitar2,
    bass,
    count
};

inline constexpr int kNumLiveTonePaths = static_cast<int> (LiveInstrumentRole::count);

[[nodiscard]] juce::String liveInstrumentRoleName (LiveInstrumentRole role);
[[nodiscard]] juce::String liveInstrumentRoleShortName (LiveInstrumentRole role);
[[nodiscard]] juce::Colour liveInstrumentRoleColour (LiveInstrumentRole role);

/**
 * NAM-style amp profile (saved in the tone library).
 * Model path is optional until a full NAM engine is wired; params drive the live DSP now.
 */
struct ToneProfile
{
    juce::String id;          // stable UUID
    juce::String name { "New Tone" };
    LiveInstrumentRole role = LiveInstrumentRole::guitar1;

    /** Optional path to a .nam model file (future real NAM). */
    juce::String namModelPath;
    /** Optional cab / IR path. */
    juce::String cabIrPath;

    float inputGain = 0.55f;   // 0..1
    float drive = 0.45f;       // amp drive / saturation
    float bass = 0.5f;
    float mid = 0.5f;
    float treble = 0.5f;
    float presence = 0.45f;
    float outputLevel = 0.7f;  // 0..1
    bool bypass = false;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static ToneProfile fromVar (const juce::var& data);
    [[nodiscard]] static ToneProfile makeDefault (LiveInstrumentRole role);
};

/** Three profile ids assigned to a setlist song (empty = library default for that role). */
struct SongToneAssignment
{
    juce::String guitar1ProfileId;
    juce::String guitar2ProfileId;
    juce::String bassProfileId;

    [[nodiscard]] juce::String& idFor (LiveInstrumentRole role) noexcept;
    [[nodiscard]] const juce::String& idFor (LiveInstrumentRole role) const noexcept;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static SongToneAssignment fromVar (const juce::var& data);
};

/** Persistent tone library under Documents/JamStudio/Tones/. */
class ToneLibrary
{
public:
    ToneLibrary();

    [[nodiscard]] juce::File getLibraryDirectory() const;
    [[nodiscard]] juce::File getLibraryFile() const;

    bool load();
    bool save() const;

    [[nodiscard]] const juce::Array<ToneProfile>& getProfiles() const noexcept { return profiles; }
    [[nodiscard]] juce::Array<ToneProfile> profilesForRole (LiveInstrumentRole role) const;

    [[nodiscard]] ToneProfile* findById (const juce::String& id);
    [[nodiscard]] const ToneProfile* findById (const juce::String& id) const;

    /** Resolve assignment or fall back to first profile of that role / built-in default. */
    [[nodiscard]] ToneProfile resolve (LiveInstrumentRole role, const juce::String& profileId) const;

    ToneProfile& addProfile (ToneProfile profile);
    bool updateProfile (const ToneProfile& profile);
    bool removeProfile (const juce::String& id);

    /** Ensure each role has at least one default profile. */
    void ensureDefaults();

private:
    juce::Array<ToneProfile> profiles;
};

} // namespace jamstudio::performance
