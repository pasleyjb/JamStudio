#include "MidiMappingProfile.h"

namespace jamstudio::midi
{

namespace
{
juce::String typeToString (MidiBinding::Type type)
{
    switch (type)
    {
        case MidiBinding::Type::cc: return "cc";
        case MidiBinding::Type::note: return "note";
        case MidiBinding::Type::pitchBend: return "pitchBend";
    }
    return "cc";
}

MidiBinding::Type typeFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("note")) return MidiBinding::Type::note;
    if (s.equalsIgnoreCase ("pitchBend")) return MidiBinding::Type::pitchBend;
    return MidiBinding::Type::cc;
}

MidiBinding makeCc (int cc, MidiTarget target, int channel = 0, bool toggle = false)
{
    MidiBinding b;
    b.type = MidiBinding::Type::cc;
    b.channel = channel;
    b.number = cc;
    b.target = target;
    b.toggle = toggle;
    return b;
}

MidiBinding makeNote (int note, MidiTarget target, int channel = 0, bool toggle = true)
{
    MidiBinding b;
    b.type = MidiBinding::Type::note;
    b.channel = channel;
    b.number = note;
    b.target = target;
    b.toggle = toggle;
    return b;
}
} // namespace

juce::String midiTargetToString (const MidiTarget target)
{
    switch (target)
    {
        case MidiTarget::none: return "None";
        case MidiTarget::masterVolume: return "Master Volume";
        case MidiTarget::play: return "Play";
        case MidiTarget::pause: return "Pause";
        case MidiTarget::stop: return "Stop";
        case MidiTarget::togglePlayPause: return "Play/Pause Toggle";
        case MidiTarget::recordToggle: return "Record Toggle";
        case MidiTarget::metronomeToggle: return "Metronome Toggle";
        case MidiTarget::nextSong: return "Next Song / Foot Pedal";
        case MidiTarget::stemVolume0: return "Stem 1 Volume";
        case MidiTarget::stemVolume1: return "Stem 2 Volume";
        case MidiTarget::stemVolume2: return "Stem 3 Volume";
        case MidiTarget::stemVolume3: return "Stem 4 Volume";
        case MidiTarget::stemVolume4: return "Stem 5 Volume";
        case MidiTarget::stemVolume5: return "Stem 6 Volume";
        case MidiTarget::stemVolume6: return "Stem 7 Volume";
        case MidiTarget::stemVolume7: return "Stem 8 Volume";
        case MidiTarget::stemMute0: return "Stem 1 Mute";
        case MidiTarget::stemMute1: return "Stem 2 Mute";
        case MidiTarget::stemMute2: return "Stem 3 Mute";
        case MidiTarget::stemMute3: return "Stem 4 Mute";
        case MidiTarget::stemMute4: return "Stem 5 Mute";
        case MidiTarget::stemMute5: return "Stem 6 Mute";
        case MidiTarget::stemMute6: return "Stem 7 Mute";
        case MidiTarget::stemMute7: return "Stem 8 Mute";
        case MidiTarget::stemSolo0: return "Stem 1 Solo";
        case MidiTarget::stemSolo1: return "Stem 2 Solo";
        case MidiTarget::stemSolo2: return "Stem 3 Solo";
        case MidiTarget::stemSolo3: return "Stem 4 Solo";
        case MidiTarget::stemSolo4: return "Stem 5 Solo";
        case MidiTarget::stemSolo5: return "Stem 6 Solo";
        case MidiTarget::stemSolo6: return "Stem 7 Solo";
        case MidiTarget::stemSolo7: return "Stem 8 Solo";
    }
    return "None";
}

MidiTarget midiTargetFromString (const juce::String& name)
{
    for (int i = static_cast<int> (MidiTarget::none);
         i <= static_cast<int> (MidiTarget::stemSolo7); ++i)
    {
        const auto t = static_cast<MidiTarget> (i);
        if (midiTargetToString (t).equalsIgnoreCase (name))
            return t;
    }
    return MidiTarget::none;
}

juce::StringArray allMidiTargetNames()
{
    juce::StringArray names;
    for (int i = static_cast<int> (MidiTarget::none);
         i <= static_cast<int> (MidiTarget::stemSolo7); ++i)
        names.add (midiTargetToString (static_cast<MidiTarget> (i)));
    return names;
}

int stemIndexFromVolumeTarget (const MidiTarget target) noexcept
{
    const auto v = static_cast<int> (target) - static_cast<int> (MidiTarget::stemVolume0);
    return (v >= 0 && v < MidiMappingProfile::maxStems) ? v : -1;
}

int stemIndexFromMuteTarget (const MidiTarget target) noexcept
{
    const auto v = static_cast<int> (target) - static_cast<int> (MidiTarget::stemMute0);
    return (v >= 0 && v < MidiMappingProfile::maxStems) ? v : -1;
}

int stemIndexFromSoloTarget (const MidiTarget target) noexcept
{
    const auto v = static_cast<int> (target) - static_cast<int> (MidiTarget::stemSolo0);
    return (v >= 0 && v < MidiMappingProfile::maxStems) ? v : -1;
}

MidiTarget stemVolumeTarget (const int index) noexcept
{
    if (index < 0 || index >= MidiMappingProfile::maxStems)
        return MidiTarget::none;
    return static_cast<MidiTarget> (static_cast<int> (MidiTarget::stemVolume0) + index);
}

MidiTarget stemMuteTarget (const int index) noexcept
{
    if (index < 0 || index >= MidiMappingProfile::maxStems)
        return MidiTarget::none;
    return static_cast<MidiTarget> (static_cast<int> (MidiTarget::stemMute0) + index);
}

MidiTarget stemSoloTarget (const int index) noexcept
{
    if (index < 0 || index >= MidiMappingProfile::maxStems)
        return MidiTarget::none;
    return static_cast<MidiTarget> (static_cast<int> (MidiTarget::stemSolo0) + index);
}

juce::var MidiBinding::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", typeToString (type));
    o->setProperty ("channel", channel);
    o->setProperty ("number", number);
    o->setProperty ("target", midiTargetToString (target));
    o->setProperty ("toggle", toggle);
    o->setProperty ("invert", invert);
    return juce::var (o);
}

MidiBinding MidiBinding::fromVar (const juce::var& data)
{
    MidiBinding b;
    if (auto* o = data.getDynamicObject())
    {
        b.type = typeFromString (o->getProperty ("type").toString());
        b.channel = static_cast<int> (o->getProperty ("channel"));
        b.number = static_cast<int> (o->getProperty ("number"));
        b.target = midiTargetFromString (o->getProperty ("target").toString());
        b.toggle = static_cast<bool> (o->getProperty ("toggle"));
        b.invert = static_cast<bool> (o->getProperty ("invert"));
    }
    return b;
}

juce::String MidiBinding::describe() const
{
    juce::String src;
    if (type == Type::cc)
        src = "CC " + juce::String (number);
    else if (type == Type::note)
        src = "Note " + juce::String (number);
    else
        src = "PitchBend";

    if (channel >= 0)
        src += " ch" + juce::String (channel + 1);

    return src + " -> " + midiTargetToString (target);
}

void MidiMappingProfile::addBinding (MidiBinding binding)
{
    if (binding.target == MidiTarget::none)
        return;
    removeBindingsForTarget (binding.target);
    bindings.add (std::move (binding));
}

void MidiMappingProfile::removeBindingsForTarget (const MidiTarget target)
{
    for (int i = bindings.size(); --i >= 0;)
        if (bindings.getReference (i).target == target)
            bindings.remove (i);
}

const MidiBinding* MidiMappingProfile::findBinding (const MidiBinding::Type type,
                                                    const int channel,
                                                    const int number) const
{
    for (const auto& b : bindings)
    {
        if (b.type != type || b.number != number)
            continue;
        if (b.channel >= 0 && channel >= 0 && b.channel != channel)
            continue;
        return &b;
    }
    return nullptr;
}

juce::var MidiMappingProfile::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("name", name);
    o->setProperty ("notes", notes);
    juce::Array<juce::var> arr;
    for (const auto& b : bindings)
        arr.add (b.toVar());
    o->setProperty ("bindings", arr);
    return juce::var (o);
}

MidiMappingProfile MidiMappingProfile::fromVar (const juce::var& data)
{
    MidiMappingProfile p;
    if (auto* o = data.getDynamicObject())
    {
        p.id = o->getProperty ("id").toString();
        p.name = o->getProperty ("name").toString();
        p.notes = o->getProperty ("notes").toString();
        if (auto* arr = o->getProperty ("bindings").getArray())
            for (const auto& item : *arr)
                p.bindings.add (MidiBinding::fromVar (item));
    }
    return p;
}

MidiMappingProfile MidiMappingProfile::genericDaw()
{
    MidiMappingProfile p;
    p.id = "generic-daw";
    p.name = "Generic DAW (CC faders)";
    p.notes = "Works with most USB controllers in CC mode: faders CC0-7, mute CC16-23, "
              "solo CC24-31, master CC7, transport CC41-43.";

    for (int i = 0; i < maxStems; ++i)
    {
        p.bindings.add (makeCc (i, stemVolumeTarget (i)));
        p.bindings.add (makeCc (16 + i, stemMuteTarget (i), 0, true));
        p.bindings.add (makeCc (24 + i, stemSoloTarget (i), 0, true));
    }

    // Dedicated master (avoid clashing with stem 8 fader on CC7)
    p.bindings.add (makeCc (119, MidiTarget::masterVolume));
    p.bindings.add (makeCc (41, MidiTarget::play, 0, false));
    p.bindings.add (makeCc (42, MidiTarget::stop, 0, false));
    p.bindings.add (makeCc (43, MidiTarget::pause, 0, false));
    p.bindings.add (makeCc (44, MidiTarget::togglePlayPause, 0, false));
    p.bindings.add (makeCc (45, MidiTarget::recordToggle, 0, true));
    p.bindings.add (makeCc (46, MidiTarget::metronomeToggle, 0, true));
    // Sustain pedal / stage footswitch (CC 64) -> next set-list song
    p.bindings.add (makeCc (64, MidiTarget::nextSong, 0, false));
    return p;
}

MidiMappingProfile MidiMappingProfile::korgNanoKontrol2()
{
    // Factory CC mode (Scene 1 style / common Linux class-compliant map)
    MidiMappingProfile p;
    p.id = "korg-nanokontrol2";
    p.name = "Korg nanoKONTROL2";
    p.notes = "CC mode: faders 0-7, knobs 16-23 (unused), S 32-39 solo, M 48-55 mute, "
              "Play 41, Stop 42, Rec 45, Cycle 46.";

    for (int i = 0; i < maxStems; ++i)
    {
        p.bindings.add (makeCc (i, stemVolumeTarget (i)));
        p.bindings.add (makeCc (32 + i, stemSoloTarget (i), 0, true));
        p.bindings.add (makeCc (48 + i, stemMuteTarget (i), 0, true));
    }

    p.bindings.add (makeCc (41, MidiTarget::play));
    p.bindings.add (makeCc (42, MidiTarget::stop));
    p.bindings.add (makeCc (43, MidiTarget::pause)); // rew often; still useful
    p.bindings.add (makeCc (45, MidiTarget::recordToggle, 0, true));
    p.bindings.add (makeCc (46, MidiTarget::metronomeToggle, 0, true));
    return p;
}

MidiMappingProfile MidiMappingProfile::akaiApcMini()
{
    // Note-based pads + faders as CC in many firmware modes
    MidiMappingProfile p;
    p.id = "akai-apc-mini";
    p.name = "Akai APC Mini / generic pads";
    p.notes = "Faders CC 48-56 (track 1-8 + master), mute notes 64-71, solo notes 82-89, "
              "Play note 91, Stop note 92.";

    for (int i = 0; i < maxStems; ++i)
    {
        p.bindings.add (makeCc (48 + i, stemVolumeTarget (i)));
        p.bindings.add (makeNote (64 + i, stemMuteTarget (i)));
        p.bindings.add (makeNote (82 + i, stemSoloTarget (i)));
    }

    p.bindings.add (makeCc (56, MidiTarget::masterVolume));
    p.bindings.add (makeNote (91, MidiTarget::play, 0, false));
    p.bindings.add (makeNote (92, MidiTarget::stop, 0, false));
    p.bindings.add (makeNote (93, MidiTarget::togglePlayPause, 0, false));
    p.bindings.add (makeNote (98, MidiTarget::recordToggle));
    return p;
}

MidiMappingProfile MidiMappingProfile::novationLaunchControl()
{
    MidiMappingProfile p;
    p.id = "novation-launchcontrol";
    p.name = "Novation Launch Control / XL";
    p.notes = "Template-style: faders CC 77-84, mute CC 73-80 (momentary->toggle), "
              "solo CC 41-48, transport often notes.";

    for (int i = 0; i < maxStems; ++i)
    {
        p.bindings.add (makeCc (77 + i, stemVolumeTarget (i)));
        p.bindings.add (makeCc (73 + i, stemMuteTarget (i), 0, true));
        p.bindings.add (makeCc (41 + i, stemSoloTarget (i), 0, true));
    }

    p.bindings.add (makeCc (7, MidiTarget::masterVolume));
    p.bindings.add (makeNote (115, MidiTarget::play, 0, false));
    p.bindings.add (makeNote (114, MidiTarget::stop, 0, false));
    p.bindings.add (makeNote (117, MidiTarget::recordToggle));
    return p;
}

MidiMappingProfile MidiMappingProfile::behringerXTouchMini()
{
    // Standard (not MC) mode - encoders as CC, layered buttons
    MidiMappingProfile p;
    p.id = "behringer-xtouch-mini";
    p.name = "Behringer X-Touch Mini";
    p.notes = "Standard mode: encoders CC 1-8 as volumes, layer A buttons notes 89-96 mute, "
              "layer B 0-7 solo, transport buttons notes.";

    for (int i = 0; i < maxStems; ++i)
    {
        p.bindings.add (makeCc (1 + i, stemVolumeTarget (i)));
        p.bindings.add (makeNote (89 + i, stemMuteTarget (i)));
        p.bindings.add (makeNote (i, stemSoloTarget (i)));
    }

    p.bindings.add (makeCc (9, MidiTarget::masterVolume));
    p.bindings.add (makeNote (93, MidiTarget::play, 0, false));
    p.bindings.add (makeNote (94, MidiTarget::stop, 0, false));
    p.bindings.add (makeNote (95, MidiTarget::recordToggle));
    return p;
}

MidiMappingProfile MidiMappingProfile::mackieControlLite()
{
    // Simplified MCU: pitch bend on ch 1-8 = faders, notes for mute/solo/transport
    MidiMappingProfile p;
    p.id = "mackie-control-lite";
    p.name = "Mackie Control / MCU (lite)";
    p.notes = "Pitch bend ch1-8 -> stem volumes (common MCU fader). Mute notes 16-23, "
              "solo 8-15, Play 94, Stop 93, Rec 95. Use with controllers in MC/MCU mode.";

    for (int i = 0; i < maxStems; ++i)
    {
        MidiBinding bend;
        bend.type = MidiBinding::Type::pitchBend;
        bend.channel = i; // channel encodes track
        bend.number = 0;
        bend.target = stemVolumeTarget (i);
        p.bindings.add (bend);
        p.bindings.add (makeNote (16 + i, stemMuteTarget (i)));
        p.bindings.add (makeNote (8 + i, stemSoloTarget (i)));
    }

    p.bindings.add (makeNote (94, MidiTarget::play, 0, false));
    p.bindings.add (makeNote (93, MidiTarget::stop, 0, false));
    p.bindings.add (makeNote (95, MidiTarget::recordToggle));
    return p;
}

juce::Array<MidiMappingProfile> MidiMappingProfile::builtInProfiles()
{
    juce::Array<MidiMappingProfile> list;
    list.add (genericDaw());
    list.add (korgNanoKontrol2());
    list.add (akaiApcMini());
    list.add (novationLaunchControl());
    list.add (behringerXTouchMini());
    list.add (mackieControlLite());
    return list;
}

juce::File MidiControlSettings::settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("JamStudio")
        .getChildFile ("midi-control.json");
}

void MidiControlSettings::load()
{
    const auto file = MidiControlSettings::settingsFile();
    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (auto* o = parsed.getDynamicObject())
    {
        enabled = static_cast<bool> (o->getProperty ("enabled"));
        inputDeviceIdentifier = o->getProperty ("inputDeviceIdentifier").toString();
        profileId = o->getProperty ("profileId").toString();
        useCustomBindings = static_cast<bool> (o->getProperty ("useCustomBindings"));
        customProfile = MidiMappingProfile::fromVar (o->getProperty ("customProfile"));
        if (profileId.isEmpty())
            profileId = "generic-daw";
    }
}

void MidiControlSettings::save() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("enabled", enabled);
    o->setProperty ("inputDeviceIdentifier", inputDeviceIdentifier);
    o->setProperty ("profileId", profileId);
    o->setProperty ("useCustomBindings", useCustomBindings);
    o->setProperty ("customProfile", customProfile.toVar());

    const auto file = MidiControlSettings::settingsFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

MidiMappingProfile MidiControlSettings::activeProfile() const
{
    if (useCustomBindings && customProfile.bindings.size() > 0)
    {
        auto p = customProfile;
        if (p.id.isEmpty())
            p.id = "custom";
        if (p.name.isEmpty())
            p.name = "Custom (MIDI Learn)";
        return p;
    }

    for (const auto& p : MidiMappingProfile::builtInProfiles())
        if (p.id == profileId)
            return p;

    return MidiMappingProfile::genericDaw();
}

} // namespace jamstudio::midi
