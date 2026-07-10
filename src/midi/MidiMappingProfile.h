#pragma once

#include <JuceHeader.h>

namespace jamstudio::midi
{

/** What a MIDI message controls in JamStudio. */
enum class MidiTarget
{
    none = 0,
    masterVolume,
    play,
    pause,
    stop,
    togglePlayPause,
    recordToggle,
    metronomeToggle,
    /** Performance mode: start next set-list song (foot pedal). */
    nextSong,
    // Stem volumes (faders / knobs)
    stemVolume0,
    stemVolume1,
    stemVolume2,
    stemVolume3,
    stemVolume4,
    stemVolume5,
    stemVolume6,
    stemVolume7,
    // Mute buttons
    stemMute0,
    stemMute1,
    stemMute2,
    stemMute3,
    stemMute4,
    stemMute5,
    stemMute6,
    stemMute7,
    // Solo buttons
    stemSolo0,
    stemSolo1,
    stemSolo2,
    stemSolo3,
    stemSolo4,
    stemSolo5,
    stemSolo6,
    stemSolo7,
};

[[nodiscard]] juce::String midiTargetToString (MidiTarget target);
[[nodiscard]] MidiTarget midiTargetFromString (const juce::String& name);
[[nodiscard]] juce::StringArray allMidiTargetNames();
[[nodiscard]] int stemIndexFromVolumeTarget (MidiTarget target) noexcept;
[[nodiscard]] int stemIndexFromMuteTarget (MidiTarget target) noexcept;
[[nodiscard]] int stemIndexFromSoloTarget (MidiTarget target) noexcept;
[[nodiscard]] MidiTarget stemVolumeTarget (int index) noexcept;
[[nodiscard]] MidiTarget stemMuteTarget (int index) noexcept;
[[nodiscard]] MidiTarget stemSoloTarget (int index) noexcept;

/** One MIDI source -> JamStudio action binding. */
struct MidiBinding
{
    enum class Type
    {
        cc,        // absolute continuous controller 0-127
        note,      // note on/off (buttons)
        pitchBend  // 14-bit fader (Mackie-style when channel encodes track)
    };

    Type type = Type::cc;
    int channel = -1;   // 0-15, or -1 = any channel
    int number = 0;     // CC number, note number, or 0 for pitch bend
    MidiTarget target = MidiTarget::none;
    bool toggle = true; // buttons: toggle state on press (mute/solo/transport)
    bool invert = false;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static MidiBinding fromVar (const juce::var& data);
    [[nodiscard]] juce::String describe() const;
};

/** Named mapping profile (Generic, nanoKONTROL2, ...). */
class MidiMappingProfile
{
public:
    static constexpr int maxStems = 8;

    juce::String id;
    juce::String name;
    juce::String notes;
    juce::Array<MidiBinding> bindings;

    void clear() { bindings.clear(); }
    void addBinding (MidiBinding binding);
    void removeBindingsForTarget (MidiTarget target);
    [[nodiscard]] const MidiBinding* findBinding (MidiBinding::Type type, int channel, int number) const;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static MidiMappingProfile fromVar (const juce::var& data);

    // Built-in profiles covering most USB "mixer" / DAW controllers in CC mode.
    [[nodiscard]] static juce::Array<MidiMappingProfile> builtInProfiles();
    [[nodiscard]] static MidiMappingProfile genericDaw();
    [[nodiscard]] static MidiMappingProfile korgNanoKontrol2();
    [[nodiscard]] static MidiMappingProfile akaiApcMini();
    [[nodiscard]] static MidiMappingProfile novationLaunchControl();
    [[nodiscard]] static MidiMappingProfile behringerXTouchMini();
    [[nodiscard]] static MidiMappingProfile mackieControlLite();
};

/** Persisted MIDI control settings. */
struct MidiControlSettings
{
    bool enabled = true;
    juce::String inputDeviceIdentifier; // empty = all enabled devices
    juce::String profileId { "generic-daw" };
    MidiMappingProfile customProfile; // used when profileId == "custom"
    bool useCustomBindings = false;

    [[nodiscard]] static juce::File settingsFile();
    void load();
    void save() const;
    [[nodiscard]] MidiMappingProfile activeProfile() const;
};

} // namespace jamstudio::midi
