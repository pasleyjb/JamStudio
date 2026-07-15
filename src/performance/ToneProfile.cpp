#include "ToneProfile.h"

namespace jamstudio::performance
{

juce::String liveInstrumentRoleName (const LiveInstrumentRole role)
{
    switch (role)
    {
        case LiveInstrumentRole::guitar1: return "Guitar 1";
        case LiveInstrumentRole::guitar2: return "Guitar 2";
        case LiveInstrumentRole::bass: return "Bass";
        case LiveInstrumentRole::count:
        default: return "Tone";
    }
}

juce::String liveInstrumentRoleShortName (const LiveInstrumentRole role)
{
    switch (role)
    {
        case LiveInstrumentRole::guitar1: return "G1";
        case LiveInstrumentRole::guitar2: return "G2";
        case LiveInstrumentRole::bass: return "Bass";
        case LiveInstrumentRole::count:
        default: return "?";
    }
}

juce::Colour liveInstrumentRoleColour (const LiveInstrumentRole role)
{
    switch (role)
    {
        case LiveInstrumentRole::guitar1: return juce::Colour (0xffe8a838); // amber
        case LiveInstrumentRole::guitar2: return juce::Colour (0xff4ecdc4); // teal
        case LiveInstrumentRole::bass: return juce::Colour (0xff9b59b6);    // purple
        case LiveInstrumentRole::count:
        default: return juce::Colours::grey;
    }
}

namespace
{
juce::String roleToString (const LiveInstrumentRole role)
{
    switch (role)
    {
        case LiveInstrumentRole::guitar1: return "guitar1";
        case LiveInstrumentRole::guitar2: return "guitar2";
        case LiveInstrumentRole::bass: return "bass";
        case LiveInstrumentRole::count:
        default: return "guitar1";
    }
}

LiveInstrumentRole roleFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("guitar2") || s.equalsIgnoreCase ("g2"))
        return LiveInstrumentRole::guitar2;
    if (s.equalsIgnoreCase ("bass"))
        return LiveInstrumentRole::bass;
    return LiveInstrumentRole::guitar1;
}
} // namespace

juce::var ToneProfile::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("name", name);
    o->setProperty ("role", roleToString (role));
    o->setProperty ("namModelPath", namModelPath);
    o->setProperty ("cabIrPath", cabIrPath);
    o->setProperty ("inputGain", inputGain);
    o->setProperty ("drive", drive);
    o->setProperty ("bass", bass);
    o->setProperty ("mid", mid);
    o->setProperty ("treble", treble);
    o->setProperty ("presence", presence);
    o->setProperty ("outputLevel", outputLevel);
    o->setProperty ("bypass", bypass);
    return juce::var (o);
}

ToneProfile ToneProfile::fromVar (const juce::var& data)
{
    ToneProfile p;

    if (auto* o = data.getDynamicObject())
    {
        p.id = o->getProperty ("id").toString();
        p.name = o->getProperty ("name").toString();
        p.role = roleFromString (o->getProperty ("role").toString());
        p.namModelPath = o->getProperty ("namModelPath").toString();
        p.cabIrPath = o->getProperty ("cabIrPath").toString();
        p.inputGain = (float) (double) o->getProperty ("inputGain");
        p.drive = (float) (double) o->getProperty ("drive");
        p.bass = (float) (double) o->getProperty ("bass");
        p.mid = (float) (double) o->getProperty ("mid");
        p.treble = (float) (double) o->getProperty ("treble");
        p.presence = (float) (double) o->getProperty ("presence");
        p.outputLevel = (float) (double) o->getProperty ("outputLevel");
        p.bypass = (bool) o->getProperty ("bypass");

        if (p.name.isEmpty())
            p.name = "Tone";
        if (p.id.isEmpty())
            p.id = juce::Uuid().toDashedString();

        // Clamp in case of hand-edited JSON
        auto clamp01 = [] (float& v) { v = juce::jlimit (0.0f, 1.0f, v); };
        clamp01 (p.inputGain);
        clamp01 (p.drive);
        clamp01 (p.bass);
        clamp01 (p.mid);
        clamp01 (p.treble);
        clamp01 (p.presence);
        clamp01 (p.outputLevel);
    }

    return p;
}

ToneProfile ToneProfile::makeDefault (const LiveInstrumentRole role)
{
    ToneProfile p;
    p.id = juce::Uuid().toDashedString();
    p.role = role;

    switch (role)
    {
        case LiveInstrumentRole::guitar1:
            p.name = "G1 Crunch";
            p.inputGain = 0.55f;
            p.drive = 0.52f;
            p.bass = 0.48f;
            p.mid = 0.55f;
            p.treble = 0.52f;
            p.presence = 0.5f;
            p.outputLevel = 0.72f;
            break;
        case LiveInstrumentRole::guitar2:
            p.name = "G2 Clean";
            p.inputGain = 0.5f;
            p.drive = 0.22f;
            p.bass = 0.45f;
            p.mid = 0.5f;
            p.treble = 0.58f;
            p.presence = 0.48f;
            p.outputLevel = 0.7f;
            break;
        case LiveInstrumentRole::bass:
            p.name = "Bass Round";
            p.inputGain = 0.5f;
            p.drive = 0.28f;
            p.bass = 0.62f;
            p.mid = 0.42f;
            p.treble = 0.38f;
            p.presence = 0.35f;
            p.outputLevel = 0.75f;
            break;
        case LiveInstrumentRole::count:
        default:
            p.name = "Default";
            break;
    }

    return p;
}

juce::String& SongToneAssignment::idFor (const LiveInstrumentRole role) noexcept
{
    switch (role)
    {
        case LiveInstrumentRole::guitar2: return guitar2ProfileId;
        case LiveInstrumentRole::bass: return bassProfileId;
        case LiveInstrumentRole::guitar1:
        case LiveInstrumentRole::count:
        default: return guitar1ProfileId;
    }
}

const juce::String& SongToneAssignment::idFor (const LiveInstrumentRole role) const noexcept
{
    switch (role)
    {
        case LiveInstrumentRole::guitar2: return guitar2ProfileId;
        case LiveInstrumentRole::bass: return bassProfileId;
        case LiveInstrumentRole::guitar1:
        case LiveInstrumentRole::count:
        default: return guitar1ProfileId;
    }
}

juce::var SongToneAssignment::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("guitar1", guitar1ProfileId);
    o->setProperty ("guitar2", guitar2ProfileId);
    o->setProperty ("bass", bassProfileId);
    return juce::var (o);
}

SongToneAssignment SongToneAssignment::fromVar (const juce::var& data)
{
    SongToneAssignment a;
    if (auto* o = data.getDynamicObject())
    {
        a.guitar1ProfileId = o->getProperty ("guitar1").toString();
        a.guitar2ProfileId = o->getProperty ("guitar2").toString();
        a.bassProfileId = o->getProperty ("bass").toString();
    }
    return a;
}

// ---------------------------------------------------------------------------

ToneLibrary::ToneLibrary()
{
    ensureDirectories (true);
    ensureDefaults();
}

juce::File ToneLibrary::getJamStudioRoot()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("JamStudio");
    dir.createDirectory();
    return dir;
}

juce::File ToneLibrary::getLibraryDirectory() const
{
    auto dir = getJamStudioRoot().getChildFile ("Tones");
    dir.createDirectory();
    return dir;
}

juce::File ToneLibrary::getLibraryFile() const
{
    return getLibraryDirectory().getChildFile ("tone-library.json");
}

juce::File ToneLibrary::getAmpModelsRoot() const
{
    auto dir = getJamStudioRoot().getChildFile ("AmpModels");
    dir.createDirectory();
    return dir;
}

juce::File ToneLibrary::getAmpModelsDirectoryForRole (const LiveInstrumentRole role) const
{
    const char* sub = "guitar";
    if (role == LiveInstrumentRole::bass)
        sub = "bass";
    auto dir = getAmpModelsRoot().getChildFile (sub);
    dir.createDirectory();
    return dir;
}

juce::File ToneLibrary::getSharedAmpModelsDirectory() const
{
    auto dir = getAmpModelsRoot().getChildFile ("shared");
    dir.createDirectory();
    return dir;
}

juce::File ToneLibrary::getCabIrsDirectory() const
{
    auto dir = getJamStudioRoot().getChildFile ("CabIRs");
    dir.createDirectory();
    return dir;
}

void ToneLibrary::ensureDirectories (const bool seedExampleModels) const
{
    getLibraryDirectory();
    getAmpModelsDirectoryForRole (LiveInstrumentRole::guitar1);
    getAmpModelsDirectoryForRole (LiveInstrumentRole::bass);
    getSharedAmpModelsDirectory();
    getCabIrsDirectory();

    // Drop a short README so the folders make sense in the file manager.
    const auto readme = getAmpModelsRoot().getChildFile ("README.txt");
    if (! readme.existsAsFile())
    {
        readme.replaceWithText (
            "JamStudio amp models (.nam)\n"
            "===========================\n\n"
            "guitar/   — models for Guitar 1 and Guitar 2 (G1 / G2)\n"
            "bass/     — models for Bass\n"
            "shared/   — models usable by any path\n\n"
            "Import from Performance Setup (Load .nam), or copy .nam files here.\n"
            "Download free models from https://www.tone3000.com\n\n"
            "Cab IRs (optional) go in ../CabIRs/\n"
            "Tone knob profiles are saved in ../Tones/tone-library.json\n");
    }

    if (! seedExampleModels)
        return;

    // Seed shared/ from bundled NeuralAmpModelerCore examples when empty.
    const auto shared = getSharedAmpModelsDirectory();
    if (! shared.findChildFiles (juce::File::findFiles, false, "*.nam").isEmpty())
        return;

    juce::Array<juce::File> exampleRoots;
   #ifdef JAMSTUDIO_SOURCE_DIR
    exampleRoots.add (juce::File (JAMSTUDIO_SOURCE_DIR)
                          .getChildFile ("third_party/NeuralAmpModelerCore/example_models"));
   #endif
    exampleRoots.add (juce::File::getCurrentWorkingDirectory()
                          .getChildFile ("third_party/NeuralAmpModelerCore/example_models"));
    exampleRoots.add (juce::File::getCurrentWorkingDirectory()
                          .getChildFile ("../third_party/NeuralAmpModelerCore/example_models"));

    for (const auto& root : exampleRoots)
    {
        if (! root.isDirectory())
            continue;
        for (const auto& f : root.findChildFiles (juce::File::findFiles, false, "*.nam"))
        {
            // Prefer small useful examples; copy a few only.
            const auto name = f.getFileName();
            if (name.containsIgnoreCase ("wavenet") || name.containsIgnoreCase ("lstm")
                || name.containsIgnoreCase ("A2"))
            {
                f.copyFileTo (shared.getChildFile (name));
            }
        }
        break;
    }
}

juce::File ToneLibrary::importNamModel (const juce::File& sourceNam,
                                        const LiveInstrumentRole role,
                                        const bool useSharedFolder) const
{
    if (! sourceNam.existsAsFile() || ! sourceNam.hasFileExtension (".nam"))
        return {};

    ensureDirectories (false);
    auto destDir = useSharedFolder ? getSharedAmpModelsDirectory()
                                   : getAmpModelsDirectoryForRole (role);
    auto dest = destDir.getChildFile (sourceNam.getFileName());

    if (dest.existsAsFile() && dest.getSize() == sourceNam.getSize())
        return dest;

    if (dest.existsAsFile())
    {
        int n = 2;
        while (dest.existsAsFile())
        {
            dest = destDir.getChildFile (sourceNam.getFileNameWithoutExtension()
                                         + "-" + juce::String (n) + ".nam");
            ++n;
        }
    }

    if (! sourceNam.copyFileTo (dest))
        return {};

    return dest;
}

juce::Array<juce::File> ToneLibrary::listNamModels (const LiveInstrumentRole role) const
{
    ensureDirectories (false);
    juce::Array<juce::File> files;

    auto addDir = [&files] (const juce::File& dir)
    {
        if (! dir.isDirectory())
            return;
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.nam"))
            files.addIfNotAlreadyThere (f);
    };

    addDir (getAmpModelsDirectoryForRole (role));
    addDir (getSharedAmpModelsDirectory());
    files.sort();
    return files;
}

bool ToneLibrary::load()
{
    ensureDirectories (true);
    const auto file = getLibraryFile();
    if (! file.existsAsFile())
    {
        ensureDefaults();
        return save();
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    profiles.clear();

    if (auto* o = parsed.getDynamicObject())
        if (const auto* arr = o->getProperty ("profiles").getArray())
            for (const auto& v : *arr)
                profiles.add (ToneProfile::fromVar (v));

    ensureDefaults();
    return true;
}

bool ToneLibrary::save() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    juce::Array<juce::var> arr;
    for (const auto& p : profiles)
        arr.add (p.toVar());
    o->setProperty ("profiles", arr);

    const auto file = getLibraryFile();
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

juce::Array<ToneProfile> ToneLibrary::profilesForRole (const LiveInstrumentRole role) const
{
    juce::Array<ToneProfile> out;
    for (const auto& p : profiles)
        if (p.role == role)
            out.add (p);
    return out;
}

ToneProfile* ToneLibrary::findById (const juce::String& id)
{
    if (id.isEmpty())
        return nullptr;
    for (auto& p : profiles)
        if (p.id == id)
            return &p;
    return nullptr;
}

const ToneProfile* ToneLibrary::findById (const juce::String& id) const
{
    if (id.isEmpty())
        return nullptr;
    for (const auto& p : profiles)
        if (p.id == id)
            return &p;
    return nullptr;
}

ToneProfile ToneLibrary::resolve (const LiveInstrumentRole role, const juce::String& profileId) const
{
    if (const auto* p = findById (profileId))
        return *p;

    for (const auto& p : profiles)
        if (p.role == role)
            return p;

    return ToneProfile::makeDefault (role);
}

ToneProfile& ToneLibrary::addProfile (ToneProfile profile)
{
    if (profile.id.isEmpty())
        profile.id = juce::Uuid().toDashedString();
    profiles.add (profile);
    return profiles.getReference (profiles.size() - 1);
}

bool ToneLibrary::updateProfile (const ToneProfile& profile)
{
    for (int i = 0; i < profiles.size(); ++i)
    {
        if (profiles.getReference (i).id == profile.id)
        {
            profiles.getReference (i) = profile;
            return true;
        }
    }
    return false;
}

bool ToneLibrary::removeProfile (const juce::String& id)
{
    for (int i = 0; i < profiles.size(); ++i)
    {
        if (profiles.getReference (i).id == id)
        {
            profiles.remove (i);
            ensureDefaults();
            return true;
        }
    }
    return false;
}

void ToneLibrary::ensureDefaults()
{
    auto hasRole = [this] (const LiveInstrumentRole role)
    {
        for (const auto& p : profiles)
            if (p.role == role)
                return true;
        return false;
    };

    for (int r = 0; r < kNumLiveTonePaths; ++r)
    {
        const auto role = static_cast<LiveInstrumentRole> (r);
        if (! hasRole (role))
            profiles.add (ToneProfile::makeDefault (role));
    }
}

} // namespace jamstudio::performance
