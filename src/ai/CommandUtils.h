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

/** Parses the latest "NN%" value from tool output (tqdm / demucs style). Returns -1 if none. */
[[nodiscard]] float parsePercentProgress (const juce::String& output);

/** Soft progress that approaches ~0.95 while a long job runs (used when tools don't print %). */
[[nodiscard]] float estimateRunningProgress (double elapsedSeconds, double expectedSeconds = 60.0);

} // namespace jamstudio::ai