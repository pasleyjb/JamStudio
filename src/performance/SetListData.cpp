#include "SetListData.h"

namespace jamstudio::performance
{

juce::var StemMixPref::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", stemName);
    o->setProperty ("volume", volume);
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
        p.muted = static_cast<bool> (o->getProperty ("muted"));
        p.solo = static_cast<bool> (o->getProperty ("solo"));
    }

    return p;
}

juce::var SetListSong::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("projectPath", projectPath);
    o->setProperty ("displayName", displayName);
    o->setProperty ("showTabs", showTabs);
    o->setProperty ("showLyrics", showLyrics);
    o->setProperty ("preferredPartHint", preferredPartHint);

    juce::Array<juce::var> prefs;

    for (const auto& p : stemPrefs)
        prefs.add (p.toVar());

    o->setProperty ("stemPrefs", prefs);
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

        if (const auto* arr = o->getProperty ("stemPrefs").getArray())
            for (const auto& v : *arr)
                s.stemPrefs.add (StemMixPref::fromVar (v));
    }

    if (s.displayName.isEmpty() && s.projectPath.isNotEmpty())
        s.displayName = juce::File (s.projectPath).getFileNameWithoutExtension();

    return s;
}

juce::var SetList::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("version", 1);

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

    auto add = [&] (const char* name, float vol, bool mute)
    {
        StemMixPref p;
        p.stemName = name;
        p.volume = vol;
        p.muted = mute;
        p.solo = false;
        prefs.add (p);
    };

    // Lead guitar + sing: back the lead guitar a bit, kill guide vocal, push rhythm section.
    add ("Guitar", 0.35f, false);
    add ("Vocals", 0.0f, true);
    add ("Drums", 1.0f, false);
    add ("Bass", 1.0f, false);
    add ("Piano", 0.75f, false);
    add ("Other", 0.75f, false);

    return prefs;
}

} // namespace jamstudio::performance
