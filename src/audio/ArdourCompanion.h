#pragma once

#include "ExternalRecorder.h"
#include "StemMixer.h"

#include <JuceHeader.h>

namespace jamstudio::audio
{

/** Snapshot of JamStudio audio + song state for an Ardour handoff. */
struct ArdourHandoffContext
{
    juce::String songTitle;
    juce::File songFile;
    double sampleRate = 48000.0;
    int bufferSize = 256;
    int inputChannels = 2;
    int outputChannels = 2;
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    juce::String deviceTypeName;
    double tempoBpm = 120.0;
};

struct ArdourHandoffResult
{
    bool ok = false;
    juce::String message;
    juce::File sessionPackDir;   // pack root (interop + bridge)
    juce::File mixBounceFile;
    juce::File executable;
};

/**
 * Linux-first Ardour companion:
 *  - Detect Ardour (PATH / flatpak / common paths)
 *  - Export stems + mix bounce into a session pack
 *  - Write bridge JSON (device, rate, inputs) for re-import
 *  - Release JamStudio audio device, launch Ardour
 *
 * Seamless setup without embedding Ardour binaries. Full .ardour XML is
 * version-fragile; we ship a ready-to-import pack + launch Ardour so the
 * user lands in a prepared workspace with clear next steps.
 */
class ArdourCompanion
{
public:
    /** Find Ardour executable (prefers ardour8 > ardour7 > ardour > flatpak). */
    [[nodiscard]] static ExternalRecorderApp detectArdour();

    [[nodiscard]] static bool isAvailable()
    {
        const auto a = detectArdour();
        return a.executable.exists() || a.name.containsIgnoreCase ("Flatpak");
    }

    /**
     * Full handoff: bounce/export → pack → close device → launch Ardour.
     * @param releaseAudioDevice called after files are written (stop JUCE device).
     */
    [[nodiscard]] static ArdourHandoffResult openStudio (StemMixer& mixer,
                                                         juce::AudioFormatManager& formats,
                                                         const ArdourHandoffContext& context,
                                                         const std::function<void()>& releaseAudioDevice);

    /** Pack directory for a new session (Documents/JamStudio/ArdourSessions/…). */
    [[nodiscard]] static juce::File makeSessionPackRoot (const juce::String& songTitle);

    /** Write bridge + README into an existing pack (after audio export). */
    static void writeBridgeFiles (const juce::File& packDir,
                                  const ArdourHandoffContext& context,
                                  const juce::StringArray& audioFileNames,
                                  const juce::File& mixBounce);

private:
    [[nodiscard]] static bool exportStemFiles (StemMixer& mixer,
                                               juce::AudioFormatManager& formats,
                                               const juce::File& interopDir,
                                               juce::StringArray& outNames,
                                               juce::String& error,
                                               double sampleRate);

    [[nodiscard]] static bool launchArdour (const juce::File& executable,
                                            const juce::File& packDir,
                                            juce::String& error);
};

} // namespace jamstudio::audio
