#include "ExternalRecorder.h"

#include <cstdlib>

namespace jamstudio::audio
{

namespace
{
bool commandOnPath (const juce::String& command)
{
    juce::ChildProcess process;
    juce::StringArray args;
   #if JUCE_WINDOWS
    args.add ("where");
    args.add (command);
   #elif JUCE_MAC
    args.add ("/bin/sh");
    args.add ("-c");
    args.add ("command -v " + command);
   #else
    args.add ("/bin/sh");
    args.add ("-c");
    args.add ("command -v " + command);
   #endif

    if (! process.start (args, juce::ChildProcess::wantStdOut))
        return false;

    process.waitForProcessToFinish (3000);
    return process.getExitCode() == 0;
}

juce::File resolveOnPath (const juce::String& command)
{
    juce::ChildProcess process;
    juce::StringArray args;
   #if JUCE_WINDOWS
    args.add ("where");
    args.add (command);
   #else
    args.add ("/bin/sh");
    args.add ("-c");
    args.add ("command -v " + command);
   #endif

    if (! process.start (args, juce::ChildProcess::wantStdOut))
        return {};

    process.waitForProcessToFinish (3000);
    if (process.getExitCode() != 0)
        return {};

    auto path = process.readAllProcessOutput().trim().upToFirstOccurrenceOf ("\n", false, false).trim();
    if (path.isEmpty())
        return {};

    return juce::File (path);
}

void addIfExists (juce::Array<ExternalRecorderApp>& list,
                  const juce::String& name,
                  const juce::File& exe,
                  const bool acceptsFile = true)
{
    if (! exe.existsAsFile())
        return;

    for (const auto& existing : list)
        if (existing.executable == exe || existing.name == name)
            return;

    ExternalRecorderApp app;
    app.name = name;
    app.executable = exe;
    app.acceptsFileArgument = acceptsFile;
    list.add (app);
}

void addIfOnPath (juce::Array<ExternalRecorderApp>& list,
                  const juce::String& name,
                  const juce::String& command,
                  const bool acceptsFile = true)
{
    if (! commandOnPath (command))
        return;

    auto exe = resolveOnPath (command);
    if (exe.existsAsFile())
        addIfExists (list, name, exe, acceptsFile);
}
} // namespace

juce::Array<ExternalRecorderApp> ExternalRecorder::detectInstalled()
{
    juce::Array<ExternalRecorderApp> list;

    // Prefer Audacity, then common DAWs.
    addIfOnPath (list, "Audacity", "audacity");
    addIfOnPath (list, "Audacity", "audacity-3");

   #if JUCE_LINUX
    addIfExists (list, "Audacity", juce::File ("/usr/bin/audacity"));
    addIfExists (list, "Audacity", juce::File ("/usr/local/bin/audacity"));
    addIfExists (list, "Audacity", juce::File ("/snap/bin/audacity"));
    addIfExists (list, "Audacity", juce::File ("/var/lib/flatpak/exports/bin/org.audacityteam.Audacity"));
    addIfOnPath (list, "Ardour", "ardour");
    addIfOnPath (list, "Ardour", "ardour8");
    addIfOnPath (list, "Ardour", "ardour7");
    addIfOnPath (list, "Reaper", "reaper");
    addIfOnPath (list, "Qtractor", "qtractor");
    addIfOnPath (list, "LMMS", "lmms", false);
    addIfOnPath (list, "Ocenaudio", "ocenaudio");
   #elif JUCE_MAC
    addIfExists (list, "Audacity", juce::File ("/Applications/Audacity.app"));
    addIfExists (list, "Reaper", juce::File ("/Applications/REAPER.app"));
    addIfExists (list, "Logic Pro", juce::File ("/Applications/Logic Pro.app"), false);
    addIfExists (list, "GarageBand", juce::File ("/Applications/GarageBand.app"), false);
    addIfOnPath (list, "Audacity", "audacity");
   #elif JUCE_WINDOWS
    addIfOnPath (list, "Audacity", "audacity.exe");
    const auto pf = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);
    addIfExists (list, "Audacity", pf.getChildFile ("Audacity/Audacity.exe"));
    addIfExists (list, "Reaper", pf.getChildFile ("REAPER (x64)/reaper.exe"));
    addIfExists (list, "Reaper", pf.getChildFile ("REAPER/reaper.exe"));
    addIfExists (list, "Ocenaudio", pf.getChildFile ("ocenaudio/ocenaudio.exe"));
   #endif

    return list;
}

ExternalRecorderApp ExternalRecorder::getPreferred()
{
    const auto apps = detectInstalled();
    if (apps.isEmpty())
        return {};

    // Prefer Audacity by name.
    for (const auto& a : apps)
        if (a.name.containsIgnoreCase ("Audacity"))
            return a;

    return apps.getFirst();
}

bool ExternalRecorder::launch (const ExternalRecorderApp& app,
                               const juce::File& mediaFile,
                               juce::String& errorMessage)
{
    if (app.name.isEmpty() || ! app.executable.exists())
    {
        errorMessage = "No external recorder found. Install Audacity (recommended), Reaper, or Ardour.";
        return false;
    }

    const auto mediaPath = (mediaFile.existsAsFile() && app.acceptsFileArgument)
                               ? mediaFile.getFullPathName()
                               : juce::String();

   #if JUCE_MAC
    if (app.executable.getFullPathName().endsWithIgnoreCase (".app"))
    {
        juce::StringArray args;
        args.add ("/usr/bin/open");
        args.add ("-a");
        args.add (app.executable.getFullPathName());
        if (mediaPath.isNotEmpty())
            args.add (mediaPath);

        juce::ChildProcess proc;
        if (! proc.start (args))
        {
            errorMessage = "Could not launch " + app.name + ".";
            return false;
        }

        return true;
    }
   #endif

    // Detached shell launch so JamStudio does not own/kill the DAW process.
    juce::String cmd;
   #if JUCE_WINDOWS
    cmd = "start \"\" \"" + app.executable.getFullPathName() + "\"";
    if (mediaPath.isNotEmpty())
        cmd += " \"" + mediaPath + "\"";
    if (std::system (cmd.toRawUTF8()) != 0)
    {
        errorMessage = "Could not launch " + app.name + ".";
        return false;
    }
   #else
    cmd = "\"" + app.executable.getFullPathName() + "\"";
    if (mediaPath.isNotEmpty())
        cmd += " \"" + mediaPath + "\"";
    cmd += " >/dev/null 2>&1 &";
    if (std::system (cmd.toRawUTF8()) != 0)
    {
        // system() may return non-zero even when background launch works; still try openDocument.
        if (! juce::Process::openDocument (app.executable.getFullPathName(), mediaPath))
        {
            errorMessage = "Could not launch " + app.name + ".";
            return false;
        }
    }
   #endif

    return true;
}

bool ExternalRecorder::bounceMixToWav (StemMixer& mixer,
                                       const juce::File& destinationWav,
                                       juce::String& errorMessage,
                                       const double sampleRate)
{
    if (mixer.getNumStems() == 0)
    {
        errorMessage = "Nothing to bounce — load a backing track or project first.";
        return false;
    }

    const auto length = mixer.getLengthInSeconds();
    if (length <= 0.05)
    {
        errorMessage = "Backing track has no length to export.";
        return false;
    }

    destinationWav.getParentDirectory().createDirectory();
    destinationWav.deleteFile();

    std::unique_ptr<juce::OutputStream> outStream (destinationWav.createOutputStream());
    if (outStream == nullptr)
    {
        errorMessage = "Could not create bounce file.";
        return false;
    }

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withNumChannels (2)
                             .withBitsPerSample (16);

    auto writer = wav.createWriterFor (outStream, options);
    if (writer == nullptr)
    {
        errorMessage = "Could not create WAV writer.";
        return false;
    }

    const int block = 2048;
    const auto wasPlaying = mixer.isPlaying();
    const auto savedPos = mixer.getPosition();

    mixer.prepareToPlay (block, sampleRate);
    mixer.setPosition (0.0);
    mixer.play();

    juce::AudioBuffer<float> buffer (2, block);
    double rendered = 0.0;

    while (rendered < length)
    {
        buffer.clear();
        juce::AudioSourceChannelInfo info (&buffer, 0, block);
        mixer.getNextAudioBlock (info);

        const auto frames = juce::jmin (block, static_cast<int> ((length - rendered) * sampleRate) + 1);
        if (frames <= 0)
            break;

        writer->writeFromAudioSampleBuffer (buffer, 0, frames);
        rendered += static_cast<double> (frames) / sampleRate;

        if (! mixer.isPlaying() && rendered < length - 0.05)
            mixer.play(); // if mixer auto-stopped at end mid-loop
    }

    writer.reset(); // flush

    mixer.pause();
    mixer.setPosition (savedPos);
    if (wasPlaying)
        mixer.play();

    if (! destinationWav.existsAsFile() || destinationWav.getSize() < 128)
    {
        errorMessage = "Bounce produced an empty file.";
        return false;
    }

    return true;
}

} // namespace jamstudio::audio
