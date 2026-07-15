#include "ArdourCompanion.h"

#include <cstdlib>

namespace jamstudio::audio
{

namespace
{
bool commandOnPath (const juce::String& command)
{
    juce::ChildProcess process;
    juce::StringArray args;
    args.add ("/bin/sh");
    args.add ("-c");
    args.add ("command -v " + command);
    if (! process.start (args, juce::ChildProcess::wantStdOut))
        return false;
    process.waitForProcessToFinish (3000);
    return process.getExitCode() == 0;
}

juce::File resolveOnPath (const juce::String& command)
{
    juce::ChildProcess process;
    juce::StringArray args;
    args.add ("/bin/sh");
    args.add ("-c");
    args.add ("command -v " + command);
    if (! process.start (args, juce::ChildProcess::wantStdOut))
        return {};
    process.waitForProcessToFinish (3000);
    if (process.getExitCode() != 0)
        return {};
    auto path = process.readAllProcessOutput().trim().upToFirstOccurrenceOf ("\n", false, false).trim();
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

juce::String sanitizeSessionName (juce::String name)
{
    name = name.trim();
    if (name.isEmpty())
        name = "JamStudio-Session";
    // Ardour-friendly folder name
    const juce::String banned ("/\\:*?\"<>| ");
    for (int i = 0; i < banned.length(); ++i)
        name = name.replaceCharacter (banned[i], '-');
    while (name.contains ("--"))
        name = name.replace ("--", "-");
    return name.substring (0, 64);
}

bool writeStemWav (const StemTrack& stem,
                   juce::AudioFormatManager& formats,
                   const juce::File& dest,
                   const double sampleRate,
                   juce::String& error)
{
    juce::ignoreUnused (formats);
    const auto* constReader = stem.getReader();
    if (constReader == nullptr)
    {
        error = "Stem has no audio reader.";
        return false;
    }
    // AudioFormatReader::read is non-const on some JUCE versions
    auto* reader = const_cast<juce::AudioFormatReader*> (constReader);

    dest.getParentDirectory().createDirectory();
    dest.deleteFile();
    std::unique_ptr<juce::OutputStream> out (dest.createOutputStream());
    if (out == nullptr)
    {
        error = "Could not create " + dest.getFileName();
        return false;
    }

    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate > 0.0 ? sampleRate : reader->sampleRate)
                             .withNumChannels (channels)
                             .withBitsPerSample (24);

    auto writer = wav.createWriterFor (out, options);
    if (writer == nullptr)
    {
        error = "Could not open WAV writer for " + dest.getFileName();
        return false;
    }

    // Offline copy via AudioFormatReader::read
    const int block = 4096;
    juce::AudioBuffer<float> buffer (channels, block);
    juce::int64 pos = 0;
    const auto total = reader->lengthInSamples;

    while (pos < total)
    {
        const auto n = (int) juce::jmin ((juce::int64) block, total - pos);
        buffer.clear();
        reader->read (&buffer, 0, n, pos, true, channels > 1);
        writer->writeFromAudioSampleBuffer (buffer, 0, n);
        pos += n;
    }

    writer.reset();
    return dest.existsAsFile() && dest.getSize() > 64;
}
} // namespace

//==============================================================================
ExternalRecorderApp ArdourCompanion::detectArdour()
{
    ExternalRecorderApp app;
    app.name = "Ardour";
    app.acceptsFileArgument = true;

   #if ! JUCE_LINUX
    return app; // empty exe — not available path for now
   #else
    const juce::String commands[] = {
        "ardour8", "ardour7", "ardour6", "ardour", "Ardour"
    };
    for (const auto& cmd : commands)
    {
        if (! commandOnPath (cmd))
            continue;
        auto exe = resolveOnPath (cmd);
        if (exe.existsAsFile())
        {
            app.executable = exe;
            app.name = "Ardour";
            return app;
        }
    }

    const juce::File candidates[] = {
        juce::File ("/usr/bin/ardour8"),
        juce::File ("/usr/bin/ardour7"),
        juce::File ("/usr/bin/ardour"),
        juce::File ("/usr/local/bin/ardour8"),
        juce::File ("/usr/local/bin/ardour"),
        juce::File ("/opt/Ardour-8/bin/ardour8"),
        juce::File ("/opt/Ardour-7/bin/ardour7"),
        juce::File ("/var/lib/flatpak/exports/bin/org.ardour.Ardour"),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile (".local/share/flatpak/exports/bin/org.ardour.Ardour"),
    };

    for (const auto& f : candidates)
    {
        if (f.existsAsFile() || f.exists()) // flatpak export can be a script
        {
            app.executable = f;
            return app;
        }
    }

    // flatpak run without export
    if (commandOnPath ("flatpak"))
    {
        juce::ChildProcess p;
        if (p.start ("/bin/sh -c \"flatpak info org.ardour.Ardour >/dev/null 2>&1\"")
            && p.waitForProcessToFinish (4000) == 0)
        {
            app.executable = juce::File ("/usr/bin/flatpak"); // special-cased in launch
            app.name = "Ardour (Flatpak)";
            return app;
        }
    }

    return app;
   #endif
}

juce::File ArdourCompanion::makeSessionPackRoot (const juce::String& songTitle)
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    const auto name = sanitizeSessionName (songTitle) + "-" + stamp;
    auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("JamStudio")
                    .getChildFile ("ArdourSessions")
                    .getChildFile (name);
    root.createDirectory();
    root.getChildFile ("interop").createDirectory();
    root.getChildFile ("export").createDirectory();
    return root;
}

void ArdourCompanion::writeBridgeFiles (const juce::File& packDir,
                                        const ArdourHandoffContext& context,
                                        const juce::StringArray& audioFileNames,
                                        const juce::File& mixBounce)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "jamstudio-ardour-bridge/1");
    o->setProperty ("created", juce::Time::getCurrentTime().toISO8601 (true));
    o->setProperty ("songTitle", context.songTitle);
    o->setProperty ("songFile", context.songFile.getFullPathName());
    o->setProperty ("sampleRate", context.sampleRate);
    o->setProperty ("bufferSize", context.bufferSize);
    o->setProperty ("inputChannels", context.inputChannels);
    o->setProperty ("outputChannels", context.outputChannels);
    o->setProperty ("inputDeviceName", context.inputDeviceName);
    o->setProperty ("outputDeviceName", context.outputDeviceName);
    o->setProperty ("deviceTypeName", context.deviceTypeName);
    o->setProperty ("tempoBpm", context.tempoBpm);
    o->setProperty ("mixBounce", mixBounce.getFileName());
    o->setProperty ("interopDir", "interop");
    o->setProperty ("platform", "linux");

    juce::Array<juce::var> files;
    for (const auto& n : audioFileNames)
        files.add (n);
    o->setProperty ("audioFiles", files);

    o->setProperty ("instructions",
                    juce::StringArray {
                        "1. JamStudio released the audio interface so Ardour can use it.",
                        "2. In Ardour: create or open a session at THIS sample rate: "
                            + juce::String (context.sampleRate, 0) + " Hz.",
                        "3. Session → Import → select all WAVs in the interop/ folder (or drag them in).",
                        "4. Add empty audio tracks for recording; assign inputs from your interface.",
                        "5. Record with plugins. Export WAV takes to export/ or Documents.",
                        "6. Back in JamStudio: Transport → Import Take from File…",
                    }.joinIntoString ("\n"));

    const auto bridge = packDir.getChildFile ("jamstudio-bridge.json");
    bridge.replaceWithText (juce::JSON::toString (juce::var (o), true));

    juce::String readme;
    readme << "JamStudio → Ardour companion pack\n"
           << "=================================\n\n"
           << "Song: " << context.songTitle << "\n"
           << "Sample rate: " << juce::String (context.sampleRate, 0) << " Hz\n"
           << "Buffer (JamStudio): " << context.bufferSize << "\n"
           << "Input device: " << context.inputDeviceName << " (" << context.inputChannels << " ch)\n"
           << "Output device: " << context.outputDeviceName << " (" << context.outputChannels << " ch)\n"
           << "Backend type: " << context.deviceTypeName << "\n"
           << "Tempo hint: " << juce::String (context.tempoBpm, 1) << " BPM\n\n"
           << "Audio files are in: interop/\n";
    if (mixBounce.existsAsFile())
        readme << "  Mix bounce: " << mixBounce.getFileName() << "\n";
    for (const auto& n : audioFileNames)
        readme << "  - " << n << "\n";
    readme << "\n"
           << "Quick start in Ardour\n"
           << "---------------------\n"
           << "1. Session → New…  (or open an existing template)\n"
           << "   • Use sample rate " << juce::String (context.sampleRate, 0) << " Hz\n"
           << "   • Audio system: ALSA or JACK/PipeWire (same as JamStudio when possible)\n"
           << "2. Session → Import… → choose all files in interop/\n"
           << "3. Create record-armed tracks for your mics/DI (Inputs 1..N)\n"
           << "4. Record. Export finished takes as WAV.\n"
           << "5. In JamStudio: Transport → Import Take from File…\n\n"
           << "JamStudio closed its audio device so Ardour can take the interface.\n"
           << "When you return to JamStudio, re-open Audio Interface if needed.\n";

    packDir.getChildFile ("README-ARDOUR.txt").replaceWithText (readme);

    // Convenience launcher (re-open pack folder + Ardour)
    juce::String sh;
    sh << "#!/bin/sh\n"
       << "cd \"$(dirname \"$0\")\" || exit 1\n"
       << "xdg-open \"interop\" 2>/dev/null &\n"
       << "if command -v ardour8 >/dev/null 2>&1; then exec ardour8 \"$@\"; fi\n"
       << "if command -v ardour7 >/dev/null 2>&1; then exec ardour7 \"$@\"; fi\n"
       << "if command -v ardour >/dev/null 2>&1; then exec ardour \"$@\"; fi\n"
       << "if flatpak info org.ardour.Ardour >/dev/null 2>&1; then exec flatpak run org.ardour.Ardour \"$@\"; fi\n"
       << "echo \"Ardour not found\"; exit 1\n";
    const auto script = packDir.getChildFile ("open-ardour.sh");
    script.replaceWithText (sh);
    script.setExecutePermission (true);
}

bool ArdourCompanion::exportStemFiles (StemMixer& mixer,
                                       juce::AudioFormatManager& formats,
                                       const juce::File& interopDir,
                                       juce::StringArray& outNames,
                                       juce::String& error,
                                       const double sampleRate)
{
    outNames.clear();
    interopDir.createDirectory();

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        const auto* stem = mixer.getStem (i);
        if (stem == nullptr || ! stem->isLoaded())
            continue;

        auto base = stem->getName().isNotEmpty() ? stem->getName() : juce::String ("Stem") + juce::String (i + 1);
        base = sanitizeSessionName (base);
        const auto dest = interopDir.getChildFile (
            juce::String::formatted ("%02d-", i + 1) + base + ".wav");

        juce::String err;
        if (! writeStemWav (*stem, formats, dest, sampleRate, err))
        {
            error = err;
            return false;
        }
        outNames.add (dest.getFileName());
    }

    return true;
}

bool ArdourCompanion::launchArdour (const juce::File& executable,
                                    const juce::File& packDir,
                                    juce::String& error)
{
    if (! executable.exists() && executable.getFileName() != "flatpak")
    {
        // flatpak path uses /usr/bin/flatpak
        if (! juce::File ("/usr/bin/flatpak").existsAsFile()
            && executable.getFullPathName() != "/usr/bin/flatpak")
        {
            error = "Ardour executable missing.";
            return false;
        }
    }

    // Open interop folder for drag-import convenience
    juce::Process::openDocument (packDir.getChildFile ("interop").getFullPathName(), {});

    juce::String cmd;
    if (executable.getFileName() == "flatpak"
        || executable.getFullPathName().contains ("flatpak"))
    {
        cmd = "flatpak run org.ardour.Ardour >/dev/null 2>&1 &";
    }
    else
    {
        cmd = "\"" + executable.getFullPathName() + "\" >/dev/null 2>&1 &";
    }

    // Detached so JamStudio does not own Ardour
    if (std::system (cmd.toRawUTF8()) != 0)
    {
        if (! juce::Process::openDocument (executable.getFullPathName(), {}))
        {
            error = "Could not launch Ardour. Is it installed? (sudo apt install ardour)";
            return false;
        }
    }

    return true;
}

ArdourHandoffResult ArdourCompanion::openStudio (StemMixer& mixer,
                                                 juce::AudioFormatManager& formats,
                                                 const ArdourHandoffContext& context,
                                                 const std::function<void()>& releaseAudioDevice)
{
    ArdourHandoffResult result;

   #if ! JUCE_LINUX
    result.message = "Ardour companion is currently supported on Linux.";
    return result;
   #else

    auto ardour = detectArdour();
    if (! ardour.executable.exists()
        && ardour.executable.getFullPathName() != "/usr/bin/flatpak"
        && ardour.name != "Ardour (Flatpak)")
    {
        // detect may set flatpak special case
        if (! isAvailable())
        {
            result.message =
                "Ardour not found. Install with:\n\n"
                "  sudo apt install ardour\n\n"
                "or from https://ardour.org — then try Open Studio again.";
            return result;
        }
        ardour = detectArdour();
    }

    if (! isAvailable() && ! ardour.executable.exists()
        && ardour.name != "Ardour (Flatpak)")
    {
        result.message = "Ardour not found. Install: sudo apt install ardour";
        return result;
    }

    result.executable = ardour.executable.exists() ? ardour.executable
                                                   : juce::File ("/usr/bin/flatpak");

    const auto pack = makeSessionPackRoot (context.songTitle);
    result.sessionPackDir = pack;
    const auto interop = pack.getChildFile ("interop");
    interop.createDirectory();

    juce::String error;
    juce::StringArray stemNames;

    // Export stems if present
    if (mixer.getNumStems() > 0)
    {
        if (! exportStemFiles (mixer, formats, interop, stemNames, error, context.sampleRate))
        {
            result.message = "Stem export failed: " + error;
            return result;
        }

        // Mix bounce for a single "backing" track
        const auto bounce = interop.getChildFile ("00-mix-bounce.wav");
        if (ExternalRecorder::bounceMixToWav (mixer, bounce, error, context.sampleRate))
        {
            result.mixBounceFile = bounce;
            if (! stemNames.contains (bounce.getFileName()))
                stemNames.insert (0, bounce.getFileName());
        }
    }

    writeBridgeFiles (pack, context, stemNames, result.mixBounceFile);

    // Critical: release audio so Ardour can claim ALSA/JACK/PipeWire
    if (releaseAudioDevice)
        releaseAudioDevice();

    juce::Thread::sleep (350); // brief settle for device release

    if (! launchArdour (result.executable, pack, error))
    {
        result.message = error;
        return result;
    }

    result.ok = true;
    result.message =
        "Opened Ardour Studio pack.\n\n"
        "JamStudio released the audio interface.\n"
        "Session pack:\n" + pack.getFullPathName() + "\n\n"
        "In Ardour: New Session @ " + juce::String (context.sampleRate, 0)
        + " Hz → Import all files from interop/.\n"
        "When finished, Export WAV and use Import Take in JamStudio.";
    return result;
   #endif
}

} // namespace jamstudio::audio
