#pragma once

#include "StemMixer.h"
#include <JuceHeader.h>

namespace jamstudio::audio
{

/** A detected third-party DAW / recorder (Audacity, Reaper, etc.). */
struct ExternalRecorderApp
{
    juce::String name;
    juce::File executable;
    /** True if the app accepts a media file path as a launch argument. */
    bool acceptsFileArgument = true;
};

/**
 * Finds installed recording apps and can open them with an optional WAV.
 * Prefer external DAWs for amp sims / plugins; JamStudio remains the player + organizer.
 */
class ExternalRecorder
{
public:
    /** Scan PATH / common install locations for known apps. */
    [[nodiscard]] static juce::Array<ExternalRecorderApp> detectInstalled();

    /** Preferred app (Audacity first, then others). Null name if none. */
    [[nodiscard]] static ExternalRecorderApp getPreferred();

    /** Launch app; if mediaFile is valid and acceptsFileArgument, pass it on the command line. */
    [[nodiscard]] static bool launch (const ExternalRecorderApp& app,
                                      const juce::File& mediaFile,
                                      juce::String& errorMessage);

    /**
     * Offline bounce of stem mix (FOH pair) to a 16-bit stereo WAV.
     * Transport should be stopped by the caller for a clean render.
     */
    [[nodiscard]] static bool bounceMixToWav (StemMixer& mixer,
                                              const juce::File& destinationWav,
                                              juce::String& errorMessage,
                                              double sampleRate = 44100.0);
};

} // namespace jamstudio::audio
