#pragma once

#include <JuceHeader.h>

namespace jamstudio::ai
{

[[nodiscard]] bool commandExists (const juce::String& command);

/** Returns true if the executable responds successfully to a probe argument (e.g. --help). */
[[nodiscard]] bool commandResponds (const juce::String& executable, const juce::String& probeArg = "--help");

/** Picks the first candidate that responds to the probe, or an empty string if none work. */
[[nodiscard]] juce::String findWorkingExecutable (const juce::StringArray& candidates,
                                                  const juce::String& probeArg = "--help");

/** Extracts a short human-readable error from subprocess output. */
[[nodiscard]] juce::String extractProcessErrorSummary (const juce::String& output);

} // namespace jamstudio::ai