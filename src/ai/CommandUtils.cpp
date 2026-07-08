#include "CommandUtils.h"

namespace jamstudio::ai
{

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

} // namespace jamstudio::ai