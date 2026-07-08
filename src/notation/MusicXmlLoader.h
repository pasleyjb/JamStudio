#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

/** Loads MusicXML text from plain XML or compressed MXL files. */
class MusicXmlLoader
{
public:
    [[nodiscard]] static bool loadScoreXmlFromFile (const juce::File& file,
                                                    juce::String& xmlText,
                                                    juce::String& errorMessage);

private:
    [[nodiscard]] static bool extractFromMxl (const juce::File& file,
                                              juce::String& xmlText,
                                              juce::String& errorMessage);
};

} // namespace jamstudio::notation