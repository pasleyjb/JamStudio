#include "CommandUtils.h"

#include <cmath>

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

float parsePercentProgress (const juce::String& output)
{
    if (output.isEmpty())
        return -1.0f;

    auto lines = juce::StringArray::fromLines (output);
    float best = -1.0f;

    // tqdm rewrites the same line with \r — split those too.
    juce::StringArray tokens;

    for (const auto& line : lines)
        tokens.addTokens (line, "\r", "");

    for (const auto& token : tokens)
    {
        const auto trimmed = token.trim();
        const auto percentIndex = trimmed.indexOfChar ('%');

        if (percentIndex <= 0)
            continue;

        int start = percentIndex - 1;

        while (start >= 0
               && (juce::CharacterFunctions::isDigit (trimmed[start])
                   || trimmed[start] == '.'
                   || trimmed[start] == ' '))
            --start;

        const auto value = trimmed.substring (start + 1, percentIndex).trim().getFloatValue();

        if (value >= 0.0f && value <= 100.0f)
            best = juce::jmax (best, value / 100.0f);
    }

    return best;
}

float estimateRunningProgress (const double elapsedSeconds, const double expectedSeconds)
{
    const auto expected = juce::jmax (1.0, expectedSeconds);
    // 1 - e^(-t/T) approaches 1 asymptotically; keep UI moving without claiming 100%.
    const auto fraction = 1.0 - std::exp (-elapsedSeconds / expected);
    return static_cast<float> (juce::jlimit (0.05, 0.95, 0.05 + 0.9 * fraction));
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