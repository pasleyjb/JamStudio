#include "SetListData.h"

namespace jamstudio::performance
{

juce::String stageMediaKindToString (const StageMediaKind kind)
{
    switch (kind)
    {
        case StageMediaKind::video: return "video";
        case StageMediaKind::slideshow: return "slideshow";
        case StageMediaKind::none:
        default: return "none";
    }
}

StageMediaKind stageMediaKindFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("video"))
        return StageMediaKind::video;
    if (s.equalsIgnoreCase ("slideshow"))
        return StageMediaKind::slideshow;
    return StageMediaKind::none;
}

juce::var StemMixPref::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", stemName);
    o->setProperty ("volume", volume);
    o->setProperty ("monA", monA);
    o->setProperty ("monB", monB);
    o->setProperty ("mon1", monA); // canonical names
    o->setProperty ("mon2", monB);
    o->setProperty ("mon3", mon3);
    o->setProperty ("mon4", mon4);
    o->setProperty ("mon5", mon5);
    o->setProperty ("muted", muted);
    o->setProperty ("solo", solo);
    return juce::var (o);
}

StemMixPref StemMixPref::fromVar (const juce::var& data)
{
    StemMixPref p;

    if (auto* o = data.getDynamicObject())
    {
        p.stemName = o->getProperty ("name").toString();
        p.volume = static_cast<float> (static_cast<double> (o->getProperty ("volume")));
        // Prefer mon1/mon2; fall back to monA/monB for older setlists.
        if (o->hasProperty ("mon1"))
            p.monA = static_cast<float> (static_cast<double> (o->getProperty ("mon1")));
        else if (o->hasProperty ("monA"))
            p.monA = static_cast<float> (static_cast<double> (o->getProperty ("monA")));
        else
            p.monA = p.volume;

        if (o->hasProperty ("mon2"))
            p.monB = static_cast<float> (static_cast<double> (o->getProperty ("mon2")));
        else if (o->hasProperty ("monB"))
            p.monB = static_cast<float> (static_cast<double> (o->getProperty ("monB")));
        else
            p.monB = 0.0f;

        p.mon3 = o->hasProperty ("mon3")
                     ? static_cast<float> (static_cast<double> (o->getProperty ("mon3")))
                     : 0.0f;
        p.mon4 = o->hasProperty ("mon4")
                     ? static_cast<float> (static_cast<double> (o->getProperty ("mon4")))
                     : 0.0f;
        p.mon5 = o->hasProperty ("mon5")
                     ? static_cast<float> (static_cast<double> (o->getProperty ("mon5")))
                     : 0.0f;
        p.muted = static_cast<bool> (o->getProperty ("muted"));
        p.solo = static_cast<bool> (o->getProperty ("solo"));
    }

    return p;
}

namespace
{
juce::File resolvePath (const juce::String& path, const juce::File& setListFile)
{
    if (path.isEmpty())
        return {};

    juce::File f (path);

    if (juce::File::isAbsolutePath (path) && f.exists())
        return f;

    if (setListFile != juce::File())
    {
        auto rel = setListFile.getParentDirectory().getChildFile (path);
        if (rel.exists())
            return rel;
    }

    return f;
}
} // namespace

juce::File SetListSong::resolveStageMediaFile (const juce::File& setListFile) const
{
    return resolvePath (stageMediaPath, setListFile);
}

juce::Array<juce::File> SetListSong::resolveSlideFiles (const juce::File& setListFile) const
{
    juce::Array<juce::File> files;

    if (! stageSlidePaths.isEmpty())
    {
        for (const auto& p : stageSlidePaths)
        {
            auto f = resolvePath (p, setListFile);
            if (f.existsAsFile())
                files.add (f);
        }
        return files;
    }

    auto root = resolveStageMediaFile (setListFile);

    if (root.isDirectory())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (
                 root, false, "*.jpg;*.jpeg;*.png;*.gif;*.bmp;*.webp", juce::File::findFiles))
            files.add (entry.getFile());

        files.sort();
    }
    else if (root.existsAsFile())
    {
        files.add (root);
    }

    return files;
}

juce::var SetListSong::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("projectPath", projectPath);
    o->setProperty ("displayName", displayName);
    o->setProperty ("showTabs", showTabs);
    o->setProperty ("showLyrics", showLyrics);
    o->setProperty ("preferredPartHint", preferredPartHint);
    o->setProperty ("stageMediaKind", stageMediaKindToString (stageMediaKind));
    o->setProperty ("stageMediaPath", stageMediaPath);
    o->setProperty ("stageMediaAutoPlay", stageMediaAutoPlay);
    o->setProperty ("stageSlideSeconds", stageSlideSeconds);

    juce::Array<juce::var> prefs;
    for (const auto& p : stemPrefs)
        prefs.add (p.toVar());
    o->setProperty ("stemPrefs", prefs);

    juce::Array<juce::var> slides;
    for (const auto& p : stageSlidePaths)
        slides.add (p);
    o->setProperty ("stageSlidePaths", slides);

    return juce::var (o);
}

SetListSong SetListSong::fromVar (const juce::var& data)
{
    SetListSong s;

    if (auto* o = data.getDynamicObject())
    {
        s.projectPath = o->getProperty ("projectPath").toString();
        s.displayName = o->getProperty ("displayName").toString();
        s.showTabs = o->hasProperty ("showTabs") ? static_cast<bool> (o->getProperty ("showTabs")) : true;
        s.showLyrics = o->hasProperty ("showLyrics") ? static_cast<bool> (o->getProperty ("showLyrics")) : true;
        s.preferredPartHint = o->getProperty ("preferredPartHint").toString();

        if (s.preferredPartHint.isEmpty())
            s.preferredPartHint = "Guitar";

        s.stageMediaKind = stageMediaKindFromString (o->getProperty ("stageMediaKind").toString());
        s.stageMediaPath = o->getProperty ("stageMediaPath").toString();
        s.stageMediaAutoPlay = o->hasProperty ("stageMediaAutoPlay")
                                   ? static_cast<bool> (o->getProperty ("stageMediaAutoPlay"))
                                   : true;
        s.stageSlideSeconds = o->hasProperty ("stageSlideSeconds")
                                  ? static_cast<float> (static_cast<double> (o->getProperty ("stageSlideSeconds")))
                                  : 5.0f;

        if (const auto* arr = o->getProperty ("stemPrefs").getArray())
            for (const auto& v : *arr)
                s.stemPrefs.add (StemMixPref::fromVar (v));

        if (const auto* arr = o->getProperty ("stageSlidePaths").getArray())
            for (const auto& v : *arr)
                s.stageSlidePaths.add (v.toString());
    }

    if (s.displayName.isEmpty() && s.projectPath.isNotEmpty())
        s.displayName = juce::File (s.projectPath).getFileNameWithoutExtension();

    return s;
}

juce::var SetList::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("version", 2);

    juce::Array<juce::var> songArr;
    for (const auto& s : songs)
        songArr.add (s.toVar());
    o->setProperty ("songs", songArr);

    juce::Array<juce::var> defArr;
    for (const auto& p : defaultStemPrefs)
        defArr.add (p.toVar());
    o->setProperty ("defaultStemPrefs", defArr);

    return juce::var (o);
}

SetList SetList::fromVar (const juce::var& data)
{
    SetList list;

    if (auto* o = data.getDynamicObject())
    {
        list.name = o->getProperty ("name").toString();

        if (list.name.isEmpty())
            list.name = "My Set";

        if (const auto* arr = o->getProperty ("songs").getArray())
            for (const auto& v : *arr)
                list.songs.add (SetListSong::fromVar (v));

        if (const auto* arr = o->getProperty ("defaultStemPrefs").getArray())
            for (const auto& v : *arr)
                list.defaultStemPrefs.add (StemMixPref::fromVar (v));
    }

    if (list.defaultStemPrefs.isEmpty())
        list.defaultStemPrefs = leadGuitarSingerDefaults();

    return list;
}

juce::Array<StemMixPref> SetList::leadGuitarSingerDefaults()
{
    juce::Array<StemMixPref> prefs;

    auto add = [&] (const char* name, float foh, float mon, bool mute)
    {
        StemMixPref p;
        p.stemName = name;
        p.volume = foh;
        p.monA = mon;
        p.monB = 0.0f;
        p.muted = mute;
        p.solo = false;
        prefs.add (p);
    };

    add ("Guitar", 0.35f, 0.85f, false);
    add ("Vocals", 0.0f, 0.5f, true);
    add ("Drums", 1.0f, 0.8f, false);
    add ("Bass", 1.0f, 0.8f, false);
    add ("Piano", 0.75f, 0.6f, false);
    add ("Other", 0.75f, 0.6f, false);

    return prefs;
}

} // namespace jamstudio::performance
