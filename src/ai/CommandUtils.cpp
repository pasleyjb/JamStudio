#include "CommandUtils.h"

namespace jamstudio::ai
{

namespace
{
juce::StringArray buildProbeCommand (const juce::String& executable, const juce::String& probeArg)
{
    juce::StringArray command;

    if (executable.contains (" -m "))
    {
        command.addTokens (executable, " ", "\"'");
        command.add (probeArg);
    }
    else
    {
        command.add (executable);
        command.add (probeArg);
    }

    return command;
}
} // namespace

bool commandExists (const juce::String& command)
{
    juce::ChildProcess process;
    juce::StringArray args;

   #if JUCE_WINDOWS
    args.add ("where");
    args.add (command);
   #else
    args.add ("sh");
    args.add ("-c");
    args.add ("command -v " + command);
   #endif

    if (! process.start (args, juce::ChildProcess::wantStdOut))
        return false;

    process.waitForProcessToFinish (5000);
    return process.getExitCode() == 0;
}

bool commandResponds (const juce::String& executable, const juce::String& probeArg)
{
    if (executable.isEmpty())
        return false;

    juce::ChildProcess process;
    const auto command = buildProbeCommand (executable, probeArg);

    if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return false;

    process.waitForProcessToFinish (20000);
    return process.getExitCode() == 0;
}

juce::String findWorkingExecutable (const juce::StringArray& candidates, const juce::String& probeArg)
{
    juce::StringArray expanded;

    for (const auto& candidate : candidates)
    {
        expanded.add (candidate);

        if (! candidate.containsChar ('/') && ! candidate.contains (" -m "))
        {
            const auto localBin = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                .getChildFile (".local")
                .getChildFile ("bin")
                .getChildFile (candidate);

            if (localBin.existsAsFile())
                expanded.add (localBin.getFullPathName());
        }
    }

    for (const auto& candidate : expanded)
    {
        if (commandResponds (candidate, probeArg))
            return candidate;
    }

    return {};
}

juce::String extractProcessErrorSummary (const juce::String& output)
{
    if (output.isEmpty())
        return {};

    const auto lines = juce::StringArray::fromLines (output);
    juce::String lastMeaningful;

    for (int i = lines.size() - 1; i >= 0; --i)
    {
        const auto line = lines[i].trim();

        if (line.isEmpty())
            continue;

        if (line.containsIgnoreCase ("ModuleNotFoundError")
            || line.containsIgnoreCase ("No module named")
            || line.containsIgnoreCase ("command not found")
            || line.containsIgnoreCase ("Error:")
            || line.containsIgnoreCase ("error:")
            || line.startsWithIgnoreCase ("Traceback"))
        {
            lastMeaningful = line;
            break;
        }

        if (lastMeaningful.isEmpty() && ! line.startsWithChar ('['))
            lastMeaningful = line;
    }

    if (lastMeaningful.length() > 180)
        return lastMeaningful.substring (0, 177) + "...";

    return lastMeaningful;
}

} // namespace jamstudio::ai