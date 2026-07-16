#include "HelpCatalog.h"

namespace jamstudio::ui
{

bool HelpTopic::matches (const juce::String& query) const
{
    if (query.isEmpty())
        return true;

    const auto q = query.trim().toLowerCase();
    return title.toLowerCase().contains (q)
           || category.toLowerCase().contains (q)
           || keywords.toLowerCase().contains (q)
           || body.toLowerCase().contains (q);
}

namespace
{
HelpTopic topic (const char* id,
                 const char* title,
                 const char* category,
                 const char* keywords,
                 const char* body)
{
    HelpTopic t;
    t.id = id;
    t.title = title;
    t.category = category;
    t.keywords = keywords;
    t.body = body;
    return t;
}

juce::Array<HelpTopic> buildTopics()
{
    juce::Array<HelpTopic> t;

    // ---------- Getting Started ----------
    t.add (topic (
        "start-wizard",
        "Startup wizard & modes",
        "Getting Started",
        "wizard practice performance recording stage show welcome",
        "When JamStudio opens, pick a mode from the floating tiles:\n\n"
        "- Practice - learn songs with stems, tabs, and lyrics.\n"
        "- Performance - run a live set list with foot-pedal next song.\n"
        "- Stage Show - build set lists with pinned videos/slideshows.\n"
        "- Recording - capture yourself and use takes as stems.\n\n"
        "Skip the wizard anytime:\n"
        "- File / Project → Open Project... or Recent Projects...\n"
        "- File → Open Song...\n"
        "- File → New Practice from Song...\n\n"
        "Bring the wizard back: File / Project → Welcome Wizard...\n"
        "(choose mode again without quitting)."));

    t.add (topic (
        "start-practice",
        "Practice: open project or new from song",
        "Getting Started",
        "practice open project new song demucs setup",
        "In Practice mode:\n\n"
        "- Open Project - load a saved .jamstudio file with stems, tabs, lyrics.\n"
        "- New from Song - pick an audio file; JamStudio can separate stems (Demucs), "
        "fetch tabs/lyrics when AI tools are available, and auto-save a project.\n\n"
        "Use File / Project menus anytime for open/save."));

    // ---------- Projects ----------
    t.add (topic (
        "project-save-load",
        "Save and load projects",
        "Projects",
        "save load project jamstudio media stems",
        "Project / File → Open Project... loads a .jamstudio session (works even from the "
        "welcome wizard — the wizard closes automatically).\n"
        "Project → Save Project stores the song, stems, mix state, tabs, and lyrics.\n"
        "Media is kept under the project's .media folder when possible.\n\n"
        "Recent Projects... restores a recent session.\n"
        "If stems are missing, JamStudio may offer to re-separate and recover them."));

    t.add (topic (
        "file-open-song",
        "Open a song file",
        "Projects",
        "open song wav mp3 flac audio import",
        "File -> Open Song loads an audio file into the workspace.\n"
        "Supported formats include WAV, FLAC, OGG, AIFF, and compressed formats when codecs are available.\n\n"
        "After opening, separate stems or use the file as a single mix track."));

    // ---------- Transport ----------
    t.add (topic (
        "transport-main",
        "Main transport (play / pause / stop / skip)",
        "Transport",
        "play pause stop skip forward back scrub linked video",
        "The main transport bar controls setlist/song stems and, by default, stage video together:\n\n"
        "- Skip back (5 s) - Play - Pause - Stop - Skip forward (5 s)\n"
        "- Position scrubber seeks song and linked stage media.\n"
        "- Master volume controls stem master (FOH).\n\n"
        "Mixer MAIN row has the same linked transport.\n"
        "Mixer LINK toggles whether main transport also drives stage video."));

    t.add (topic (
        "transport-count-in",
        "4-count intro",
        "Transport",
        "count in count-in intro 4-in metronome click",
        "Transport -> 4-Count Intro (or 4-IN on the transport bar / mixer):\n\n"
        "When enabled, Play clicks 1-2-3-4 at the current BPM, then starts the song.\n"
        "Works in every mode. Stop/Pause cancels an in-progress count-in.\n"
        "Count-in clicks always sound; normal Metro on/off applies after playback starts."));

    t.add (topic (
        "transport-metronome",
        "Metronome (click track)",
        "Transport",
        "metronome click bpm tempo detect",
        "Metro button enables a click aligned to BPM.\n"
        "BPM slider sets tempo; Detect estimates tempo from the loaded song.\n\n"
        "In the multi-bus mixer, click defaults to Monitor mixes (not FOH).\n"
        "Use Clk FOH / Clk Mon on the BUSES strip to route click."));

    t.add (topic (
        "transport-input-monitor",
        "Hear yourself (input monitor)",
        "Transport",
        "input monitor mon hear myself guitar mic line latency direct",
        "The IN meter shows your live input. MON mixes that input into the speakers "
        "so you can hear yourself with the stems — guitar, singing, or both.\n\n"
        "- MON button ON (green) = software monitor (default)\n"
        "- Channel box: In 1 / In 2 / … = which jack; All = guitar + vocal together\n"
        "- Small slider = your monitor level\n"
        "- Turn MON off if you get feedback with open mics + speakers\n"
        "- Scarlett Direct Monitor is still best for zero-latency headphones\n\n"
        "Linux multi-input: Help → Audio Interface → enable Multi-input Pro Audio "
        "so the Scarlett is one multi-channel device (not separate Mic1/Mic2). "
        "Then open 8 input channels and set MON to All.\n\n"
        "In Performance mode, the G1/G2/Bass NAM rack replaces dry MON "
        "(processed amp sound instead of raw input)."));

    t.add (topic (
        "transport-record",
        "Record",
        "Transport",
        "record recording take stem",
        "Transport -> Record / Stop arms recording through the audio device.\n"
        "Takes can be loaded as stems for practice and mixing."));

    t.add (topic (
        "audio-interface",
        "Audio interface (plug and play)",
        "Audio",
        "audio interface scarlett focusrite asound alsa wasapi device input output monitor speakers plug hotplug",
        "Help -> Audio Interface... configures how JamStudio uses your hardware.\n\n"
        "- Plug and play (default) - multi-input USB interfaces (Scarlett 18i20, etc.) "
        "are preferred for capture. Monitor audio goes to computer speakers when "
        "'monitor on computer speakers' is on - useful when nothing is plugged into "
        "the interface outputs.\n"
        "- Same device - interface for both in and out (stage multi-bus: outs 1-2 FOH, "
        "3-4 M1 ... 11-12 M5).\n"
        "- Manual - pick exact input and output devices.\n\n"
        "Channel counts are matched to what the hardware actually exposes (not fixed "
        "to 6 outs). USB hot-plug is detected every couple of seconds.\n\n"
        "Jack sense: class-compliant USB audio does not report whether cables are "
        "plugged into line outs. JamStudio cannot know empty Scarlett jacks; use "
        "plug-and-play + computer monitor for that case.\n\n"
        "No hardware? Create a virtual 18i20 for testing (Linux):\n"
        "  ./scripts/virtual-scarlett-18i20.sh start\n"
        "Then restart JamStudio and Rescan in this dialog. Stop with:\n"
        "  ./scripts/virtual-scarlett-18i20.sh stop\n\n"
        "Settings are saved in the JamStudio app data folder (audio-interface.json)."));

    // ---------- Mixer & Buses ----------
    t.add (topic (
        "mixer-overview",
        "Mixer window overview",
        "Mixer",
        "mixer fader mute solo strip channel",
        "View -> Show Mixer opens the floating mixer board.\n\n"
        "- MAIN transport - song + linked stage video\n"
        "- Channel strips - each stem with mute, solo, and bus sends\n"
        "- BUSES - FOH + Mon 1-5 masters, PC listen, click routing\n"
        "- VIDEO - stage media transport and level\n\n"
        "Maximise uses the middle title button; click again to restore size."));

    t.add (topic (
        "mixer-save-setlist",
        "Save mix to current setlist track",
        "Mixer",
        "save mix setlist track performance automation show",
        "In Performance mode, open the Mixer and use the top button:\n\n"
        "  Save Mix -> Set Track\n\n"
        "This captures every stem's FOH, Mon 1-5, mute, and solo for the "
        "song that is currently loaded in the set list, and writes it into the "
        ".setlist file.\n\n"
        "Next time that song plays, those levels are restored automatically.\n"
        "The label next to the button shows which set track will receive the save.\n"
        "Button is disabled until a set is active and a song is loaded."));

    t.add (topic (
        "mixer-buses",
        "FOH and monitor / IEM buses",
        "Mixer",
        "foh monitor iem in-ear house pa bus send multi output",
        "Each stem has six independent send faders (band-sized monitor section):\n\n"
        "- FOH - Front of House / PA (hardware outs 1-2)\n"
        "- M1-M5 - five band / IEM mixes (outs 3-4, 5-6, 7-8, 9-10, 11-12)\n\n"
        "Name whose monitor is which in Performance Setup (MONITOR MIXES row) — "
        "click FOH / M1… and type a person or instrument (e.g. Jay, Vocals). "
        "Names show on mixer sends and bus masters and are remembered.\n\n"
        "Example: guitar quiet in FOH, loud in Jay's mix; click on Mons only.\n\n"
        "Bus masters on the right set overall level per destination.\n"
        "Full matrix needs a multi-output interface (up to 12 outs). Fewer outs open "
        "only the buses that fit (FOH first).\n\n"
        "PC LISTEN (mixer BUSES strip):\n"
        "- Choose FOH / M1-M5 / Sum - that bus is folded to your PC speakers "
        "so you can audition each player's mix without 12-out hardware.\n"
        "- 'Fold to PC stereo' on = listen mode (default). Off + enough outs = "
        "full matrix to hardware.\n"
        "- Bus meters under the masters show activity on each pair."));
    t.add (topic (
        "mixer-video-strip",
        "VIDEO strip on the mixer",
        "Mixer",
        "video strip stage media play pause stop volume",
        "The purple VIDEO strip controls stage media independently:\n\n"
        "Play / Pause / Stop and volume for stage video or slideshow.\n"
        "When LINK is on, the MAIN transport also drives this player."));

    // ---------- Stage FX ----------
    t.add (topic (
        "stage-fx-controller",
        "Stage FX Controller",
        "Stage FX",
        "stage fx controller media open play loop output display",
        "View / Performance -> Show Stage FX Controller.\n\n"
        "- Open Media - load video/audio (MPEG via FFmpeg) or use set-list pins\n"
        "- Play / Pause / Stop / LOOP - stage transport\n"
        "- Video sound - level (also on mixer VIDEO fader)\n"
        "- Karaoke / Stage FX display pickers and Open/Close\n\n"
        "Video-only MP4s are supported (silent clock + frames on Stage screen)."));

    t.add (topic (
        "stage-fx-loop",
        "Stage media loop",
        "Stage FX",
        "loop loop stage video slideshow",
        "LOOP on the Stage FX Controller restarts media at the end.\n"
        "Works for video clips and slideshows.\n"
        "Off = stop at end."));

    t.add (topic (
        "stage-fx-outputs",
        "Karaoke and Stage FX video screens",
        "Stage FX",
        "karaoke stage output display monitor dual screen",
        "Performance menu or Stage FX Controller:\n\n"
        "- Open Karaoke Video Output - large lyrics for singer/house\n"
        "- Open Stage FX Video Output - stage board / video playback\n"
        "- Move ... to Next Display - cycle monitors\n\n"
        "Screens do not auto-open when starting Performance; assign displays yourself."));

    t.add (topic (
        "stage-fx-mpeg",
        "MPEG / video codec support",
        "Stage FX",
        "mpeg mp4 mov mkv ffmpeg codec video",
        "Stage media uses FFmpeg for MP3, AAC, MP4, MOV, MKV, MPEG, WEBM, AVI, and more.\n"
        "WAV/FLAC/OGG/AIFF use native readers.\n\n"
        "Video frames decode on a background thread (downscaled) so the UI stays responsive."));

    // ---------- Stage Show / Performance ----------
    t.add (topic (
        "performance-setup-live",
        "Performance Setup vs On Stage Live",
        "Performance",
        "performance setup live stage manager nam guitar bass tone go live",
        "Performance is two modes:\n\n"
        "- Setup - build the show: G1 / G2 / Bass NAM-style rack, save tone profiles, "
        "name monitor mixes (whose IEM is which), assign tones to the current setlist song, "
        "open mixer for send levels, dry-run.\n"
        "- Live - stage manager: START / NEXT (or foot pedal) advances the set; "
        "loads stems, mix, three tone profiles, and stage media together.\n\n"
        "In Setup, the MONITOR MIXES row lets you rename FOH and M1–M5 "
        "(e.g. Jay, Vocals, Drums). Names stick and show on the mixer.\n\n"
        "Lyrics and tabs stay off the main performance page. Use Karaoke / Stage FX "
        "outputs for words and visuals.\n\n"
        "Tone profiles live in Documents/JamStudio/Tones/ and are referenced from the setlist."));

    t.add (topic (
        "performance-setlist",
        "Performance set list",
        "Performance",
        "set list setlist performance next pedal foot",
        "Performance -> Edit / Start Set List builds an ordered show from .jamstudio projects.\n\n"
        "- Add / remove / reorder songs\n"
        "- Stem mix prefs per song (or lead-guitar+singer defaults)\n"
        "- G1 / G2 / Bass tone profiles per song (Performance Setup rack)\n"
        "- Save set list under Documents/JamStudio/SetLists/\n"
        "- Start Performance -> Setup first, then GO LIVE\n"
        "- START / NEXT or foot pedal between songs\n\n"
        "MIDI Next Song / sustain pedal can trigger the next song."));

    t.add (topic (
        "stage-show-builder",
        "Stage Show Builder",
        "Performance",
        "stage show builder pin video slideshow setlist",
        "Wizard tile Stage Show, or Performance -> Stage Show Builder.\n\n"
        "Pin a video or image slideshow to each set-list song:\n"
        "- Pin video... / Pin slideshow... / Clear stage media\n"
        "- Save packages media into <SetName>.media/ next to the .setlist file\n"
        "- When the song plays, stage media loads and auto-plays by default\n\n"
        "Control playback from Stage FX Controller or mixer VIDEO / linked MAIN transport."));

    t.add (topic (
        "performance-next",
        "Next song / foot pedal",
        "Performance",
        "next song foot pedal midi cc64 performance",
        "Performance -> Next Song / Start (Foot Pedal) advances the set.\n"
        "Between songs the app waits for the next trigger.\n"
        "Map MIDI Next Song in Help -> MIDI Control Surface."));

    // ---------- Floating windows ----------
    t.add (topic (
        "dock-sticky",
        "Sticky Mixer + Stage FX windows",
        "Floating Windows",
        "dock sticky stick unstick attach detach floating",
        "Mixer and Stage FX Controller can stick together:\n\n"
        "Title bar near X:\n"
        "- ><  Unstick (move independently)\n"
        "- <>  Stick together again\n\n"
        "When sticky, the mixer is the parent: move either window and the pair follows.\n"
        "Defaults: sticky ON, Stage FX docked to the RIGHT, 4 px gap."));

    t.add (topic (
        "dock-menu",
        "Floating Window Dock menu settings",
        "Floating Windows",
        "dock side gap auto stick maximise menu view",
        "View -> Floating Window Dock:\n\n"
        "- Stick / Unstick\n"
        "- Dock side: Right, Left, Top, Bottom\n"
        "- Gap: Tight (0), Normal (4), Wide (12)\n"
        "- Auto-Stick When Edges Touch\n"
        "- Unstick When Mixer Maximised\n\n"
        "Settings save to JamStudio app data (floating-dock.json)."));

    // ---------- Notation ----------
    t.add (topic (
        "notation-tabs",
        "Tabs and sheet music",
        "Notation",
        "tabs tab sheet musicxml score notation",
        "Notation menu:\n\n"
        "- Browse Tab Library / Import MusicXML\n"
        "- AI Tab Transcription (Basic Pitch) when tools are set up\n"
        "- Tab View / Sheet View\n"
        "- Full Page Tabs for printing\n"
        "- Show Tabs Panel toggles the workspace panel\n\n"
        "Part selector chooses which score part is active (e.g. Guitar)."));

    // ---------- Lyrics ----------
    t.add (topic (
        "lyrics",
        "Lyrics panel and karaoke",
        "Lyrics",
        "lyrics lrc whisper karaoke synced",
        "Lyrics menu:\n\n"
        "- Find Synced Lyrics Online\n"
        "- Import LRC\n"
        "- AI Vocal Transcription (Whisper)\n"
        "- Full Page Lyrics\n"
        "- Show Lyrics Panel\n\n"
        "Karaoke video output shows large current/previous/next lines synced to transport."));

    // ---------- Stems ----------
    t.add (topic (
        "stems-separate",
        "Separate stems (Demucs)",
        "Stems",
        "stems demucs separate guitar drums bass vocals",
        "Stems -> Separate Stems runs Demucs (when installed via AI Tools Setup).\n"
        "Produces guitar/drums/bass/vocals (and more) for independent mixer levels.\n"
        "Stem lanes under the transport show mini waveforms."));

    // ---------- Recording ----------
    t.add (topic (
        "recording-mode",
        "Recording mode + Ardour Studio (Linux)",
        "Recording",
        "recording mode ardour studio companion handoff stems import take linux",
        "On Linux, Recording mode is a seamless Ardour companion.\n\n"
        "Transport -> Open Studio (Ardour)...  or  Recording panel -> Open Studio:\n\n"
        "1. JamStudio exports stems + mix bounce into a session pack\n"
        "   (Documents/JamStudio/ArdourSessions/.../interop/)\n"
        "2. Writes jamstudio-bridge.json (sample rate, devices, inputs)\n"
        "3. Releases the audio interface so Ardour can use it\n"
        "4. Launches Ardour and opens the interop folder\n"
        "5. In Ardour: New Session at that sample rate -> Import all interop WAVs\n"
        "6. Record with plugins / multi-track as usual\n"
        "7. Export WAV -> JamStudio Transport -> Import Take...\n\n"
        "Install Ardour:  sudo apt install ardour\n\n"
        "Also available: Open External Recorder (Audacity...) for a lighter path.\n"
        "Internal Record / Stop still works inside JamStudio.\n"
        "Packs live under Documents/JamStudio/ArdourSessions/."));

    // ---------- MIDI ----------
    t.add (topic (
        "midi-control",
        "MIDI control surface",
        "MIDI",
        "midi control surface mapping cc pedal next",
        "Help -> MIDI Control Surface...\n\n"
        "Enable MIDI input, pick a device, map CCs/notes to:\n"
        "stem volumes, mute/solo, transport, metronome, record, next song, etc.\n\n"
        "Profiles can be saved. Foot-pedal next song is ideal for Performance mode."));

    // ---------- AI ----------
    t.add (topic (
        "ai-tools",
        "AI Tools Setup",
        "AI Tools",
        "ai demucs whisper basic pitch python tools",
        "Help -> AI Tools Setup... checks optional Python tools:\n\n"
        "- Demucs - stem separation\n"
        "- Whisper - lyrics from vocals\n"
        "- Basic Pitch - tab/MIDI transcription\n\n"
        "Install missing tools as guided; availability is shown in related menus."));

    // ---------- About ----------
    t.add (topic (
        "about",
        "About JamStudio",
        "Help",
        "about version jamstudio",
        "JamStudio is a guitar practice and performance workstation:\n"
        "stems, tabs, lyrics, multi-bus mixer, stage video, and live set lists.\n\n"
        "Open Help -> Instructions... anytime to search this guide."));

    return t;
}
} // namespace

const juce::Array<HelpTopic>& HelpCatalog::getAllTopics()
{
    static const juce::Array<HelpTopic> topics = buildTopics();
    return topics;
}

juce::StringArray HelpCatalog::getCategories()
{
    juce::StringArray cats;
    for (const auto& t : getAllTopics())
        cats.addIfNotAlreadyThere (t.category);
    cats.sort (true);
    return cats;
}

juce::Array<HelpTopic> HelpCatalog::search (const juce::String& query, const juce::String& categoryFilter)
{
    juce::Array<HelpTopic> out;
    for (const auto& t : getAllTopics())
    {
        if (categoryFilter.isNotEmpty() && t.category != categoryFilter)
            continue;
        if (t.matches (query))
            out.add (t);
    }
    return out;
}

const HelpTopic* HelpCatalog::findById (const juce::String& id)
{
    for (const auto& t : getAllTopics())
        if (t.id == id)
            return &t;
    return nullptr;
}

} // namespace jamstudio::ui
