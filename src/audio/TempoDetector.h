#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/** Estimates song tempo from audio onset periodicity. */
class TempoDetector
{
public:
    [[nodiscard]] static bool detectFromFile (const juce::File& audioFile,
                                              juce::AudioFormatManager& formatManager,
                                              double& bpmOut);
};

} // namespace jamstudio::audio