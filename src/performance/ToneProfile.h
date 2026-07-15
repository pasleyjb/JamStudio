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
 * Amp profile saved in the tone library.
 * When namModelPath points to a real .nam, LiveToneEngine runs Neural Amp Modeler;
 * otherwise the built-in amp sim is used with the same knobs.
 */
struct ToneProfile
{
    juce::String id;          // stable UUID
    juce::String name { "New Tone" };
    LiveInstrumentRole role = LiveInstrumentRole::guitar1;

    /** Absolute or user-library path to a .nam model file. */
    juce::String namModelPath;
    /** Optional cab / IR path under CabIRs/. */
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

/**
 * Performance tone library + model folders:
 *
 *   Documents/JamStudio/
 *     Tones/tone-library.json     profile knobs + model refs
 *     AmpModels/guitar/           G1/G2 .nam imports
 *     AmpModels/bass/             Bass .nam imports
 *     AmpModels/shared/           any-role models
 *     CabIRs/                     optional cab IRs
 */
class ToneLibrary
{
public:
    ToneLibrary();

    [[nodiscard]] static juce::File getJamStudioRoot();
    [[nodiscard]] juce::File getLibraryDirectory() const;
    [[nodiscard]] juce::File getLibraryFile() const;
    [[nodiscard]] juce::File getAmpModelsRoot() const;
    [[nodiscard]] juce::File getAmpModelsDirectoryForRole (LiveInstrumentRole role) const;
    [[nodiscard]] juce::File getSharedAmpModelsDirectory() const;
    [[nodiscard]] juce::File getCabIrsDirectory() const;

    /** Create standard folders; optionally seed example .nam into shared/. */
    void ensureDirectories (bool seedExampleModels = true) const;

    /** Copy a .nam into AmpModels/{role|shared}; returns destination or invalid. */
    [[nodiscard]] juce::File importNamModel (const juce::File& sourceNam,
                                             LiveInstrumentRole role,
                                             bool useSharedFolder = false) const;

    /** Scan AmpModels for .nam files (role folder + shared). */
    [[nodiscard]] juce::Array<juce::File> listNamModels (LiveInstrumentRole role) const;

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
